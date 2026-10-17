/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

// Regression tests for the locking bugs fixed in the BYD client and poller.
// Both used to hang forever instead of returning, so every call is run with a
// watchdog and the tests assert that it came back in time.
//
// Everything a timed-out call touches (callable, client, buffers, completion
// flag) is heap-allocated and shared with the worker thread, so a hung call
// cannot reach destroyed objects after the test returns.

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "byd_cloud_client.h"
#include "byd_http_client.h"
#include "byd_types.h"
#include "byd_vehicle_poller.h"

namespace {

using namespace std::chrono_literals;

using Supla::Linux::Byd::BydAccountConfig;
using Supla::Linux::Byd::BydCloudClient;
using Supla::Linux::Byd::BydHttpTransport;
using Supla::Linux::Byd::BydVehicleConfig;
using Supla::Linux::Byd::BydVehiclePoller;

// postSecure() refuses to touch the transport until the Bangcle codec has
// loaded its tables, so the tests need the repository copy. ctest runs from the
// build directory, but the binary is also started by hand and by IDE runners
// from other directories, so search upwards instead of hard-coding one
// relative path. Returns an empty string if the repository is not reachable;
// the assertions below then report it.
std::string searchUpwardsForBangcleTables(std::filesystem::path directory) {
  const std::filesystem::path relativePath =
      "extras/porting/linux/byd/data/bangcle_tables.bin";
  std::error_code error;
  while (!directory.empty()) {
    const auto candidate = directory / relativePath;
    if (std::filesystem::exists(candidate, error)) {
      return candidate.string();
    }
    const auto parent = directory.parent_path();
    if (parent == directory) {
      break;
    }
    directory = parent;
  }
  return {};
}

std::string findBangcleTables() {
  std::error_code error;
  const auto fromCwd =
      searchUpwardsForBangcleTables(std::filesystem::current_path(error));
  if (!fromCwd.empty()) {
    return fromCwd;
  }
  // Started from outside the repository: look next to this executable.
  const auto executable =
      std::filesystem::read_symlink("/proc/self/exe", error);
  if (error || executable.empty()) {
    return {};
  }
  return searchUpwardsForBangcleTables(executable.parent_path());
}

BydAccountConfig testAccount() {
  BydAccountConfig account;
  account.username = "concurrency-test@example.com";
  account.password = "secret";
  account.baseUrl = "https://example.invalid";
  account.bangcleTablesPath = findBangcleTables();
  return account;
}

BydVehicleConfig testVehicle(const std::string& vin) {
  BydVehicleConfig vehicle;
  vehicle.vin = vin;
  vehicle.pollIntervalSec = 5;
  return vehicle;
}

// Transport double. It counts calls and can park the calling thread until the
// test releases it, which is how the poller tests get the worker to sit inside
// pollOnce() at a known point.
class FakeHttpTransport : public BydHttpTransport {
 public:
  bool postJson(const std::string& /*url*/,
                const std::string& /*body*/,
                const std::string& /*userAgent*/,
                int* statusCode,
                std::string* responseBody,
                std::string* error) override {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      calls_++;
      entered_ = true;
    }
    enteredCv_.notify_all();

    if (blockCalls_.load()) {
      std::unique_lock<std::mutex> gate(gateMutex_);
      gateCv_.wait(gate, [this] { return gateOpen_.load(); });
    }

    if (statusCode != nullptr) {
      *statusCode = 200;
    }
    if (responseBody != nullptr) {
      // No "response" envelope, so the client fails this request quickly
      // without needing a real login handshake.
      *responseBody = "{\"code\":\"0\"}";
    }
    if (error != nullptr) {
      error->clear();
    }
    return true;
  }

  void blockCalls() {
    blockCalls_.store(true);
  }

  int calls() const {
    return calls_;
  }

  bool waitForFirstCall(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    return enteredCv_.wait_for(lock, timeout, [this] { return entered_; });
  }

  void openGate() {
    gateOpen_.store(true);
    gateCv_.notify_all();
  }

 private:
  mutable std::mutex mutex_;
  std::condition_variable enteredCv_;
  bool entered_ = false;
  int calls_ = 0;

  std::atomic<bool> blockCalls_{false};
  std::mutex gateMutex_;
  std::condition_variable gateCv_;
  std::atomic<bool> gateOpen_{false};
};

enum class WatchdogResult { Completed, TimedOut };

// Runs fn() on its own thread and reports whether it finished in time. The
// callable, its result and the completion flag live in a heap block owned by
// the worker, so a timed-out call that keeps running after the test returns
// cannot touch destroyed memory. A hung call is detached so the watchdog itself
// cannot block.
WatchdogResult runWithTimeout(std::function<bool()> fn,
                              std::chrono::milliseconds timeout,
                              bool* result) {
  struct State {
    std::function<bool()> fn;
    std::atomic<bool> done{false};
    bool result = false;
  };
  auto state = std::make_shared<State>();
  state->fn = std::move(fn);

  std::thread worker([state] {
    state->result = state->fn();
    state->done.store(true);
  });

  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (!state->done.load() && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(10ms);
  }

  if (!state->done.load()) {
    worker.detach();
    return WatchdogResult::TimedOut;
  }
  worker.join();
  *result = state->result;
  return WatchdogResult::Completed;
}

// Owns everything a fetch call touches, and is captured by value in the worker
// lambda so it outlives a timed-out test.
struct FetchCall {
  std::shared_ptr<BydCloudClient> client;
  nlohmann::json out;
  std::string error;

  bool fetchRealtime(const BydVehicleConfig& vehicle) {
    return client->fetchRealtime(vehicle, &out, &error);
  }

  bool fetchChargingHomepage(const std::string& vin) {
    return client->fetchChargingHomepage(vin, &out, &error);
  }
};

// Same for the poller test, including the hook synchronisation.
struct UnsubscribeCall {
  std::shared_ptr<BydVehiclePoller> poller;
  void* owner = nullptr;
  std::atomic<bool> done{false};

  std::mutex hookMutex;
  std::condition_variable hookCv;
  bool lockHeld = false;
};

}  // namespace

// fetch*() locked mutex_ and then called postTokenJson(), which reached
// ensureLoggedIn() - locking the same non-recursive mutex again on the same
// thread. That is undefined behaviour and in practice the call never returned,
// so the poller worker blocked on its first poll and shutdown() hung in join().
TEST(BydConcurrencyTest, FetchReturnsInsteadOfDeadlockingOnClientMutex) {
  auto transport = std::make_unique<FakeHttpTransport>();
  FakeHttpTransport* fake = transport.get();
  auto call = std::make_shared<FetchCall>();
  call->client =
      std::make_shared<BydCloudClient>(testAccount(), std::move(transport));

  bool fetched = true;
  const auto outcome = runWithTimeout(
      [call] {
        return call->fetchRealtime(testVehicle("TESTVINCONCURRENCY0001"));
      },
      5s,
      &fetched);

  if (outcome == WatchdogResult::TimedOut) {
    FAIL() << "fetchRealtime() deadlocked: ensureLoggedIn() re-locked mutex_";
  }
  // No login response is served, so the fetch reports failure - the point is
  // that it returns at all, and with the request actually sent.
  EXPECT_FALSE(fetched);
  EXPECT_GT(fake->calls(), 0)
      << "transport was never reached; Bangcle tables not found from the "
         "current directory";
}

// Same lock-recursion on another entry point, which also retries once on a
// session-expiry code.
TEST(BydConcurrencyTest, ChargingHomepageFetchReturnsInsteadOfDeadlocking) {
  auto transport = std::make_unique<FakeHttpTransport>();
  FakeHttpTransport* fake = transport.get();
  auto call = std::make_shared<FetchCall>();
  call->client =
      std::make_shared<BydCloudClient>(testAccount(), std::move(transport));

  bool fetched = true;
  const auto outcome = runWithTimeout(
      [call] { return call->fetchChargingHomepage("TESTVINCONCURRENCY0002"); },
      5s,
      &fetched);

  if (outcome == WatchdogResult::TimedOut) {
    FAIL() << "fetchChargingHomepage() deadlocked: ensureLoggedIn() re-locked "
              "mutex_";
  }
  EXPECT_FALSE(fetched);
  EXPECT_GT(fake->calls(), 0)
      << "transport was never reached; Bangcle tables not found from the "
         "current directory";
}

// unsubscribe() erased the subscriber and then joined the worker while still
// holding subscribersMutex_. pollOnce() takes that same mutex to publish
// readings, so a worker that was mid-poll could never finish and the join
// never returned.
TEST(BydConcurrencyTest, UnsubscribeReturnsWhileWorkerIsPublishing) {
  auto transport = std::make_unique<FakeHttpTransport>();
  FakeHttpTransport* fake = transport.get();
  fake->blockCalls();
  auto client =
      std::make_shared<BydCloudClient>(testAccount(), std::move(transport));

  const BydAccountConfig account = testAccount();
  auto poller = BydVehiclePoller::acquire(
      account, testVehicle("TESTVINCONCURRENCY0003"), client);

  auto state = std::make_shared<BydVehiclePoller::SubscriberState>();
  state->owner = state.get();
  BydVehiclePoller::Subscriber subscriber;
  subscriber.state = state;
  poller->subscribe(subscriber);

  // Park the worker inside pollOnce(), before it takes subscribersMutex_ to
  // publish the readings it just fetched.
  ASSERT_TRUE(fake->waitForFirstCall(10s))
      << "worker never called the transport; Bangcle tables not found from "
         "the current directory";

  auto call = std::make_shared<UnsubscribeCall>();
  call->poller = poller;
  call->owner = state->owner;

  // The hook fires while unsubscribe() holds subscribersMutex_, which removes
  // the timing guesswork: the worker is only released once the lock is
  // provably held, so a join performed under that lock must deadlock.
  poller->setSubscribersLockHookForTest([call] {
    {
      std::lock_guard<std::mutex> hookLock(call->hookMutex);
      call->lockHeld = true;
    }
    call->hookCv.notify_all();
  });

  std::thread unsubscriber([call] {
    call->poller->unsubscribe(call->owner);
    call->done.store(true);
  });

  {
    std::unique_lock<std::mutex> hookLock(call->hookMutex);
    ASSERT_TRUE(call->hookCv.wait_for(hookLock, 10s, [&call] {
      return call->lockHeld;
    })) << "unsubscribe() never reached subscribersMutex_";
  }
  fake->openGate();

  const auto deadline = std::chrono::steady_clock::now() + 10s;
  while (!call->done.load() && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(10ms);
  }

  if (!call->done.load()) {
    // The detached unsubscriber keeps `call` - and with it the poller - alive,
    // so nothing is destroyed while the hung call still references it.
    unsubscriber.detach();
    FAIL() << "unsubscribe() deadlocked on subscribersMutex_ while joining "
              "the worker";
  }

  unsubscriber.join();
  poller->shutdown();
  SUCCEED();
}

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

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
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

// Relative to the test working directory (extras/test/build). postSecure()
// refuses to touch the transport until the Bangcle codec has loaded these, so
// a wrong path shows up below as "transport never reached".
constexpr const char* kBangcleTablesPath =
    "../../porting/linux/byd/data/bangcle_tables.bin";

BydAccountConfig testAccount() {
  BydAccountConfig account;
  account.username = "concurrency-test@example.com";
  account.password = "secret";
  account.baseUrl = "https://example.invalid";
  account.bangcleTablesPath = kBangcleTablesPath;
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

// Runs fn() on its own thread and reports whether it finished in time. A hung
// call is detached rather than joined so the watchdog itself cannot block.
bool runWithTimeout(const std::function<bool()>& fn,
                    std::chrono::milliseconds timeout,
                    bool* result) {
  std::atomic<bool> done{false};
  std::thread worker([&fn, &done, result] {
    *result = fn();
    done.store(true);
  });

  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (!done.load() && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(10ms);
  }

  if (!done.load()) {
    worker.detach();
    return false;
  }
  worker.join();
  return true;
}

}  // namespace

// fetch*() locked mutex_ and then called postTokenJson(), which reached
// ensureLoggedIn() - locking the same non-recursive mutex again on the same
// thread. That is undefined behaviour and in practice the call never returned,
// so the poller worker blocked on its first poll and shutdown() hung in join().
TEST(BydConcurrencyTest, FetchReturnsInsteadOfDeadlockingOnClientMutex) {
  auto transport = std::make_unique<FakeHttpTransport>();
  FakeHttpTransport* fake = transport.get();
  BydCloudClient client(testAccount(), std::move(transport));

  nlohmann::json out;
  std::string error;
  bool fetched = true;
  const bool returned = runWithTimeout(
      [&client, &out, &error] {
        return client.fetchRealtime(
            testVehicle("TESTVINCONCURRENCY0001"), &out, &error);
      },
      5s,
      &fetched);

  ASSERT_TRUE(returned)
      << "fetchRealtime() deadlocked: ensureLoggedIn() re-locked mutex_";
  // No login response is served, so the fetch reports failure - the point is
  // that it returns at all, and with the request actually sent.
  EXPECT_FALSE(fetched);
  EXPECT_GT(fake->calls(), 0)
      << "transport was never reached; missing Bangcle tables at "
      << kBangcleTablesPath;
}

// Same lock-recursion on another entry point, which also retries once on a
// session-expiry code.
TEST(BydConcurrencyTest, ChargingHomepageFetchReturnsInsteadOfDeadlocking) {
  auto transport = std::make_unique<FakeHttpTransport>();
  FakeHttpTransport* fake = transport.get();
  BydCloudClient client(testAccount(), std::move(transport));

  nlohmann::json out;
  std::string error;
  bool fetched = true;
  const bool returned = runWithTimeout(
      [&client, &out, &error] {
        return client.fetchChargingHomepage(
            "TESTVINCONCURRENCY0002", &out, &error);
      },
      5s,
      &fetched);

  ASSERT_TRUE(returned) << "fetchChargingHomepage() deadlocked: "
                           "ensureLoggedIn() re-locked mutex_";
  EXPECT_FALSE(fetched);
  EXPECT_GT(fake->calls(), 0)
      << "transport was never reached; missing Bangcle tables at "
      << kBangcleTablesPath;
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
      << "worker never called the transport; missing Bangcle tables at "
      << kBangcleTablesPath;

  std::atomic<bool> unsubscribed{false};
  std::thread unsubscriber([&poller, &state, &unsubscribed] {
    poller->unsubscribe(state->owner);
    unsubscribed.store(true);
  });

  // Let unsubscribe() take subscribersMutex_, then release the worker so it
  // needs that same mutex to publish before it can reach the stop flag.
  std::this_thread::sleep_for(200ms);
  fake->openGate();

  const auto deadline = std::chrono::steady_clock::now() + 10s;
  while (!unsubscribed.load() && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(10ms);
  }

  if (!unsubscribed.load()) {
    unsubscriber.detach();
    // Intentional leak: destroying the poller here would destroy a still
    // joinable std::thread and call std::terminate, which would hide the
    // deadlock this test exists to report.
    new std::shared_ptr<BydVehiclePoller>(std::move(poller));
    FAIL() << "unsubscribe() deadlocked on subscribersMutex_ while joining "
              "the worker";
  }

  unsubscriber.join();
  poller->shutdown();
  SUCCEED();
}

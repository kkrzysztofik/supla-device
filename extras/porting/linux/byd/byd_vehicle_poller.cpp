/*
 Copyright (C) Krzysztof Krzysztofik
*/

#include "byd_vehicle_poller.h"

#include <supla/log_wrapper.h>

#include <algorithm>
#include <chrono>
#include <map>
#include <thread>

namespace Supla {
namespace Linux {
namespace Byd {

namespace {

std::mutex gRegistryMutex;
std::map<std::string, std::weak_ptr<BydVehiclePoller>> gPollersByKey;
std::map<std::string, std::shared_ptr<BydCloudClient>> gClientsByAccount;

std::string pollerKey(const BydAccountConfig& account, const std::string& vin) {
  return accountKey(account) + "|" + vin;
}

}  // namespace

std::shared_ptr<BydVehiclePoller> BydVehiclePoller::acquire(
    const BydAccountConfig& account,
    const BydVehicleConfig& vehicle,
    std::shared_ptr<BydCloudClient> client) {
  const std::string key = pollerKey(account, vehicle.vin);
  std::lock_guard<std::mutex> lock(gRegistryMutex);
  if (auto existing = gPollersByKey[key].lock()) {
    return existing;
  }
  auto poller = std::shared_ptr<BydVehiclePoller>(
      new BydVehiclePoller(account, vehicle, std::move(client)));
  gPollersByKey[key] = poller;
  return poller;
}

BydVehiclePoller::BydVehiclePoller(BydAccountConfig account,
                                   BydVehicleConfig vehicle,
                                   std::shared_ptr<BydCloudClient> client)
    : account_(std::move(account)),
      vehicle_(std::move(vehicle)),
      client_(std::move(client)) {}

void BydVehiclePoller::shutdown() {
  stopWorker_.store(true);
  if (worker_.joinable()) {
    worker_.join();
  }
  workerRunning_.store(false);
}

void BydVehiclePoller::subscribe(Subscriber subscriber) {
  std::lock_guard<std::mutex> lock(subscribersMutex_);
  subscribers_.push_back(std::move(subscriber));
  startWorkerIfNeeded();
}

void BydVehiclePoller::unsubscribe(void* owner) {
  std::lock_guard<std::mutex> lock(subscribersMutex_);
  subscribers_.erase(
      std::remove_if(subscribers_.begin(),
                     subscribers_.end(),
                     [owner](const Subscriber& sub) {
                       return sub.state && sub.state->owner == owner;
                     }),
      subscribers_.end());
  stopWorkerIfIdle();
}

void BydVehiclePoller::startWorkerIfNeeded() {
  if (workerRunning_.load()) {
    return;
  }
  stopWorker_.store(false);
  workerRunning_.store(true);
  worker_ = std::thread([self = shared_from_this()]() { self->workerLoop(); });
}

void BydVehiclePoller::stopWorkerIfIdle() {
  if (!subscribers_.empty()) {
    return;
  }
  stopWorker_.store(true);
  if (worker_.joinable()) {
    worker_.join();
  }
  workerRunning_.store(false);
}

void BydVehiclePoller::pollOnce() {
  nlohmann::json realtime;
  nlohmann::json charging;
  nlohmann::json hvac;
  nlohmann::json gps;
  nlohmann::json energy;
  bool hasRealtime = false;
  bool hasCharging = false;
  bool hasHvac = false;
  bool hasGps = false;
  bool hasEnergy = false;
  std::string error;

  if (client_->fetchRealtime(vehicle_, &realtime, &error)) {
    hasRealtime = true;
  } else {
    SUPLA_LOG_DEBUG("BYD poller realtime failed vin=%s: %s",
                    vehicle_.vin.c_str(),
                    error.c_str());
  }

  error.clear();
  if (client_->fetchChargingHomepage(vehicle_.vin, &charging, &error)) {
    hasCharging = true;
  } else {
    SUPLA_LOG_DEBUG("BYD poller charging failed vin=%s: %s",
                    vehicle_.vin.c_str(),
                    error.c_str());
  }

  if (cycleCounter_ % kHvacPollEveryCycles == 0) {
    error.clear();
    if (client_->fetchHvacStatus(vehicle_.vin, &hvac, &error)) {
      hasHvac = true;
    }
  }

  if (cycleCounter_ % kGpsPollEveryCycles == 0) {
    error.clear();
    if (client_->fetchGps(vehicle_.vin, &gps, &error)) {
      hasGps = true;
    }
  }

  if (cycleCounter_ % kEnergyPollEveryCycles == 0) {
    error.clear();
    if (client_->fetchEnergyConsumption(vehicle_, &energy, &error)) {
      hasEnergy = true;
    }
  }

  const BydReadings readings = BydReadingsBuilder::fromSources(
      realtime, charging, hvac, gps, energy,
      hasRealtime, hasCharging, hasHvac, hasGps, hasEnergy);

  std::lock_guard<std::mutex> lock(subscribersMutex_);
  for (const auto& subscriber : subscribers_) {
    if (!subscriber.state) {
      continue;
    }
    std::lock_guard<std::mutex> stateLock(subscriber.state->mutex);
    subscriber.state->readings = readings;
  }
}

void BydVehiclePoller::workerLoop() {
  const int intervalSec = std::max(5, vehicle_.pollIntervalSec);
  while (!stopWorker_.load()) {
    pollOnce();
    ++cycleCounter_;
    for (int i = 0; i < intervalSec * 10 && !stopWorker_.load(); ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }
}

void shutdownAllBydPollers() {
  std::lock_guard<std::mutex> lock(gRegistryMutex);
  for (auto& entry : gPollersByKey) {
    if (auto poller = entry.second.lock()) {
      poller->shutdown();
    }
  }
  gPollersByKey.clear();
  gClientsByAccount.clear();
}

std::shared_ptr<BydCloudClient> acquireBydCloudClient(
    const BydAccountConfig& account) {
  const std::string key = accountKey(account);
  std::lock_guard<std::mutex> lock(gRegistryMutex);
  auto it = gClientsByAccount.find(key);
  if (it != gClientsByAccount.end()) {
    return it->second;
  }
  auto client = std::make_shared<BydCloudClient>(account);
  gClientsByAccount[key] = client;
  return client;
}

}  // namespace Byd
}  // namespace Linux
}  // namespace Supla

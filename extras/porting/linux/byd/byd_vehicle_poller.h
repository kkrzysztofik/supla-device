/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#ifndef EXTRAS_PORTING_LINUX_BYD_BYD_VEHICLE_POLLER_H_
#define EXTRAS_PORTING_LINUX_BYD_BYD_VEHICLE_POLLER_H_

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifdef SUPLA_TEST
#include <functional>
#endif

#include "byd_cloud_client.h"
#include "byd_readings.h"
#include "byd_types.h"

namespace Supla {
namespace Linux {
namespace Byd {

class BydVehiclePoller : public std::enable_shared_from_this<BydVehiclePoller> {
 public:
  struct SubscriberState {
    void* owner = nullptr;
    mutable std::mutex mutex;
    BydReadings readings;
  };

  struct Subscriber {
    std::shared_ptr<SubscriberState> state;
  };

  static std::shared_ptr<BydVehiclePoller> acquire(
      const BydAccountConfig& account,
      const BydVehicleConfig& vehicle,
      std::shared_ptr<BydCloudClient> client);

  void shutdown();

  void subscribe(Subscriber subscriber);
  void unsubscribe(void* owner);

#ifdef SUPLA_TEST
  // Invoked while subscribersMutex_ is held in unsubscribe(), so a test can
  // order the worker join against the lock acquisition deterministically.
  void setSubscribersLockHookForTest(std::function<void()> hook) {
    subscribersLockHook_ = std::move(hook);
  }
#endif

 private:
  BydVehiclePoller(BydAccountConfig account,
                   BydVehicleConfig vehicle,
                   std::shared_ptr<BydCloudClient> client);
  void startWorkerIfNeeded();
  void stopWorkerIfIdle();
  void workerLoop();
  void pollOnce();

  BydAccountConfig account_;
  BydVehicleConfig vehicle_;
  std::shared_ptr<BydCloudClient> client_;

  std::mutex subscribersMutex_;
  std::vector<Subscriber> subscribers_;

  std::thread worker_;
  std::atomic<bool> stopWorker_{false};
  std::atomic<bool> workerRunning_{false};
  int cycleCounter_ = 0;

#ifdef SUPLA_TEST
  std::function<void()> subscribersLockHook_;
#endif
};

void shutdownAllBydPollers();
std::shared_ptr<BydCloudClient> acquireBydCloudClient(
    const BydAccountConfig& account);

}  // namespace Byd
}  // namespace Linux
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_BYD_BYD_VEHICLE_POLLER_H_

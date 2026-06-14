/*
 Copyright (C) Krzysztof Krzysztofik
*/

#ifndef EXTRAS_PORTING_LINUX_BYD_BYD_VEHICLE_POLLER_H_
#define EXTRAS_PORTING_LINUX_BYD_BYD_VEHICLE_POLLER_H_

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

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
};

void shutdownAllBydPollers();
std::shared_ptr<BydCloudClient> acquireBydCloudClient(
    const BydAccountConfig& account);

}  // namespace Byd
}  // namespace Linux
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_BYD_BYD_VEHICLE_POLLER_H_

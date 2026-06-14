/*
 Copyright (C) Krzysztof Krzysztofik
*/

#ifndef EXTRAS_PORTING_LINUX_BYD_BYD_POLLER_CLIENT_H_
#define EXTRAS_PORTING_LINUX_BYD_BYD_POLLER_CLIENT_H_

#include <map>
#include <memory>
#include <mutex>
#include <string>

#include "byd_vehicle_poller.h"

namespace Supla {
namespace Linux {
namespace Byd {

class BydPollerClient {
 public:
  BydPollerClient(void* owner,
                  BydAccountConfig account,
                  BydVehicleConfig vehicle,
                  std::shared_ptr<BydCloudClient> client);
  BydPollerClient(const BydPollerClient&) = delete;
  BydPollerClient& operator=(const BydPollerClient&) = delete;
  ~BydPollerClient();

  void attach();
  void detach();

  bool copyReadings(BydReadings* readings) const;

#ifdef SUPLA_TEST
  void setReadingsForTest(const BydReadings& readings);
#endif

 private:
  std::shared_ptr<BydVehiclePoller> poller_;
  std::shared_ptr<BydVehiclePoller::SubscriberState> state_;
};

}  // namespace Byd
}  // namespace Linux
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_BYD_BYD_POLLER_CLIENT_H_

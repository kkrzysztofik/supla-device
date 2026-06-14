/*
 Copyright (C) Krzysztof Krzysztofik
*/

#include "byd_poller_client.h"

namespace Supla {
namespace Linux {
namespace Byd {

BydPollerClient::BydPollerClient(void* owner,
                                 BydAccountConfig account,
                                 BydVehicleConfig vehicle,
                                 std::shared_ptr<BydCloudClient> client)
    : poller_(BydVehiclePoller::acquire(account, vehicle, client)),
      state_(std::make_shared<BydVehiclePoller::SubscriberState>()) {
  state_->owner = owner;
}

BydPollerClient::~BydPollerClient() {
  detach();
}

void BydPollerClient::attach() {
  if (!poller_ || !state_) {
    return;
  }
  BydVehiclePoller::Subscriber subscriber;
  subscriber.state = state_;
  poller_->subscribe(std::move(subscriber));
}

void BydPollerClient::detach() {
  if (poller_ && state_) {
    poller_->unsubscribe(state_->owner);
  }
}

bool BydPollerClient::copyReadings(BydReadings* readings) const {
  if (readings == nullptr || !state_) {
    return false;
  }
  std::lock_guard<std::mutex> lock(state_->mutex);
  *readings = state_->readings;
  return state_->readings.valid;
}

#ifdef SUPLA_TEST
void BydPollerClient::setReadingsForTest(const BydReadings& readings) {
  if (!state_) {
    return;
  }
  std::lock_guard<std::mutex> lock(state_->mutex);
  state_->readings = readings;
  state_->readings.valid = true;
}
#endif

}  // namespace Byd
}  // namespace Linux
}  // namespace Supla

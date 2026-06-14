/*
 Copyright (C) Krzysztof Krzysztofik
*/

#include "byd_meter.h"

#include <cmath>

#include "byd_vehicle_poller.h"
#include "linux_channel_read_helpers.h"

namespace Supla {
namespace PV {

BydMeter::BydMeter(Supla::Linux::Byd::BydAccountConfig account,
                   Supla::Linux::Byd::BydVehicleConfig vehicle,
                   std::string fieldKey)
    : Supla::Sensor::GeneralPurposeMeter(nullptr, false),
      fieldKey_(std::move(fieldKey)),
      pollerClient_(this,
                    account,
                    vehicle,
                    Supla::Linux::Byd::acquireBydCloudClient(account)) {
  const int intervalSec = std::max(5, vehicle.pollIntervalSec);
  setRefreshIntervalMs(intervalSec * 1000);
}

BydMeter::~BydMeter() = default;

void BydMeter::onInit() {
  pollerClient_.attach();
  Supla::Sensor::GeneralPurposeMeter::onInit();
}

#ifdef SUPLA_TEST
void BydMeter::setReadingsForTest(
    const Supla::Linux::Byd::BydReadings& readings) {
  pollerClient_.setReadingsForTest(readings);
}
#endif

double BydMeter::getValue() {
  Supla::Linux::Byd::BydReadings readings;
  if (!pollerClient_.copyReadings(&readings) || !readings.valid) {
    if (Supla::Linux::markInvalidRead(&staleReadCounter_)) {
      return NAN;
    }
    return channel.getValueDouble();
  }

  Supla::Linux::markValidRead(&staleReadCounter_);
  const auto it = readings.values.find(fieldKey_);
  if (it == readings.values.end() || !std::isfinite(it->second)) {
    return NAN;
  }
  return it->second;
}

}  // namespace PV
}  // namespace Supla

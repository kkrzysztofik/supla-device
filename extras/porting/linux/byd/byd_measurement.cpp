/*
 Copyright (C) Krzysztof Krzysztofik
*/

#include "byd_measurement.h"

#include <cmath>

#include "byd_vehicle_poller.h"
#include "linux_channel_read_helpers.h"

namespace Supla {
namespace PV {

BydMeasurement::BydMeasurement(Supla::Linux::Byd::BydAccountConfig account,
                               Supla::Linux::Byd::BydVehicleConfig vehicle,
                               std::string fieldKey)
    : Supla::Sensor::GeneralPurposeMeasurement(nullptr, false),
      fieldKey_(std::move(fieldKey)),
      pollerClient_(this,
                    account,
                    vehicle,
                    Supla::Linux::Byd::acquireBydCloudClient(account)) {
  const int intervalSec = std::max(5, vehicle.pollIntervalSec);
  setRefreshIntervalMs(intervalSec * 1000);
}

BydMeasurement::~BydMeasurement() = default;

void BydMeasurement::onInit() {
  pollerClient_.attach();
  Supla::Sensor::GeneralPurposeMeasurement::onInit();
}

#ifdef SUPLA_TEST
void BydMeasurement::setReadingsForTest(
    const Supla::Linux::Byd::BydReadings& readings) {
  pollerClient_.setReadingsForTest(readings);
}
#endif

double BydMeasurement::getValue() {
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

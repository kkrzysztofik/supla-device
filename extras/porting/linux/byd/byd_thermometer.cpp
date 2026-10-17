/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include "byd_thermometer.h"

#include <cmath>

#include "byd_vehicle_poller.h"
#include "linux_channel_read_helpers.h"

namespace Supla {
namespace PV {

BydThermometer::BydThermometer(Supla::Linux::Byd::BydAccountConfig account,
                               Supla::Linux::Byd::BydVehicleConfig vehicle,
                               std::string fieldKey)
    : fieldKey_(std::move(fieldKey)),
      pollerClient_(this,
                    account,
                    vehicle,
                    Supla::Linux::Byd::acquireBydCloudClient(account)) {
  const int intervalSec = std::max(5, vehicle.pollIntervalSec);
  setRefreshIntervalMs(intervalSec * 1000);
}

BydThermometer::~BydThermometer() = default;

void BydThermometer::onInit() {
  pollerClient_.attach();
  Supla::Sensor::Thermometer::onInit();
}

#ifdef SUPLA_TEST
void BydThermometer::setReadingsForTest(
    const Supla::Linux::Byd::BydReadings& readings) {
  pollerClient_.setReadingsForTest(readings);
}
#endif

double BydThermometer::getValue() {
  Supla::Linux::Byd::BydReadings readings;
  if (!pollerClient_.copyReadings(&readings) || !readings.valid) {
    if (Supla::Linux::markInvalidRead(&staleReadCounter_)) {
      return TEMPERATURE_NOT_AVAILABLE;
    }
    return channel.getValueDouble();
  }

  Supla::Linux::markValidRead(&staleReadCounter_);
  const auto it = readings.values.find(fieldKey_);
  if (it == readings.values.end() || !std::isfinite(it->second)) {
    return TEMPERATURE_NOT_AVAILABLE;
  }
  return it->second;
}

}  // namespace PV
}  // namespace Supla

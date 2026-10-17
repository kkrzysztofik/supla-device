/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include "byd_binary.h"

#include "byd_vehicle_poller.h"
#include "linux_channel_read_helpers.h"

namespace Supla {
namespace PV {

BydBinary::BydBinary(Supla::Linux::Byd::BydAccountConfig account,
                     Supla::Linux::Byd::BydVehicleConfig vehicle,
                     std::string fieldKey)
    : Supla::Sensor::VirtualBinary(false),
      fieldKey_(std::move(fieldKey)),
      pollerClient_(this,
                    account,
                    vehicle,
                    Supla::Linux::Byd::acquireBydCloudClient(account)) {
  setUseConfiguredTimeout(false);
}

BydBinary::~BydBinary() = default;

void BydBinary::onInit() {
  pollerClient_.attach();
  Supla::Sensor::VirtualBinary::onInit();
}

#ifdef SUPLA_TEST
void BydBinary::setReadingsForTest(
    const Supla::Linux::Byd::BydReadings& readings) {
  pollerClient_.setReadingsForTest(readings);
}
#endif

bool BydBinary::getValue() {
  Supla::Linux::Byd::BydReadings readings;
  if (!pollerClient_.copyReadings(&readings) || !readings.valid) {
    if (Supla::Linux::markInvalidRead(&staleReadCounter_)) {
      return false;
    }
    return Supla::Sensor::VirtualBinary::getValue();
  }

  Supla::Linux::markValidRead(&staleReadCounter_);
  const auto it = readings.booleans.find(fieldKey_);
  if (it == readings.booleans.end()) {
    return false;
  }
  return it->second;
}

}  // namespace PV
}  // namespace Supla

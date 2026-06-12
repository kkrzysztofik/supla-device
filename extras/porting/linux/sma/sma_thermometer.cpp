/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include "sma_thermometer.h"

#include <supla/sensor/thermometer_driver.h>

#include <cmath>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace Supla {
namespace PV {

namespace {

int normalizePollIntervalSec(int pollIntervalSec) {
  return pollIntervalSec > 0 ? pollIntervalSec
                             : Supla::Linux::Sma::kDefaultPollIntervalSec;
}

}  // namespace

SmaThermometer::SmaThermometer(std::string serialDevice,
                               int baud,
                               Supla::Linux::Sma::SerialMedia media,
                               uint16_t netAddress,
                               int pollIntervalSec,
                               std::vector<SmaMappedChannel> channels,
                               std::string deviceProfile)
    : channelKey_(channels.empty() ? std::string() : channels.front().key),
      busClient_(this,
                 Supla::Linux::Sma::SmaBusConfig{
                     std::move(serialDevice),
                     baud,
                     media,
                     netAddress,
                     normalizePollIntervalSec(pollIntervalSec),
                     // NOLINTNEXTLINE(whitespace/indent_namespace)
                     std::move(deviceProfile)},
                 std::move(channels)) {
  const int intervalSec = normalizePollIntervalSec(pollIntervalSec);
  setRefreshIntervalMs(intervalSec * 1000);
}

SmaThermometer::~SmaThermometer() = default;

void SmaThermometer::onInit() {
  busClient_.attach();
  Supla::Sensor::Thermometer::onInit();
}

double SmaThermometer::getValue() {
  std::map<std::string, double> values;
  bool valid = false;
  if (!busClient_.copyReadings(&values, &valid) || !valid) {
    staleReadCounter_++;
    if (staleReadCounter_ > 3) {
      return TEMPERATURE_NOT_AVAILABLE;
    }
    return channel.getValueDouble();
  }

  staleReadCounter_ = 0;
  const auto it = values.find(channelKey_);
  if (it == values.end() || !std::isfinite(it->second)) {
    return TEMPERATURE_NOT_AVAILABLE;
  }

  return it->second;
}

}  // namespace PV
}  // namespace Supla

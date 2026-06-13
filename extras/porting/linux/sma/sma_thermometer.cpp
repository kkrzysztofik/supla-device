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

#include "sma_channel_helpers.h"

namespace Supla {
namespace PV {

namespace {
constexpr int kMaxStaleReads = 3;
}  // namespace

SmaThermometer::SmaThermometer(Config config)
    : channelKey_(config.channels.empty() ? std::string() : config.channels.front().key),
      busClient_(this,
                 makeSmaBusConfig(std::move(config.serialDevice),
                                  config.baud,
                                  config.media,
                                  config.netAddress,
                                  config.pollIntervalSec,
                                  std::move(config.deviceProfile)),
                 std::move(config.channels)) {
  const int intervalSec = normalizeSmaPollIntervalSec(config.pollIntervalSec);
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
    if (staleReadCounter_ > kMaxStaleReads) {
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

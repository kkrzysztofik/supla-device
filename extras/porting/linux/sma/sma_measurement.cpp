/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include "sma_measurement.h"

#include <cmath>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "linux_channel_read_helpers.h"
#include "sma_channel_helpers.h"

namespace Supla {
namespace PV {

SmaMeasurement::SmaMeasurement(std::string serialDevice,
                               int baud,
                               Supla::Linux::Sma::SerialMedia media,
                               uint16_t netAddress,
                               int pollIntervalSec,
                               std::vector<SmaMappedChannel> channels,
                               std::string deviceProfile)
    : Supla::Sensor::GeneralPurposeMeasurement(nullptr, false),
      channelKey_(channels.empty() ? std::string() : channels.front().key),
      busClient_(this,
                 makeSmaBusConfig(std::move(serialDevice),
                                  baud,
                                  media,
                                  netAddress,
                                  pollIntervalSec,
                                  std::move(deviceProfile)),
                 std::move(channels)) {
  const int intervalSec = normalizeSmaPollIntervalSec(pollIntervalSec);
  setRefreshIntervalMs(intervalSec * 1000);
}

SmaMeasurement::~SmaMeasurement() = default;

void SmaMeasurement::onInit() {
  busClient_.attach();
  Supla::Sensor::GeneralPurposeMeasurement::onInit();
}

#ifdef SUPLA_TEST
void SmaMeasurement::setReadingsForTest(
    const std::map<std::string, double>& values,
    bool valid,
    Supla::Linux::Sma::LinkState link) {
  busClient_.setReadingsForTest(values, valid, link);
}
#endif

double SmaMeasurement::getValue() {
  using Supla::Linux::Sma::LinkState;
  std::map<std::string, double> values;
  bool valid = false;
  auto link = LinkState::Unknown;
  const bool copied = busClient_.copyReadings(&values, &valid, &link);
  if (channelKey_.empty()) {
    // Reported at once, without the stale grace, so alerts are not delayed.
    switch (link) {
      case LinkState::Ok:
        return 1;
      case LinkState::Silent:
        return 0;
      case LinkState::Error:
        return -1;
      case LinkState::Unknown:
        return NAN;
    }
  }
  if (!copied || !valid) {
    if (Supla::Linux::markInvalidRead(&staleReadCounter_)) {
      return NAN;
    }
    return channel.getValueDouble();
  }

  Supla::Linux::markValidRead(&staleReadCounter_);
  const auto it = values.find(channelKey_);
  if (it == values.end() || !std::isfinite(it->second)) {
    return NAN;
  }

  return it->second;
}

}  // namespace PV
}  // namespace Supla

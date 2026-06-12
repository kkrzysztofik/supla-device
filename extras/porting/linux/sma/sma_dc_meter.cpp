/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include "sma_dc_meter.h"

#include <cmath>
#include <map>
#include <string>
#include <vector>

#include "sma_channel_helpers.h"

namespace Supla {
namespace PV {

namespace {

bool mappingIsPower(const char* mapping) {
  return smaMappingIs(mapping, "power_active");
}

bool mappingIsVoltage(const char* mapping) {
  return smaMappingIs(mapping, "voltage");
}

bool mappingIsCurrent(const char* mapping) {
  return smaMappingIs(mapping, "current");
}

}  // namespace

void SmaDcMeter::applyMappedReadings(
    const std::map<std::string, double>& values) {
  SmaInverter::applyMappedReadings(values);

  bool hasExplicitPower = false;
  double voltage = 0.0;
  double current = 0.0;
  bool hasVoltage = false;
  bool hasCurrent = false;
  std::vector<SmaMappedChannel> channels;
  busClient_.copyChannels(&channels);

  for (const auto& mapped : channels) {
    const char* mapping = mapped.descriptor.suplaMapping;
    if (mappingIsPower(mapping)) {
      hasExplicitPower = true;
    }
    auto it = values.find(mapped.key);
    if (it == values.end()) {
      continue;
    }
    if (mappingIsVoltage(mapping)) {
      voltage = it->second;
      hasVoltage = true;
    } else if (mappingIsCurrent(mapping)) {
      current = it->second;
      hasCurrent = true;
    }
  }

  if (!hasExplicitPower && hasVoltage && hasCurrent) {
    const double power = voltage * current;
    if (std::isfinite(power)) {
      setPowerActive(0, static_cast<_supla_int_t>(power * 100000.0));
      invDisabledCounter_ = 0;
    }
  }
}

}  // namespace PV
}  // namespace Supla

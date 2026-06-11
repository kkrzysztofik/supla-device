/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include "sma_inverter.h"

#include <supla/time.h>

#include <cstring>
#include <map>
#include <utility>
#include <vector>

namespace Supla {
namespace PV {

namespace {

bool mappingIsPower(const char* mapping) {
  return mapping != nullptr && std::strcmp(mapping, "power_active") == 0;
}

bool mappingIsFwdEnergy(const char* mapping) {
  return mapping != nullptr && std::strcmp(mapping, "fwd_act_energy") == 0;
}

bool mappingIsVoltage(const char* mapping) {
  return mapping != nullptr && std::strcmp(mapping, "voltage") == 0;
}

bool mappingIsCurrent(const char* mapping) {
  return mapping != nullptr && std::strcmp(mapping, "current") == 0;
}

bool mappingIsFrequency(const char* mapping) {
  return mapping != nullptr && std::strcmp(mapping, "frequency") == 0;
}

}  // namespace

SmaInverter::SmaInverter(std::string serialDevice,
                         int baud,
                         Supla::Linux::Sma::SerialMedia media,
                         uint16_t netAddress,
                         int pollIntervalSec,
                         std::vector<SmaMappedChannel> channels,
                         std::string deviceProfile)
    : busClient_(this,
                 Supla::Linux::Sma::SmaBusConfig{
                     std::move(serialDevice),
                     baud,
                     media,
                     netAddress,
                     pollIntervalSec > 0 ? pollIntervalSec : 15,
                     std::move(deviceProfile)},
                 std::move(channels)),
      pollIntervalSec_(pollIntervalSec > 0 ? pollIntervalSec : 15) {
  refreshRateSec = pollIntervalSec_;
  extChannel.setFlag(SUPLA_CHANNEL_FLAG_PHASE2_UNSUPPORTED);
  extChannel.setFlag(SUPLA_CHANNEL_FLAG_PHASE3_UNSUPPORTED);
}

SmaInverter::~SmaInverter() = default;

void SmaInverter::onInit() {
  Supla::Sensor::ElectricityMeter::onInit();
  busClient_.attach();
}

void SmaInverter::setZeroValues() {
  setPowerActive(0, 0);
  setCurrent(0, 0);
  setVoltage(0, 0);
  setFreq(0);
}

void SmaInverter::applyMappedReadings(
    const std::map<std::string, double>& values) {
  bool hasPower = false;

  for (const auto& mapped : busClient_.channels()) {
    auto it = values.find(mapped.key);
    if (it == values.end()) {
      continue;
    }
    const double value = it->second;
    const char* mapping = mapped.descriptor.suplaMapping;
    if (mappingIsPower(mapping)) {
      setPowerActive(0, static_cast<_supla_int_t>(value * 100000.0));
      hasPower = true;
    } else if (mappingIsFwdEnergy(mapping)) {
      setFwdActEnergy(
          0, static_cast<unsigned _supla_int64_t>(value * 100000.0));
    } else if (mappingIsVoltage(mapping)) {
      setVoltage(0, static_cast<unsigned _supla_int16_t>(value * 100.0));
    } else if (mappingIsCurrent(mapping)) {
      setCurrent(0, static_cast<unsigned _supla_int16_t>(value * 1000.0));
    } else if (mappingIsFrequency(mapping)) {
      setFreq(static_cast<unsigned _supla_int16_t>(value * 100.0));
      hasPower = true;
    }
  }

  if (!hasPower) {
    invDisabledCounter_++;
  }
}

void SmaInverter::applyReadingsToChannel() {
  std::map<std::string, double> values;
  bool valid = false;
  busClient_.copyReadings(&values, &valid);

  if (!valid) {
    invDisabledCounter_++;
    if (invDisabledCounter_ > 3) {
      setZeroValues();
      updateChannelValues();
    }
    return;
  }

  invDisabledCounter_ = 0;
  applyMappedReadings(values);
  updateChannelValues();
}

void SmaInverter::iterateAlways() {
  const uint32_t pollMs = static_cast<uint32_t>(pollIntervalSec_ * 1000);
  if (lastReadTime == 0 || millis() - lastReadTime > pollMs) {
    lastReadTime = millis();
    applyReadingsToChannel();
  }
  Supla::Sensor::ElectricityMeter::iterateAlways();
}

}  // namespace PV
}  // namespace Supla

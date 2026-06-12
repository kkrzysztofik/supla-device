/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include "sma_inverter.h"

#include <supla/time.h>

#include <cmath>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "sma_channel_helpers.h"

namespace Supla {
namespace PV {

namespace {

bool mappingIsPower(const char* mapping) {
  return smaMappingIs(mapping, "power_active");
}

bool mappingIsRvrEnergy(const char* mapping) {
  return smaMappingIs(mapping, "rvr_act_energy");
}

bool mappingIsFwdEnergy(const char* mapping) {
  return smaMappingIs(mapping, "fwd_act_energy");
}

bool mappingIsVoltage(const char* mapping) {
  return smaMappingIs(mapping, "voltage");
}

bool mappingIsCurrent(const char* mapping) {
  return smaMappingIs(mapping, "current");
}

bool mappingIsFrequency(const char* mapping) {
  return smaMappingIs(mapping, "frequency");
}

// SMA Pac is positive when feeding the grid; SUPLA uses negative active power
// for export (see ElectricityMeterParsed / grid meter convention).
int64_t smaAcPowerToSupla(double watts) {
  return static_cast<int64_t>(std::llround(-watts * 100000.0));
}

unsigned smaAcCurrentToSupla(double amps) {
  return static_cast<unsigned>(std::llround(std::abs(amps) * 1000.0));
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
                 makeSmaBusConfig(std::move(serialDevice),
                                  baud,
                                  media,
                                  netAddress,
                                  pollIntervalSec,
                                  std::move(deviceProfile)),
                 std::move(channels)),
      // NOLINTNEXTLINE(whitespace/indent_namespace)
      pollIntervalSec_(normalizeSmaPollIntervalSec(pollIntervalSec)) {
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
  std::vector<SmaMappedChannel> channels;
  busClient_.copyChannels(&channels);

  for (const auto& mapped : channels) {
    auto it = values.find(mapped.key);
    if (it == values.end()) {
      continue;
    }
    const double value = it->second;
    const char* mapping = mapped.descriptor.suplaMapping;
    if (mappingIsPower(mapping)) {
      setPowerActive(0, smaAcPowerToSupla(value));
      hasPower = true;
    } else if (mappingIsRvrEnergy(mapping)) {
      // Inverter yield (E-Total) is energy exported to grid → rvr_act_energy.
      setRvrActEnergy(
          0,
          static_cast<unsigned _supla_int64_t>(std::llround(value * 100000.0)));
    } else if (mappingIsFwdEnergy(mapping)) {
      setFwdActEnergy(
          0,
          static_cast<unsigned _supla_int64_t>(std::llround(value * 100000.0)));
    } else if (mappingIsVoltage(mapping)) {
      setVoltage(0, static_cast<unsigned _supla_int16_t>(value * 100.0));
    } else if (mappingIsCurrent(mapping)) {
      setCurrent(0, smaAcCurrentToSupla(value));
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

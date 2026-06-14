/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include "ingecon_dc_meter.h"

#include <supla/time.h>

#include <cstdint>
#include <limits>
#include <utility>

namespace Supla {
namespace PV {

namespace {

unsigned _supla_int16_t voltageToSupla(uint16_t voltage) {
  const uint32_t scaled = static_cast<uint32_t>(voltage) * 100U;
  return scaled >
                 static_cast<uint32_t>(
                     std::numeric_limits<unsigned _supla_int16_t>::max())
             ? std::numeric_limits<unsigned _supla_int16_t>::max()
             : static_cast<unsigned _supla_int16_t>(scaled);
}

unsigned _supla_int_t currentToSupla(uint16_t current) {
  const uint64_t scaled = static_cast<uint64_t>(current) * 1000U;
  return scaled >
                 static_cast<uint64_t>(
                     std::numeric_limits<unsigned _supla_int_t>::max())
             ? std::numeric_limits<unsigned _supla_int_t>::max()
             : static_cast<unsigned _supla_int_t>(scaled);
}

int64_t powerToSupla(uint32_t voltage, uint32_t current) {
  return static_cast<int64_t>(voltage) * current * 100000LL;
}

}  // namespace

IngeconDcMeter::IngeconDcMeter(Supla::Linux::Ingecon::BusConfig config)
    : busClient_(this, config) {
  pollIntervalSec_ =
      Supla::Linux::Ingecon::normalizePollIntervalSec(config.pollIntervalSec);
  refreshRateSec = pollIntervalSec_;
  extChannel.setFlag(SUPLA_CHANNEL_FLAG_PHASE2_UNSUPPORTED);
  extChannel.setFlag(SUPLA_CHANNEL_FLAG_PHASE3_UNSUPPORTED);
}

IngeconDcMeter::~IngeconDcMeter() = default;

void IngeconDcMeter::onInit() {
  Supla::Sensor::ElectricityMeter::onInit();
  busClient_.attach();
}

#ifdef SUPLA_TEST
void IngeconDcMeter::setReadingsForTest(
    const Supla::Linux::Ingecon::Readings& readings,
    bool valid) {
  busClient_.setReadingsForTest(readings, valid);
}

void IngeconDcMeter::applyReadingsForTest() {
  applyReadingsToChannel();
}
#endif

void IngeconDcMeter::setZeroInstantaneousValues() {
  setPowerActive(0, 0);
  setCurrent(0, 0);
  setVoltage(0, 0);
  setFreq(0);
  setPowerFactor(0, 0);
}

void IngeconDcMeter::applyValidReadings(
    const Supla::Linux::Ingecon::Readings& readings) {
  setVoltage(0, voltageToSupla(readings.vdc));
  setCurrent(0, currentToSupla(readings.idc));
  setPowerActive(0, powerToSupla(readings.vdc, readings.idc));
  setFreq(0);
  setPowerFactor(0, 0);
}

void IngeconDcMeter::applyReadingsToChannel() {
  Supla::Linux::Ingecon::Readings readings;
  bool valid = false;
  if (!busClient_.copyReadings(&readings, &valid) || !valid) {
    staleReadCounter_++;
    if (staleReadCounter_ > 3) {
      setZeroInstantaneousValues();
      updateChannelValues();
    }
    return;
  }

  staleReadCounter_ = 0;
  applyValidReadings(readings);
  updateChannelValues();
}

void IngeconDcMeter::iterateAlways() {
  const uint32_t pollMs = static_cast<uint32_t>(pollIntervalSec_ * 1000);
  if (lastReadTime_ == 0 || millis() - lastReadTime_ > pollMs) {
    lastReadTime_ = millis();
    applyReadingsToChannel();
  }
  Supla::Sensor::ElectricityMeter::iterateAlways();
}

}  // namespace PV
}  // namespace Supla

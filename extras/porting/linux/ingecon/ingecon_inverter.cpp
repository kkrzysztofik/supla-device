/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include "ingecon_inverter.h"

#include <supla/time.h>

#include <cmath>
#include <cstdint>
#include <utility>

namespace Supla {
namespace PV {

namespace {

uint64_t kwhToSuplaEnergy(uint32_t kwh) {
  return static_cast<uint64_t>(kwh) * 100000ULL;
}

}  // namespace

IngeconInverter::IngeconInverter(
    Supla::Linux::Ingecon::BusConfig config,
    IngeconEnergyMapping energyMapping)
    : busClient_(this, config), energyMapping_(energyMapping) {
  pollIntervalSec_ =
      Supla::Linux::Ingecon::normalizePollIntervalSec(config.pollIntervalSec);
  refreshRateSec = pollIntervalSec_;
  extChannel.setFlag(SUPLA_CHANNEL_FLAG_PHASE2_UNSUPPORTED);
  extChannel.setFlag(SUPLA_CHANNEL_FLAG_PHASE3_UNSUPPORTED);
}

IngeconInverter::~IngeconInverter() = default;

void IngeconInverter::onInit() {
  Supla::Sensor::ElectricityMeter::onInit();
  busClient_.attach();
}

#ifdef SUPLA_TEST
void IngeconInverter::setReadingsForTest(
    const Supla::Linux::Ingecon::Readings& readings,
    bool valid) {
  busClient_.setReadingsForTest(readings, valid);
}

void IngeconInverter::applyReadingsForTest() {
  applyReadingsToChannel();
}
#endif

void IngeconInverter::setZeroInstantaneousValues() {
  setPowerActive(0, 0);
  setCurrent(0, 0);
  setVoltage(0, 0);
  setFreq(0);
  setPowerFactor(0, 0);
}

void IngeconInverter::applyValidReadings(
    const Supla::Linux::Ingecon::Readings& readings) {
  const uint64_t energy = kwhToSuplaEnergy(readings.totalEnergyKwh);
  if (energyMapping_ == IngeconEnergyMapping::Forward) {
    setFwdActEnergy(0, energy);
  } else {
    setRvrActEnergy(0, energy);
  }

  setPowerActive(0, -static_cast<int64_t>(readings.pac) * 100000LL);
  setVoltage(0, static_cast<unsigned _supla_int16_t>(readings.vac * 100U));
  setCurrent(0, static_cast<unsigned _supla_int_t>(readings.iac * 1000U));
  setFreq(readings.fac);
  setPowerFactor(0, readings.cosPhi);
}

void IngeconInverter::applyReadingsToChannel() {
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

void IngeconInverter::iterateAlways() {
  const uint32_t pollMs = static_cast<uint32_t>(pollIntervalSec_ * 1000);
  if (lastReadTime == 0 || millis() - lastReadTime > pollMs) {
    lastReadTime = millis();
    applyReadingsToChannel();
  }
  Supla::Sensor::ElectricityMeter::iterateAlways();
}

}  // namespace PV
}  // namespace Supla

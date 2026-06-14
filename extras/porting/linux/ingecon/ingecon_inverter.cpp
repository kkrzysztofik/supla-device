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
#include <limits>
#include <utility>

#include "linux_channel_read_helpers.h"

namespace Supla {
namespace PV {

namespace {

uint64_t kwhToSuplaEnergy(uint32_t kwh) {
  return static_cast<uint64_t>(kwh) * 100000ULL;
}

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

int64_t powerToSupla(int32_t power) {
  return static_cast<int64_t>(power) * 100000LL;
}

int64_t inverterPowerToSuplaExport(int32_t power) {
  return -powerToSupla(power);
}

}  // namespace

IngeconInverter::IngeconInverter(
    Supla::Linux::Ingecon::BusConfig config,
    IngeconEnergyMapping energyMapping)
    : busClient_(this, config),
      energyMapping_(energyMapping),
      configuredProfile_(config.profile) {
  pollIntervalSec_ =
      Supla::Linux::Ingecon::normalizePollIntervalSec(config.pollIntervalSec);
  refreshRateSec = pollIntervalSec_;
  if (!isThreePhaseConfigured()) {
    extChannel.setFlag(SUPLA_CHANNEL_FLAG_PHASE2_UNSUPPORTED);
    extChannel.setFlag(SUPLA_CHANNEL_FLAG_PHASE3_UNSUPPORTED);
  }
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
  const int phaseCount = isThreePhaseConfigured() ? 3 : 1;
  for (int phase = 0; phase < phaseCount; ++phase) {
    setPowerActive(phase, 0);
    setCurrent(phase, 0);
    setVoltage(phase, 0);
    setPowerFactor(phase, 0);
  }
  setFreq(0);
}

void IngeconInverter::applyValidReadings(
    const Supla::Linux::Ingecon::Readings& readings) {
  if (isThreePhaseConfigured() &&
      readings.profile == Supla::Linux::Ingecon::Profile::TrifAasV1) {
    applyThreePhaseReadings(readings);
  } else {
    applyOnePhaseReadings(readings);
  }
}

void IngeconInverter::applyOnePhaseReadings(
    const Supla::Linux::Ingecon::Readings& readings) {
  const uint64_t energy = kwhToSuplaEnergy(readings.totalEnergyKwh);
  if (energyMapping_ == IngeconEnergyMapping::Forward) {
    setFwdActEnergy(0, energy);
  } else {
    setRvrActEnergy(0, energy);
  }

  setPowerActive(0, inverterPowerToSuplaExport(readings.pac));
  setVoltage(0, voltageToSupla(readings.vac));
  setCurrent(0, currentToSupla(readings.iac));
  setFreq(readings.fac);
  setPowerFactor(0, readings.cosPhi);
}

void IngeconInverter::applyThreePhaseReadings(
    const Supla::Linux::Ingecon::Readings& readings) {
  const uint64_t energy = kwhToSuplaEnergy(readings.totalEnergyKwh) / 3ULL;
  const int64_t powerPerPhase = inverterPowerToSuplaExport(readings.pac) / 3;
  const uint16_t voltages[3] = {readings.vac, readings.vac2, readings.vac3};
  const uint16_t currents[3] = {readings.iac, readings.iac2, readings.iac3};

  for (int phase = 0; phase < 3; ++phase) {
    if (energyMapping_ == IngeconEnergyMapping::Forward) {
      setFwdActEnergy(phase, energy);
      setRvrActEnergy(phase, 0);
    } else {
      setRvrActEnergy(phase, energy);
      setFwdActEnergy(phase, 0);
    }
    setVoltage(phase, voltageToSupla(voltages[phase]));
    setCurrent(phase, currentToSupla(currents[phase]));
    setPowerActive(phase, powerPerPhase);
    setPowerFactor(phase, readings.cosPhi);
  }
  setFreq(readings.fac);
}

bool IngeconInverter::isThreePhaseConfigured() const {
  return configuredProfile_ == Supla::Linux::Ingecon::Profile::TrifAasV1;
}

void IngeconInverter::applyReadingsToChannel() {
  Supla::Linux::Ingecon::Readings readings;
  bool valid = false;
  if (!busClient_.copyReadings(&readings, &valid) || !valid) {
    if (Supla::Linux::markInvalidRead(&staleReadCounter_)) {
      setZeroInstantaneousValues();
      updateChannelValues();
    }
    return;
  }

  Supla::Linux::markValidRead(&staleReadCounter_);
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

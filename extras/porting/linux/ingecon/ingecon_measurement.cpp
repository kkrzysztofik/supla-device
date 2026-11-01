/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include "ingecon_measurement.h"

#include <cmath>
#include <string>
#include <utility>

#include "linux_channel_read_helpers.h"

namespace Supla {
namespace PV {

IngeconMeasurement::IngeconMeasurement(
    Supla::Linux::Ingecon::BusConfig config,
    std::string valueKey)
    : Supla::Sensor::GeneralPurposeMeasurement(nullptr, false),
      valueKey_(std::move(valueKey)),
      busClient_(this, config) {
  const int intervalSec =
      Supla::Linux::Ingecon::normalizePollIntervalSec(config.pollIntervalSec);
  setRefreshIntervalMs(intervalSec * 1000);
}

IngeconMeasurement::~IngeconMeasurement() = default;

void IngeconMeasurement::onInit() {
  busClient_.attach();
  Supla::Sensor::GeneralPurposeMeasurement::onInit();
}

#ifdef SUPLA_TEST
void IngeconMeasurement::setReadingsForTest(
    const Supla::Linux::Ingecon::Readings& readings,
    bool valid,
    bool silent) {
  busClient_.setReadingsForTest(readings, valid, silent);
}
#endif

double IngeconMeasurement::readValue(
    const Supla::Linux::Ingecon::Readings& readings) const {
  if (valueKey_ == "link") return 1;
  if (valueKey_ == "vdc") return readings.vdc;
  if (valueKey_ == "idc") return readings.idc;
  if (valueKey_ == "vbus") return readings.vbus;
  if (valueKey_ == "vac") return readings.vac;
  if (valueKey_ == "vac2") return readings.vac2;
  if (valueKey_ == "vac3") return readings.vac3;
  if (valueKey_ == "iac") return readings.iac;
  if (valueKey_ == "iac2") return readings.iac2;
  if (valueKey_ == "iac3") return readings.iac3;
  if (valueKey_ == "pac") return readings.pac;
  if (valueKey_ == "fac") return readings.fac / 100.0;
  if (valueKey_ == "cos_phi") return readings.cosPhi / 1000.0;
  if (valueKey_ == "sin_sign") return readings.sinSign;
  if (valueKey_ == "profile") return static_cast<int>(readings.profile);
  if (valueKey_ == "temperature") return readings.temperature;
  if (valueKey_ == "alarm_inverter") return readings.alarmInverter;
  if (valueKey_ == "alarm_safety") return readings.alarmSafety;
  if (valueKey_ == "hours_running") return readings.hoursRunning;
  if (valueKey_ == "grid_connections") return readings.gridConnections;
  if (valueKey_ == "status1") return readings.status1;
  if (valueKey_ == "status2") return readings.status2;
  if (valueKey_ == "alarms") return readings.alarms;
  if (valueKey_ == "year") return readings.year;
  if (valueKey_ == "month") return readings.month;
  if (valueKey_ == "day") return readings.day;
  if (valueKey_ == "hour") return readings.hour;
  if (valueKey_ == "minute") return readings.minute;
  if (valueKey_ == "second") return readings.second;
  if (valueKey_.rfind("display_fw_word_", 0) == 0) {
    if (!readings.displayFwValid) {
      return NAN;
    }
    const std::string suffix = valueKey_.substr(16);
    char* end = nullptr;
    const long word = std::strtol(suffix.c_str(), &end, 10);
    if (end != suffix.c_str() && *end == '\0' && word >= 1 &&
        word <= Supla::Linux::Ingecon::kDisplayFwRegisterCount) {
      return readings.displayFw[word - 1];
    }
  }
  return NAN;
}

// A silent inverter (asleep, e.g. at night) has no voltage, current or power,
// so these read 0 like the Ingecon meters do. On a port or frame error, and
// for anything else (alarms, temperature, counters, dates), the value is
// unknown and stays NaN.
bool IngeconMeasurement::isElectricalValue() const {
  for (const char* key : {"vdc", "idc", "vbus", "vac", "vac2", "vac3", "iac",
                          "iac2", "iac3", "pac", "fac", "cos_phi"}) {
    if (valueKey_ == key) {
      return true;
    }
  }
  return false;
}

double IngeconMeasurement::getValue() {
  Supla::Linux::Ingecon::Readings readings;
  using Supla::Linux::Ingecon::LinkState;
  bool valid = false;
  auto link = LinkState::Unknown;
  if (!busClient_.copyReadings(&readings, &valid, &link) || !valid) {
    const bool silent = link == LinkState::Silent;
    // link: 1 = reads OK, 0 = inverter silent, -1 = port/frame error,
    // unavailable until the first poll finishes (no false startup alert).
    // Reported at once, without the stale grace, so alerts are not delayed.
    if (valueKey_ == "link") {
      return link == LinkState::Unknown ? NAN : silent ? 0 : -1;
    }
    if (Supla::Linux::markInvalidRead(&staleReadCounter_)) {
      return silent && isElectricalValue() ? 0 : NAN;
    }
    return channel.getValueDouble();
  }

  Supla::Linux::markValidRead(&staleReadCounter_);
  return readValue(readings);
}

}  // namespace PV
}  // namespace Supla

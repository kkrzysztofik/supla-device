/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#ifndef EXTRAS_PORTING_LINUX_INGECON_INGECON_TYPES_H_
#define EXTRAS_PORTING_LINUX_INGECON_INGECON_TYPES_H_

#include <cstdint>
#include <optional>
#include <string>

namespace Supla {
namespace Linux {
namespace Ingecon {

constexpr int kDefaultBaud = 9600;
constexpr int kDefaultModbusAddress = 1;
constexpr int kDefaultPollIntervalSec = 15;
constexpr int kDefaultTimeoutMs = 340;
constexpr int kDefaultRetries = 2;
constexpr int kMainInputRegisterCount = 27;
constexpr int kSunManagerInputRegisterCount = 47;
constexpr int kDisplayFwRegisterCount = 10;

enum class Profile {
  Auto,
  Lite27,
  MonofAayV1,
  MonofAapV1,
  TrifAasV1,
};

struct BusConfig {
  std::string serialDevice;
  int baud = kDefaultBaud;
  uint8_t modbusAddress = kDefaultModbusAddress;
  int pollIntervalSec = kDefaultPollIntervalSec;
  int timeoutMs = kDefaultTimeoutMs;
  int retries = kDefaultRetries;
  bool rtsToggle = true;
  Profile profile = Profile::Auto;
};

struct Readings {
  Profile profile = Profile::Lite27;
  bool discoveryValid = false;
  std::string serialNumber;
  std::string firmwareCode;
  uint32_t totalEnergyKwh = 0;
  bool totalEnergyValid = false;
  uint32_t hoursRunning = 0;
  uint32_t gridConnections = 0;
  uint32_t status1 = 0;
  uint32_t status2 = 0;
  uint32_t alarms = 0;
  uint16_t vdc = 0;
  uint16_t idc = 0;
  uint16_t vbus = 0;
  uint16_t iac = 0;
  uint16_t iac2 = 0;
  uint16_t iac3 = 0;
  int32_t pac = 0;
  uint16_t cosPhi = 0;
  uint16_t sinSign = 0;
  uint16_t vac = 0;
  uint16_t vac2 = 0;
  uint16_t vac3 = 0;
  uint16_t fac = 0;
  uint16_t temperature = 0;
  uint16_t alarmInverter = 0;
  uint16_t alarmSafety = 0;
  uint16_t year = 0;
  uint16_t month = 0;
  uint16_t day = 0;
  uint16_t hour = 0;
  uint16_t minute = 0;
  uint16_t second = 0;
  uint16_t displayFw[kDisplayFwRegisterCount] = {};
  bool displayFwValid = false;
};

// Lifetime energy counter guard. Inverters occasionally return a CRC-valid
// frame with a garbage high word (e.g. 0x8080E470), which would put a
// ~2 billion kWh spike into cloud history. A jump outside
// [accepted, accepted + kMaxEnergyStepKwh] is held back until a following
// poll confirms it (handles counter resets / inverter replacement).
//
// The first reading is never trusted on its own: it only becomes a candidate,
// and a second reading consistent with it is required before anything is
// published. Returns nullopt while no value may be published yet.
constexpr uint32_t kMaxEnergyStepKwh = 1000;

struct EnergyFilter {
  bool hasAccepted = false;
  uint32_t accepted = 0;
  bool hasCandidate = false;
  uint32_t candidate = 0;
};

std::optional<uint32_t> filterTotalEnergyKwh(EnergyFilter* filter,
                                             uint32_t kwh);

int normalizePollIntervalSec(int pollIntervalSec);
int normalizeTimeoutMs(int timeoutMs);
int normalizeRetries(int retries);
const char* profileToString(Profile profile);
bool parseProfileName(const std::string& value, Profile* profile);

}  // namespace Ingecon
}  // namespace Linux
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_INGECON_INGECON_TYPES_H_

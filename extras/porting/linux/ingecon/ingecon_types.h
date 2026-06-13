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
#include <string>

namespace Supla {
namespace Linux {
namespace Ingecon {

constexpr int kDefaultBaud = 9600;
constexpr int kDefaultModbusAddress = 1;
constexpr int kDefaultPollIntervalSec = 15;
constexpr int kDefaultTimeoutMs = 1000;
constexpr int kDefaultRetries = 2;
constexpr int kMainInputRegisterCount = 27;
constexpr int kDisplayFwRegisterCount = 10;

struct BusConfig {
  std::string serialDevice;
  int baud = kDefaultBaud;
  uint8_t modbusAddress = kDefaultModbusAddress;
  int pollIntervalSec = kDefaultPollIntervalSec;
  int timeoutMs = kDefaultTimeoutMs;
  int retries = kDefaultRetries;
};

struct Readings {
  uint32_t totalEnergyKwh = 0;
  uint32_t hoursRunning = 0;
  uint32_t gridConnections = 0;
  uint32_t status1 = 0;
  uint32_t status2 = 0;
  uint32_t alarms = 0;
  uint16_t vdc = 0;
  uint16_t idc = 0;
  uint16_t vbus = 0;
  uint16_t iac = 0;
  int16_t pac = 0;
  uint16_t cosPhi = 0;
  uint16_t sinSign = 0;
  uint16_t vac = 0;
  uint16_t fac = 0;
  uint16_t year = 0;
  uint16_t month = 0;
  uint16_t day = 0;
  uint16_t hour = 0;
  uint16_t minute = 0;
  uint16_t second = 0;
  uint16_t displayFw[kDisplayFwRegisterCount] = {};
  bool displayFwValid = false;
};

int normalizePollIntervalSec(int pollIntervalSec);
int normalizeTimeoutMs(int timeoutMs);
int normalizeRetries(int retries);

}  // namespace Ingecon
}  // namespace Linux
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_INGECON_INGECON_TYPES_H_

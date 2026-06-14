/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include "ingecon_modbus_rtu.h"

#include <supla/log_wrapper.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace Supla {
namespace Linux {
namespace Ingecon {

namespace {

constexpr uint8_t kReadInputRegisters = 0x04;
constexpr uint8_t kReadSerialNumber = 0x11;

uint32_t joinU32(uint16_t high, uint16_t low) {
  return (static_cast<uint32_t>(high) << 16) | low;
}

uint16_t roundedCentiToUnit(uint16_t value) {
  return static_cast<uint16_t>((static_cast<uint32_t>(value) + 50U) / 100U);
}

int32_t signedDecaToUnit(uint16_t value) {
  return static_cast<int32_t>(static_cast<int16_t>(value)) * 10;
}

void parseLite27Registers(const std::vector<uint16_t>& registers,
                          Readings* readings) {
  if (registers.size() < 27) {
    SUPLA_LOG_WARNING(
        "parseLite27Registers: expected 27 registers, got %zu",
        registers.size());
    return;
  }
  readings->totalEnergyKwh = joinU32(registers[0], registers[1]);
  readings->hoursRunning = joinU32(registers[2], registers[3]);
  readings->gridConnections = joinU32(registers[4], registers[5]);
  readings->status1 = joinU32(registers[6], registers[7]);
  readings->status2 = joinU32(registers[8], registers[9]);
  readings->alarms = joinU32(registers[10], registers[11]);
  readings->alarmInverter = registers[10];
  readings->alarmSafety = registers[11];
  readings->vdc = registers[12];
  readings->idc = registers[13];
  readings->vbus = registers[14];
  readings->iac = registers[15];
  readings->pac = static_cast<int16_t>(registers[16]);
  readings->cosPhi = registers[17];
  readings->sinSign = registers[18];
  readings->vac = registers[19];
  readings->fac = registers[20];
  readings->year = registers[21];
  readings->month = registers[22];
  readings->day = registers[23];
  readings->hour = registers[24];
  readings->minute = registers[25];
  readings->second = registers[26];
}

void parseMonofAapV1Registers(const std::vector<uint16_t>& registers,
                              Readings* readings) {
  readings->profile = Profile::MonofAapV1;
  readings->totalEnergyKwh = joinU32(registers[0], registers[1]);
  readings->hoursRunning = joinU32(registers[2], registers[3]);
  readings->gridConnections = joinU32(registers[4], registers[5]);
  readings->alarms = joinU32(registers[8], registers[9]);
  readings->status1 = registers[10];
  readings->alarmInverter = registers[8];
  readings->alarmSafety = registers[9];
  readings->vdc = registers[11];
  readings->idc = roundedCentiToUnit(registers[12]);
  readings->vbus = registers[13];
  readings->iac = roundedCentiToUnit(registers[14]);
  readings->pac = static_cast<int16_t>(registers[15]);
  readings->cosPhi = registers[16];
  readings->sinSign = registers[17];
  readings->vac = registers[18];
  readings->fac = registers[19];
  readings->year = registers[20];
  readings->month = registers[21];
  readings->day = registers[22];
  readings->hour = registers[23];
  readings->minute = registers[24];
  readings->second = registers[25];
}

void parseTrifAasV1Registers(const std::vector<uint16_t>& registers,
                             Readings* readings) {
  readings->profile = Profile::TrifAasV1;
  readings->totalEnergyKwh = joinU32(registers[0], registers[1]);
  readings->hoursRunning = joinU32(registers[2], registers[3]);
  readings->gridConnections = joinU32(registers[4], registers[5]);
  readings->alarms = joinU32(registers[6], registers[7]);
  readings->vdc = registers[8];
  readings->alarmInverter = registers[6];
  readings->alarmSafety = registers[7];
  readings->idc = registers[9];
  readings->vac = registers[10];
  readings->vac2 = registers[11];
  readings->vac3 = registers[12];
  readings->iac = registers[13];
  readings->iac2 = registers[14];
  readings->iac3 = registers[15];
  readings->vbus = 0;
  readings->cosPhi = registers[16];
  readings->sinSign = registers[17];
  readings->pac = signedDecaToUnit(registers[18]);
  readings->fac = registers[19];
  readings->year = registers[20];
  readings->month = registers[21];
  readings->day = registers[22];
  readings->hour = registers[23];
  readings->minute = registers[24];
  readings->second = registers[25];
}

std::string trimAscii(const uint8_t* data, size_t len) {
  if (data == nullptr || len == 0) {
    return {};
  }
  size_t begin = 0;
  while (begin < len && data[begin] == ' ') {
    ++begin;
  }
  size_t end = len;
  while (end > begin && (data[end - 1] == ' ' || data[end - 1] == '\0')) {
    --end;
  }
  return std::string(reinterpret_cast<const char*>(data + begin), end - begin);
}

}  // namespace

uint16_t modbusCrc16(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t pos = 0; pos < len; ++pos) {
    crc ^= data[pos];
    for (int bit = 0; bit < 8; ++bit) {
      if ((crc & 0x0001) != 0) {
        crc = static_cast<uint16_t>((crc >> 1) ^ 0xA001);
      } else {
        crc = static_cast<uint16_t>(crc >> 1);
      }
    }
  }
  return crc;
}

std::string bytesToHex(const uint8_t* data, size_t len) {
  if (data == nullptr || len == 0) {
    return {};
  }

  std::string output;
  output.reserve(len * 3);
  for (size_t i = 0; i < len; ++i) {
    char byteText[4] = {};
    std::snprintf(byteText, sizeof(byteText), "%02X", data[i]);
    if (!output.empty()) {
      output.push_back(' ');
    }
    output += byteText;
  }
  return output;
}

std::vector<uint8_t> buildReadInputRegistersRequest(uint8_t slave,
                                                    uint16_t address,
                                                    uint16_t count) {
  std::vector<uint8_t> frame{
      slave,
      kReadInputRegisters,
      static_cast<uint8_t>((address >> 8) & 0xFF),
      static_cast<uint8_t>(address & 0xFF),
      static_cast<uint8_t>((count >> 8) & 0xFF),
      static_cast<uint8_t>(count & 0xFF),
  };
  const uint16_t crc = modbusCrc16(frame.data(), frame.size());
  frame.push_back(static_cast<uint8_t>(crc & 0xFF));
  frame.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));
  SUPLA_LOG_VERBOSE(
      "IngeconModbus: built request slave=%u function=0x%02X "
      "address=%u count=%u crc=0x%04X frame=[%s]",
      slave,
      kReadInputRegisters,
      address,
      count,
      crc,
      bytesToHex(frame.data(), frame.size()).c_str());
  return frame;
}

std::vector<uint8_t> buildReadSerialNumberRequest(uint8_t slave) {
  std::vector<uint8_t> frame{slave, kReadSerialNumber};
  const uint16_t crc = modbusCrc16(frame.data(), frame.size());
  frame.push_back(static_cast<uint8_t>(crc & 0xFF));
  frame.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));
  SUPLA_LOG_VERBOSE(
      "IngeconModbus: built serial request slave=%u function=0x%02X "
      "crc=0x%04X frame=[%s]",
      slave,
      kReadSerialNumber,
      crc,
      bytesToHex(frame.data(), frame.size()).c_str());
  return frame;
}

bool parseReadInputRegistersResponse(const uint8_t* frame,
                                     size_t frameLen,
                                     uint8_t slave,
                                     uint16_t expectedCount,
                                     std::vector<uint16_t>* registers) {
  if (frame == nullptr || registers == nullptr || expectedCount == 0) {
    SUPLA_LOG_WARNING(
        "IngeconModbus: invalid parser input frame=%p registers=%p count=%u",
        frame,
        registers,
        expectedCount);
    return false;
  }

  const size_t expectedByteCount = static_cast<size_t>(expectedCount) * 2;
  const size_t expectedFrameLen = expectedByteCount + 5;
  if (frameLen != expectedFrameLen) {
    SUPLA_LOG_WARNING(
        "IngeconModbus: invalid response length got=%zu expected=%zu "
        "registers=%u frame=[%s]",
        frameLen,
        expectedFrameLen,
        expectedCount,
        bytesToHex(frame, frameLen).c_str());
    return false;
  }
  if (frame[0] != slave) {
    SUPLA_LOG_WARNING("IngeconModbus: invalid slave got=%u expected=%u",
                      frame[0],
                      slave);
    return false;
  }
  if (frame[1] != kReadInputRegisters) {
    SUPLA_LOG_WARNING("IngeconModbus: invalid function got=0x%02X expected=0x%02X",
                      frame[1],
                      kReadInputRegisters);
    return false;
  }
  if (frame[2] != expectedByteCount) {
    SUPLA_LOG_WARNING(
        "IngeconModbus: invalid byte count got=%u expected=%zu",
        frame[2],
        expectedByteCount);
    return false;
  }

  const uint16_t expectedCrc =
      static_cast<uint16_t>(frame[frameLen - 2]) |
      (static_cast<uint16_t>(frame[frameLen - 1]) << 8);
  const uint16_t calculatedCrc = modbusCrc16(frame, frameLen - 2);
  if (calculatedCrc != expectedCrc) {
    SUPLA_LOG_WARNING(
        "IngeconModbus: CRC mismatch got=0x%04X expected=0x%04X frame=[%s]",
        expectedCrc,
        calculatedCrc,
        bytesToHex(frame, frameLen).c_str());
    return false;
  }

  registers->clear();
  registers->reserve(expectedCount);
  for (uint16_t i = 0; i < expectedCount; ++i) {
    const size_t offset = 3 + static_cast<size_t>(i) * 2;
    registers->push_back(
        static_cast<uint16_t>((frame[offset] << 8) | frame[offset + 1]));
  }
  SUPLA_LOG_VERBOSE("IngeconModbus: parsed %u input registers",
                    expectedCount);
  return true;
}

bool parseReadSerialNumberResponse(const uint8_t* frame,
                                   size_t frameLen,
                                   uint8_t slave,
                                   Readings* readings) {
  constexpr size_t kSerialFrameLen = 29;
  if (frame == nullptr || readings == nullptr) {
    SUPLA_LOG_WARNING("IngeconModbus: invalid serial parser input");
    return false;
  }
  if (frameLen != kSerialFrameLen) {
    SUPLA_LOG_WARNING(
        "IngeconModbus: invalid serial response length got=%zu expected=%zu "
        "frame=[%s]",
        frameLen,
        kSerialFrameLen,
        bytesToHex(frame, frameLen).c_str());
    return false;
  }
  if (frame[0] != slave || frame[1] != kReadSerialNumber) {
    SUPLA_LOG_WARNING(
        "IngeconModbus: invalid serial response header slave=%u/%u "
        "function=0x%02X",
        frame[0],
        slave,
        frame[1]);
    return false;
  }

  const uint16_t expectedCrc =
      static_cast<uint16_t>(frame[frameLen - 2]) |
      (static_cast<uint16_t>(frame[frameLen - 1]) << 8);
  const uint16_t calculatedCrc = modbusCrc16(frame, frameLen - 2);
  if (calculatedCrc != expectedCrc) {
    SUPLA_LOG_WARNING(
        "IngeconModbus: serial CRC mismatch got=0x%04X expected=0x%04X "
        "frame=[%s]",
        expectedCrc,
        calculatedCrc,
        bytesToHex(frame, frameLen).c_str());
    return false;
  }

  readings->serialNumber = trimAscii(frame + 5, 12);
  readings->firmwareCode = trimAscii(frame + 17, 10);
  readings->discoveryValid = true;
  SUPLA_LOG_INFO("IngeconModbus: discovered serial=%s firmware=%s",
                 readings->serialNumber.c_str(),
                 readings->firmwareCode.c_str());
  return true;
}

bool parseMainInputRegisters(const std::vector<uint16_t>& registers,
                             Readings* readings) {
  if (readings == nullptr || registers.size() != kMainInputRegisterCount) {
    SUPLA_LOG_WARNING(
        "IngeconModbus: invalid main register block size=%zu expected=%d",
        registers.size(),
        kMainInputRegisterCount);
    return false;
  }

  readings->profile = Profile::Lite27;
  parseLite27Registers(registers, readings);
  SUPLA_LOG_DEBUG(
      "IngeconModbus: main readings energy=%u h=%u grid=%u status1=0x%08X "
      "status2=0x%08X alarms=0x%08X vdc=%u idc=%u vbus=%u iac=%u pac=%d "
      "cos=%u sinSign=%u vac=%u fac=%u time=%04u-%02u-%02u %02u:%02u:%02u",
      readings->totalEnergyKwh,
      readings->hoursRunning,
      readings->gridConnections,
      readings->status1,
      readings->status2,
      readings->alarms,
      readings->vdc,
      readings->idc,
      readings->vbus,
      readings->iac,
      readings->pac,
      readings->cosPhi,
      readings->sinSign,
      readings->vac,
      readings->fac,
      readings->year,
      readings->month,
      readings->day,
      readings->hour,
      readings->minute,
      readings->second);
  return true;
}

bool parseInputRegistersForProfile(Profile profile,
                                   const std::vector<uint16_t>& registers,
                                   Readings* readings) {
  if (readings == nullptr) {
    return false;
  }
  const uint16_t expectedCount = inputRegisterCountForProfile(profile);
  if (registers.size() < expectedCount) {
    SUPLA_LOG_WARNING(
        "IngeconModbus: invalid %s register block size=%zu expected>=%u",
        profileToString(profile),
        registers.size(),
        expectedCount);
    return false;
  }

  switch (profile) {
    case Profile::Auto:
    case Profile::Lite27:
      readings->profile = Profile::Lite27;
      parseLite27Registers(registers, readings);
      break;
    case Profile::MonofAayV1:
      readings->profile = Profile::MonofAayV1;
      parseLite27Registers(registers, readings);
      break;
    case Profile::MonofAapV1:
      parseMonofAapV1Registers(registers, readings);
      break;
    case Profile::TrifAasV1:
      parseTrifAasV1Registers(registers, readings);
      break;
  }

  SUPLA_LOG_DEBUG(
      "IngeconModbus: %s readings energy=%u h=%u grid=%u alarms=0x%08X "
      "vdc=%u idc=%u vbus=%u iac=%u/%u/%u pac=%d cos=%u sinSign=%u "
      "vac=%u/%u/%u fac=%u time=%04u-%02u-%02u %02u:%02u:%02u",
      profileToString(readings->profile),
      readings->totalEnergyKwh,
      readings->hoursRunning,
      readings->gridConnections,
      readings->alarms,
      readings->vdc,
      readings->idc,
      readings->vbus,
      readings->iac,
      readings->iac2,
      readings->iac3,
      readings->pac,
      readings->cosPhi,
      readings->sinSign,
      readings->vac,
      readings->vac2,
      readings->vac3,
      readings->fac,
      readings->year,
      readings->month,
      readings->day,
      readings->hour,
      readings->minute,
      readings->second);
  return true;
}

Profile resolveProfileFromFirmware(const std::string& firmwareCode) {
  if (firmwareCode.rfind("AAY", 0) == 0) {
    return Profile::MonofAayV1;
  }
  if (firmwareCode.rfind("AAP", 0) == 0) {
    return Profile::MonofAapV1;
  }
  if (firmwareCode.rfind("AAS", 0) == 0) {
    return Profile::TrifAasV1;
  }
  return Profile::Lite27;
}

uint16_t inputRegisterCountForProfile(Profile profile) {
  switch (profile) {
    case Profile::Auto:
    case Profile::Lite27:
      return kMainInputRegisterCount;
    case Profile::MonofAayV1:
    case Profile::MonofAapV1:
    case Profile::TrifAasV1:
      return kSunManagerInputRegisterCount;
  }
  return kMainInputRegisterCount;
}

bool parseDisplayFwRegisters(const std::vector<uint16_t>& registers,
                             Readings* readings) {
  if (readings == nullptr || registers.size() != kDisplayFwRegisterCount) {
    SUPLA_LOG_WARNING(
        "IngeconModbus: invalid display FW block size=%zu expected=%d",
        registers.size(),
        kDisplayFwRegisterCount);
    return false;
  }

  for (size_t i = 0; i < registers.size(); ++i) {
    readings->displayFw[i] = registers[i];
  }
  readings->displayFwValid = true;
  SUPLA_LOG_DEBUG(
      "IngeconModbus: display FW words=%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",
      readings->displayFw[0],
      readings->displayFw[1],
      readings->displayFw[2],
      readings->displayFw[3],
      readings->displayFw[4],
      readings->displayFw[5],
      readings->displayFw[6],
      readings->displayFw[7],
      readings->displayFw[8],
      readings->displayFw[9]);
  return true;
}

}  // namespace Ingecon
}  // namespace Linux
}  // namespace Supla

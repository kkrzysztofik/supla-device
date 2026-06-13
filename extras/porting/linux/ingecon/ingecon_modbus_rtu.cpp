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
#include <string>

namespace Supla {
namespace Linux {
namespace Ingecon {

namespace {

constexpr uint8_t kReadInputRegisters = 0x04;

uint32_t joinU32(uint16_t high, uint16_t low) {
  return (static_cast<uint32_t>(high) << 16) | low;
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

bool parseMainInputRegisters(const std::vector<uint16_t>& registers,
                             Readings* readings) {
  if (readings == nullptr || registers.size() != kMainInputRegisterCount) {
    SUPLA_LOG_WARNING(
        "IngeconModbus: invalid main register block size=%zu expected=%d",
        registers.size(),
        kMainInputRegisterCount);
    return false;
  }

  readings->totalEnergyKwh = joinU32(registers[0], registers[1]);
  readings->hoursRunning = joinU32(registers[2], registers[3]);
  readings->gridConnections = joinU32(registers[4], registers[5]);
  readings->status1 = joinU32(registers[6], registers[7]);
  readings->status2 = joinU32(registers[8], registers[9]);
  readings->alarms = joinU32(registers[10], registers[11]);
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

/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#ifndef EXTRAS_PORTING_LINUX_INGECON_INGECON_MODBUS_RTU_H_
#define EXTRAS_PORTING_LINUX_INGECON_INGECON_MODBUS_RTU_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "ingecon_types.h"

namespace Supla {
namespace Linux {
namespace Ingecon {

uint16_t modbusCrc16(const uint8_t* data, size_t len);

std::string bytesToHex(const uint8_t* data, size_t len);

std::vector<uint8_t> buildReadInputRegistersRequest(uint8_t slave,
                                                    uint16_t address,
                                                    uint16_t count);

std::vector<uint8_t> buildReadSerialNumberRequest(uint8_t slave);

bool parseReadInputRegistersResponse(const uint8_t* frame,
                                     size_t frameLen,
                                     uint8_t slave,
                                     uint16_t expectedCount,
                                     std::vector<uint16_t>* registers);

bool parseReadSerialNumberResponse(const uint8_t* frame,
                                   size_t frameLen,
                                   uint8_t slave,
                                   Readings* readings);

bool parseMainInputRegisters(const std::vector<uint16_t>& registers,
                             Readings* readings);

bool parseInputRegistersForProfile(Profile profile,
                                   const std::vector<uint16_t>& registers,
                                   Readings* readings);

Profile resolveProfileFromFirmware(const std::string& firmwareCode);
uint16_t inputRegisterCountForProfile(Profile profile);

bool parseDisplayFwRegisters(const std::vector<uint16_t>& registers,
                             Readings* readings);

}  // namespace Ingecon
}  // namespace Linux
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_INGECON_INGECON_MODBUS_RTU_H_

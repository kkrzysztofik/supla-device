/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include "ingecon_modbus_rtu.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

using Supla::Linux::Ingecon::Readings;
using Supla::Linux::Ingecon::Profile;
using Supla::Linux::Ingecon::buildReadInputRegistersRequest;
using Supla::Linux::Ingecon::buildReadSerialNumberRequest;
using Supla::Linux::Ingecon::inputRegisterCountForProfile;
using Supla::Linux::Ingecon::modbusCrc16;
using Supla::Linux::Ingecon::parseInputRegistersForProfile;
using Supla::Linux::Ingecon::parseMainInputRegisters;
using Supla::Linux::Ingecon::parseProfileName;
using Supla::Linux::Ingecon::parseReadInputRegistersResponse;
using Supla::Linux::Ingecon::parseReadSerialNumberResponse;
using Supla::Linux::Ingecon::resolveProfileFromFirmware;

namespace {

void appendCrc(std::vector<uint8_t>* frame) {
  const uint16_t crc = modbusCrc16(frame->data(), frame->size());
  frame->push_back(static_cast<uint8_t>(crc & 0xFF));
  frame->push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));
}

}  // namespace

TEST(IngeconModbusRtuTest, BuildsReadInputRegistersRequest) {
  const auto request = buildReadInputRegistersRequest(1, 0, 27);

  const std::vector<uint8_t> expected{
      0x01, 0x04, 0x00, 0x00, 0x00, 0x1B, 0xB0, 0x01};
  EXPECT_EQ(request, expected);
}

TEST(IngeconModbusRtuTest, BuildsReadSerialNumberRequest) {
  auto request = buildReadSerialNumberRequest(1);

  std::vector<uint8_t> expected{0x01, 0x11};
  appendCrc(&expected);
  EXPECT_EQ(request, expected);
}

TEST(IngeconModbusRtuTest, ParsesMainRegisterSample) {
  const std::vector<uint8_t> sample{
      0x01, 0x04, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0xC4, 0x00, 0x00, 0x00,
      0x49, 0x00, 0x00, 0x00, 0x00, 0x0C, 0xA6, 0x01, 0x3D, 0x00,
      0x00, 0x01, 0x3D, 0x00, 0x00, 0x00, 0x00, 0x03, 0xE8, 0x00,
      0x01, 0x02, 0x37, 0x00, 0x00, 0x07, 0xDD, 0x00, 0x01, 0x00,
      0x1E, 0x00, 0x11, 0x00, 0x05, 0x00, 0x0B, 0x1D, 0x73};

  std::vector<uint16_t> registers;
  ASSERT_TRUE(parseReadInputRegistersResponse(
      sample.data(), sample.size(), 1, 27, &registers));

  Readings readings;
  ASSERT_TRUE(parseMainInputRegisters(registers, &readings));

  EXPECT_EQ(readings.status1, 0x20C40000u);
  EXPECT_EQ(readings.status2, 0x00490000u);
  EXPECT_EQ(readings.vdc, 317u);
  EXPECT_EQ(readings.idc, 0u);
  EXPECT_EQ(readings.vbus, 317u);
  EXPECT_EQ(readings.iac, 0u);
  EXPECT_EQ(readings.pac, 0);
  EXPECT_EQ(readings.cosPhi, 1000u);
  EXPECT_EQ(readings.sinSign, 1u);
  EXPECT_EQ(readings.vac, 567u);
  EXPECT_EQ(readings.fac, 0u);
  EXPECT_EQ(readings.year, 2013u);
  EXPECT_EQ(readings.month, 1u);
  EXPECT_EQ(readings.day, 30u);
  EXPECT_EQ(readings.hour, 17u);
  EXPECT_EQ(readings.minute, 5u);
  EXPECT_EQ(readings.second, 11u);
}

TEST(IngeconModbusRtuTest, RejectsInvalidResponses) {
  std::vector<uint8_t> frame{
      0x01, 0x04, 0x02, 0x12, 0x34, 0xB4, 0x47};
  std::vector<uint16_t> registers;

  EXPECT_TRUE(parseReadInputRegistersResponse(
      frame.data(), frame.size(), 1, 1, &registers));

  auto badCrc = frame;
  badCrc.back() ^= 0x01;
  EXPECT_FALSE(parseReadInputRegistersResponse(
      badCrc.data(), badCrc.size(), 1, 1, &registers));

  auto badSlave = frame;
  badSlave[0] = 2;
  EXPECT_FALSE(parseReadInputRegistersResponse(
      badSlave.data(), badSlave.size(), 1, 1, &registers));

  auto badFunction = frame;
  badFunction[1] = 3;
  EXPECT_FALSE(parseReadInputRegistersResponse(
      badFunction.data(), badFunction.size(), 1, 1, &registers));

  auto badByteCount = frame;
  badByteCount[2] = 4;
  EXPECT_FALSE(parseReadInputRegistersResponse(
      badByteCount.data(), badByteCount.size(), 1, 1, &registers));

  EXPECT_FALSE(parseReadInputRegistersResponse(
      frame.data(), frame.size() - 1, 1, 1, &registers));
}

TEST(IngeconModbusRtuTest, ParsesSerialNumberResponse) {
  std::vector<uint8_t> frame{
      0x01, 0x11, 0x00, 0x00, 0x00,
      'S',  'N',  '1',  '2',  '3',  '4',  '5',  '6',  ' ',  ' ',  ' ',  ' ',
      'A',  'A',  'P',  '1',  '0',  '6',  '0',  '_',  'H',  ' '};
  appendCrc(&frame);

  Readings readings;
  ASSERT_TRUE(parseReadSerialNumberResponse(
      frame.data(), frame.size(), 1, &readings));

  EXPECT_TRUE(readings.discoveryValid);
  EXPECT_EQ(readings.serialNumber, "SN123456");
  EXPECT_EQ(readings.firmwareCode, "AAP1060_H");
}

TEST(IngeconModbusRtuTest, SelectsProfileFromFirmware) {
  EXPECT_EQ(resolveProfileFromFirmware("AAY1000_A"), Profile::MonofAayV1);
  EXPECT_EQ(resolveProfileFromFirmware("AAP1060_H"), Profile::MonofAapV1);
  EXPECT_EQ(resolveProfileFromFirmware("AAS1060_H"), Profile::TrifAasV1);
  EXPECT_EQ(resolveProfileFromFirmware("AAS1340_U"), Profile::TrifAasV1);
  EXPECT_EQ(resolveProfileFromFirmware("UNKNOWN"), Profile::Lite27);
  EXPECT_EQ(inputRegisterCountForProfile(Profile::MonofAapV1), 47);
}

TEST(IngeconModbusRtuTest, ParsesAas1340ProfileAlias) {
  Profile profile = Profile::Auto;
  ASSERT_TRUE(parseProfileName("aas1340_u", &profile));
  EXPECT_EQ(profile, Profile::TrifAasV1);
}

TEST(IngeconModbusRtuTest, ParsesMonofAapV1OnlineRegisters) {
  std::vector<uint16_t> registers(47);
  registers[0] = 0;
  registers[1] = 123;
  registers[2] = 0;
  registers[3] = 456;
  registers[4] = 0;
  registers[5] = 7;
  registers[8] = 0x0001;
  registers[9] = 0x0002;
  registers[10] = 0x20C4;
  registers[11] = 311;
  registers[12] = 523;
  registers[13] = 620;
  registers[14] = 812;
  registers[15] = 2345;
  registers[16] = 998;
  registers[17] = 1;
  registers[18] = 230;
  registers[19] = 5001;
  registers[20] = 2026;
  registers[21] = 6;
  registers[22] = 13;
  registers[23] = 12;
  registers[24] = 34;
  registers[25] = 56;

  Readings readings;
  ASSERT_TRUE(parseInputRegistersForProfile(
      Profile::MonofAapV1, registers, &readings));

  EXPECT_EQ(readings.profile, Profile::MonofAapV1);
  EXPECT_EQ(readings.totalEnergyKwh, 123u);
  EXPECT_EQ(readings.hoursRunning, 456u);
  EXPECT_EQ(readings.gridConnections, 7u);
  EXPECT_EQ(readings.alarmInverter, 1u);
  EXPECT_EQ(readings.alarmSafety, 2u);
  EXPECT_EQ(readings.status1, 0x20C4u);
  EXPECT_EQ(readings.vdc, 311u);
  EXPECT_EQ(readings.idc, 5u);
  EXPECT_EQ(readings.vbus, 620u);
  EXPECT_EQ(readings.iac, 8u);
  EXPECT_EQ(readings.pac, 2345);
  EXPECT_EQ(readings.cosPhi, 998u);
  EXPECT_EQ(readings.vac, 230u);
  EXPECT_EQ(readings.fac, 5001u);
  EXPECT_EQ(readings.year, 2026u);
  EXPECT_EQ(readings.second, 56u);
}

TEST(IngeconModbusRtuTest, ParsesTrifAasV1OnlineRegisters) {
  std::vector<uint16_t> registers(47);
  registers[0] = 0;
  registers[1] = 321;
  registers[6] = 0x0003;
  registers[7] = 0x0004;
  registers[8] = 612;
  registers[9] = 8;
  registers[10] = 229;
  registers[11] = 231;
  registers[12] = 232;
  registers[13] = 5;
  registers[14] = 6;
  registers[15] = 5;
  registers[16] = 997;
  registers[17] = 1;
  registers[18] = 465;
  registers[19] = 4999;
  registers[20] = 2026;
  registers[21] = 6;
  registers[22] = 13;

  Readings readings;
  ASSERT_TRUE(parseInputRegistersForProfile(
      Profile::TrifAasV1, registers, &readings));

  EXPECT_EQ(readings.profile, Profile::TrifAasV1);
  EXPECT_EQ(readings.totalEnergyKwh, 321u);
  EXPECT_EQ(readings.alarmInverter, 3u);
  EXPECT_EQ(readings.alarmSafety, 4u);
  EXPECT_EQ(readings.vdc, 612u);
  EXPECT_EQ(readings.idc, 8u);
  EXPECT_EQ(readings.vac, 229u);
  EXPECT_EQ(readings.vac2, 231u);
  EXPECT_EQ(readings.vac3, 232u);
  EXPECT_EQ(readings.iac, 5u);
  EXPECT_EQ(readings.iac2, 6u);
  EXPECT_EQ(readings.iac3, 5u);
  EXPECT_EQ(readings.pac, 4650);
  EXPECT_EQ(readings.fac, 4999u);
}

TEST(IngeconModbusRtuTest, ParsesTrifAasSignedDecaWattPower) {
  std::vector<uint16_t> registers(47);
  registers[18] = static_cast<uint16_t>(-123);

  Readings readings;
  ASSERT_TRUE(parseInputRegistersForProfile(
      Profile::TrifAasV1, registers, &readings));

  EXPECT_EQ(readings.pac, -1230);
}

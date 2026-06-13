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
using Supla::Linux::Ingecon::buildReadInputRegistersRequest;
using Supla::Linux::Ingecon::parseMainInputRegisters;
using Supla::Linux::Ingecon::parseReadInputRegistersResponse;

TEST(IngeconModbusRtuTest, BuildsReadInputRegistersRequest) {
  const auto request = buildReadInputRegistersRequest(1, 0, 27);

  const std::vector<uint8_t> expected{
      0x01, 0x04, 0x00, 0x00, 0x00, 0x1B, 0xB0, 0x01};
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

/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include <gtest/gtest.h>
#include <supla/sensor/socomec_countis.h>

#include <vector>

using Supla::Socomec::Model;

using Supla::Socomec::ParseResult;

namespace {
std::vector<uint8_t> withCrc(std::vector<uint8_t> frame) {
  const uint16_t crc = Supla::Socomec::crc16(frame.data(), frame.size());
  frame.push_back(crc & 0xFF);
  frame.push_back(crc >> 8);
  return frame;
}
}  // namespace

TEST(SocomecProtocolTests, BuildsReadHoldingRequest) {
  uint8_t frame[8];
  Supla::Socomec::buildReadHolding(2, 50520, 48, frame);
  // 50520 = 0xC558, 48 = 0x0030
  const auto expected = withCrc({0x02, 0x03, 0xC5, 0x58, 0x00, 0x30});
  EXPECT_EQ(std::vector<uint8_t>(frame, frame + 8), expected);
}

TEST(SocomecProtocolTests, KnownCrcVector) {
  // Standard Modbus example: 01 03 00 00 00 0A -> CRC C5 CD
  const uint8_t req[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};
  EXPECT_EQ(Supla::Socomec::crc16(req, sizeof(req)), 0xCDC5);
}

TEST(SocomecProtocolTests, ParsesOkReply) {
  const auto frame = withCrc({0x02, 0x03, 0x04, 0x00, 0x00, 0x5D, 0x53});
  uint16_t regs[2] = {};
  EXPECT_EQ(Supla::Socomec::parseReadHolding(frame.data(), frame.size(), 2, 2,
                                             regs),
            ParseResult::Ok);
  EXPECT_EQ(regs[0], 0x0000);
  EXPECT_EQ(regs[1], 0x5D53);  // 23891 = 238.91 V
}

TEST(SocomecProtocolTests, PartialReplyIsIncomplete) {
  const auto frame = withCrc({0x02, 0x03, 0x04, 0x00, 0x00, 0x5D, 0x53});
  uint16_t regs[2] = {};
  EXPECT_EQ(Supla::Socomec::parseReadHolding(frame.data(), 4, 2, 2, regs),
            ParseResult::Incomplete);
  EXPECT_EQ(Supla::Socomec::parseReadHolding(frame.data(), 6, 2, 2, regs),
            ParseResult::Incomplete);
}

TEST(SocomecProtocolTests, ParsesExceptionReply) {
  const auto frame = withCrc({0x02, 0x83, 0x02});
  uint16_t regs[2] = {};
  EXPECT_EQ(Supla::Socomec::parseReadHolding(frame.data(), frame.size(), 2, 2,
                                             regs),
            ParseResult::Exception);
}

TEST(SocomecProtocolTests, RejectsBadCrcWrongSlaveAndWrongCount) {
  auto frame = withCrc({0x02, 0x03, 0x04, 0x00, 0x00, 0x5D, 0x53});
  uint16_t regs[2] = {};
  auto bad = frame;
  bad.back() ^= 0xFF;
  EXPECT_EQ(Supla::Socomec::parseReadHolding(bad.data(), bad.size(), 2, 2,
                                             regs),
            ParseResult::Bad);
  EXPECT_EQ(Supla::Socomec::parseReadHolding(frame.data(), frame.size(), 3, 2,
                                             regs),
            ParseResult::Bad);
  EXPECT_EQ(Supla::Socomec::parseReadHolding(frame.data(), frame.size(), 2, 3,
                                             regs),
            ParseResult::Bad);
}

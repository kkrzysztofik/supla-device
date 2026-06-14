/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include <cstring>
#include <vector>

#include <gtest/gtest.h>

#include "sma_cinfo_parser.h"
#include "sma_channel_codec.h"
#include "sma_types.h"

using Supla::Linux::Sma::SmaChannelCodec;
using Supla::Linux::Sma::SmaCinfoParser;
using Supla::Linux::Sma::kChAnalog;
using Supla::Linux::Sma::kChIn;
using Supla::Linux::Sma::kChSpot;
using Supla::Linux::Sma::kNtypeDword;
using Supla::Linux::Sma::kNtypeWord;
using Supla::Linux::Sma::trimSmaChannelName;

namespace {

void appendLe16(std::vector<uint8_t>& buf, uint16_t value) {
  buf.push_back(static_cast<uint8_t>(value & 0xff));
  buf.push_back(static_cast<uint8_t>((value >> 8) & 0xff));
}

void appendLe32f(std::vector<uint8_t>& buf, float value) {
  uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  buf.push_back(static_cast<uint8_t>(bits & 0xff));
  buf.push_back(static_cast<uint8_t>((bits >> 8) & 0xff));
  buf.push_back(static_cast<uint8_t>((bits >> 16) & 0xff));
  buf.push_back(static_cast<uint8_t>((bits >> 24) & 0xff));
}

}  // namespace

TEST(SmaCinfoParserTest, TrimsPaddedChannelName) {
  EXPECT_EQ(trimSmaChannelName("             Pac"), "Pac");
  EXPECT_EQ(trimSmaChannelName("E-Total         "), "E-Total");
}

TEST(SmaCinfoParserTest, ParsesAnalogSpotChannel) {
  std::vector<uint8_t> buf;
  const uint16_t ctype = kChSpot | kChIn | kChAnalog;
  buf.push_back(0x50);
  appendLe16(buf, ctype);
  appendLe16(buf, kNtypeWord);
  appendLe16(buf, 0);
  const char name[] = "Pac             ";
  buf.insert(buf.end(), name, name + 16);
  const char unit[] = "W       ";
  buf.insert(buf.end(), unit, unit + 8);
  appendLe32f(buf, 1.0f);
  appendLe32f(buf, 0.0f);

  const auto parsed = SmaCinfoParser::parse(buf.data(), buf.size());
  ASSERT_TRUE(parsed.has_value());
  ASSERT_EQ(parsed->size(), 1u);
  EXPECT_EQ((*parsed)[0].name, "Pac");
  EXPECT_EQ((*parsed)[0].descriptor.ctype, ctype);
  EXPECT_EQ((*parsed)[0].descriptor.cindex, 0x50);
  EXPECT_EQ((*parsed)[0].descriptor.ntype, kNtypeWord);
  EXPECT_FLOAT_EQ((*parsed)[0].descriptor.gain, 1.0f);
}

TEST(SmaCinfoParserTest, FindByName) {
  std::vector<uint8_t> buf;
  const uint16_t ctype = kChSpot | kChIn | kChAnalog;
  buf.push_back(0x64);
  appendLe16(buf, ctype);
  appendLe16(buf, kNtypeDword);
  appendLe16(buf, 0);
  const char name[] = "E-Total         ";
  buf.insert(buf.end(), name, name + 16);
  const char unit[] = "kWh     ";
  buf.insert(buf.end(), unit, unit + 8);
  appendLe32f(buf, 0.001f);
  appendLe32f(buf, 0.0f);

  const auto parsed = SmaCinfoParser::parse(buf.data(), buf.size());
  ASSERT_TRUE(parsed.has_value());
  const auto* found = SmaCinfoParser::findByName(*parsed, "E-Total");
  ASSERT_NE(found, nullptr);
  EXPECT_EQ(found->descriptor.cindex, 0x64);
  EXPECT_FLOAT_EQ(found->descriptor.gain, 0.001f);
}

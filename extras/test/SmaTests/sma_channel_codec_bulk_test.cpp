/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include <gtest/gtest.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "sma_channel_codec.h"
#include "sma_types.h"

using Supla::Linux::Sma::kChAnalog;
using Supla::Linux::Sma::kChCounter;
using Supla::Linux::Sma::kChIn;
using Supla::Linux::Sma::kChSpot;
using Supla::Linux::Sma::kChSpotOnlineMask;
using Supla::Linux::Sma::kNtypeDword;
using Supla::Linux::Sma::kNtypeWord;
using Supla::Linux::Sma::SmaChannelCodec;
using Supla::Linux::Sma::SmaChannelInfo;

namespace {

void appendLe16(std::vector<uint8_t>* buf, uint16_t value) {
  buf->push_back(static_cast<uint8_t>(value & 0xff));
  buf->push_back(static_cast<uint8_t>((value >> 8) & 0xff));
}

void appendLe32(std::vector<uint8_t>* buf, uint32_t value) {
  buf->push_back(static_cast<uint8_t>(value & 0xff));
  buf->push_back(static_cast<uint8_t>((value >> 8) & 0xff));
  buf->push_back(static_cast<uint8_t>((value >> 16) & 0xff));
  buf->push_back(static_cast<uint8_t>((value >> 24) & 0xff));
}

SmaChannelInfo makeCatalogChannel(const char* name,
                                  uint8_t cindex,
                                  uint16_t ctype,
                                  uint16_t ntype,
                                  float gain,
                                  float offset = 0.0f) {
  SmaChannelInfo info;
  info.name = name;
  info.descriptor.cindex = cindex;
  info.descriptor.ctype = ctype;
  info.descriptor.ntype = ntype;
  info.descriptor.gain = gain;
  info.descriptor.offset = offset;
  return info;
}

std::vector<SmaChannelInfo> sampleCatalog() {
  return {
      makeCatalogChannel(
          "Pac", 1, kChSpot | kChIn | kChAnalog, kNtypeWord, 1.0f),
      makeCatalogChannel(
          "Uac", 2, kChSpot | kChIn | kChAnalog, kNtypeWord, 0.1f),
      makeCatalogChannel(
          "E-Total", 3, kChSpot | kChIn | kChCounter, kNtypeDword, 0.001f),
  };
}

std::vector<uint8_t> makeBulkPayload(bool includeCounter) {
  std::vector<uint8_t> payload;
  appendLe16(&payload, kChSpotOnlineMask);
  payload.push_back(0);        // all matching channel indexes
  appendLe16(&payload, 1);     // dataset count
  appendLe32(&payload, 0);     // time
  appendLe32(&payload, 0);     // timebase
  appendLe16(&payload, 500);   // Pac
  appendLe16(&payload, 2301);  // Uac -> 230.1
  if (includeCounter) {
    appendLe32(&payload, 123456);  // E-Total -> 123.456
  }
  return payload;
}

}  // namespace

TEST(SmaChannelCodecBulkTest, ParsesBulkSpotValuesByNameInCatalogOrder) {
  const auto payload = makeBulkPayload(true);
  std::map<std::string, double> values;

  ASSERT_TRUE(SmaChannelCodec::parseBulkSpotValuesByName(
      payload.data(), payload.size(), sampleCatalog(), &values));

  ASSERT_EQ(values.size(), 3u);
  EXPECT_DOUBLE_EQ(values["Pac"], 500.0);
  EXPECT_NEAR(values["Uac"], 230.1, 0.0001);
  EXPECT_NEAR(values["E-Total"], 123.456, 0.0001);
}

TEST(SmaChannelCodecBulkTest, KeepsDecodedValuesWhenBulkPayloadStopsEarly) {
  const auto payload = makeBulkPayload(false);
  std::map<std::string, double> values;

  ASSERT_TRUE(SmaChannelCodec::parseBulkSpotValuesByName(
      payload.data(), payload.size(), sampleCatalog(), &values));

  ASSERT_EQ(values.size(), 2u);
  EXPECT_DOUBLE_EQ(values["Pac"], 500.0);
  EXPECT_NEAR(values["Uac"], 230.1, 0.0001);
  EXPECT_EQ(values.count("E-Total"), 0u);
}

TEST(SmaChannelCodecBulkTest, RejectsPayloadThatEndsBeforeFirstValue) {
  std::vector<uint8_t> payload;
  appendLe16(&payload, kChSpotOnlineMask);
  payload.push_back(0);
  appendLe16(&payload, 1);
  appendLe32(&payload, 0);
  appendLe32(&payload, 0);

  std::map<std::string, double> values;
  EXPECT_FALSE(SmaChannelCodec::parseBulkSpotValuesByName(
      payload.data(), payload.size(), sampleCatalog(), &values));
}

TEST(SmaChannelCodecBulkTest, AppliesAnalogOffsetButNotCounterOffset) {
  Supla::Linux::Sma::SmaChannelDescriptor analog;
  analog.ctype = kChSpot | kChIn | kChAnalog;
  analog.gain = 0.5f;
  analog.offset = 10.0f;

  Supla::Linux::Sma::SmaChannelDescriptor counter;
  counter.ctype = kChSpot | kChIn | kChCounter;
  counter.gain = 0.5f;
  counter.offset = 10.0f;

  EXPECT_DOUBLE_EQ(SmaChannelCodec::applyGainOffset(100.0, analog), 60.0);
  EXPECT_DOUBLE_EQ(SmaChannelCodec::applyGainOffset(100.0, counter), 50.0);
}

TEST(SmaChannelCodecBulkTest, SpotOnlineMaskMatchesAnalogAndCounterChannels) {
  const auto catalog = sampleCatalog();

  EXPECT_TRUE(SmaChannelCodec::channelMatchesFilter(
      catalog[0].descriptor, kChSpotOnlineMask, 0));
  EXPECT_TRUE(SmaChannelCodec::channelMatchesFilter(
      catalog[2].descriptor, kChSpotOnlineMask, 0));
  EXPECT_FALSE(SmaChannelCodec::channelMatchesFilter(
      catalog[0].descriptor, kChSpotOnlineMask, 99));
}

/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include "sma_profile_loader.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "sma_cinfo_parser.h"
#include "sma_types.h"

using Supla::Linux::Sma::kChAnalog;
using Supla::Linux::Sma::kChIn;
using Supla::Linux::Sma::kChSpot;
using Supla::Linux::Sma::kNtypeWord;
using Supla::Linux::Sma::SmaCinfoParser;
using Supla::Linux::Sma::SmaProfileLoader;

namespace {

void appendLe16(std::vector<uint8_t>* buf, uint16_t value) {
  buf->push_back(static_cast<uint8_t>(value & 0xff));
  buf->push_back(static_cast<uint8_t>((value >> 8) & 0xff));
}

void appendLe32f(std::vector<uint8_t>* buf, float value) {
  uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  buf->push_back(static_cast<uint8_t>(bits & 0xff));
  buf->push_back(static_cast<uint8_t>((bits >> 8) & 0xff));
  buf->push_back(static_cast<uint8_t>((bits >> 16) & 0xff));
  buf->push_back(static_cast<uint8_t>((bits >> 24) & 0xff));
}

std::vector<uint8_t> makeYasdiCacheFile(uint8_t version) {
  std::vector<uint8_t> buf;
  buf.push_back(version);
  buf.push_back(0x07);
  appendLe16(&buf, kChSpot | kChIn | kChAnalog);
  appendLe16(&buf, kNtypeWord);
  appendLe16(&buf, 0);
  const char name[] = "Pac             ";
  buf.insert(buf.end(), name, name + 16);
  const char unit[] = "W       ";
  buf.insert(buf.end(), unit, unit + 8);
  appendLe32f(&buf, 1.0f);
  appendLe32f(&buf, 0.0f);
  return buf;
}

std::string writeTempProfile(const std::vector<uint8_t>& content,
                             const char* suffix) {
  const std::string path =
      std::string("/tmp/supla-sma-profile-test-") + suffix + ".bin";
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  file.write(reinterpret_cast<const char*>(content.data()), content.size());
  return path;
}

}  // namespace

TEST(SmaProfileLoaderTest, LoadsYasdiBinCacheWithVersionByte) {
  const std::string path = writeTempProfile(makeYasdiCacheFile(10), "valid");

  const auto catalog = SmaProfileLoader::loadYasdiBinFile(path);
  std::remove(path.c_str());

  ASSERT_TRUE(catalog.has_value());
  ASSERT_EQ(catalog->size(), 1u);
  EXPECT_EQ((*catalog)[0].name, "Pac");
  EXPECT_EQ((*catalog)[0].descriptor.cindex, 0x07);
  EXPECT_FLOAT_EQ((*catalog)[0].descriptor.gain, 1.0f);
}

TEST(SmaProfileLoaderTest, RejectsUnsupportedYasdiCacheVersion) {
  const std::string path =
      writeTempProfile(makeYasdiCacheFile(9), "unsupported");

  const auto catalog = SmaProfileLoader::loadYasdiBinFile(path);
  std::remove(path.c_str());

  EXPECT_FALSE(catalog.has_value());
}

TEST(SmaProfileLoaderTest, ResolvesBuiltInWr33ProfileWithTrimmedType) {
  const auto catalog = SmaProfileLoader::resolveProfile("  WR33 008   ");

  ASSERT_TRUE(catalog.has_value());
  EXPECT_GT(catalog->size(), 30u);
  const auto* pac = SmaCinfoParser::findByName(*catalog, "Pac");
  ASSERT_NE(pac, nullptr);
  EXPECT_EQ(pac->descriptor.ntype, kNtypeWord);
  const auto* eTotal = SmaCinfoParser::findByName(*catalog, "E-Total");
  ASSERT_NE(eTotal, nullptr);
  EXPECT_FLOAT_EQ(eTotal->descriptor.gain, 0.001f);
}

TEST(SmaProfileLoaderTest, ReturnsNulloptForUnknownProfile) {
  EXPECT_FALSE(
      SmaProfileLoader::resolveProfile("UNKNOWN-SMA-PROFILE").has_value());
}

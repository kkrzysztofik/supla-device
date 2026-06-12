/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include "sma_profile_loader.h"

#include <gtest/gtest.h>

#include "sma_cinfo_parser.h"
#include "sma_types.h"

using Supla::Linux::Sma::kNtypeWord;
using Supla::Linux::Sma::SmaCinfoParser;
using Supla::Linux::Sma::SmaProfileLoader;

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

TEST(SmaProfileLoaderTest, DoesNotResolveYasdiBinProfileNames) {
  EXPECT_FALSE(SmaProfileLoader::resolveProfile("WR33-008.bin").has_value());
  EXPECT_FALSE(
      SmaProfileLoader::resolveProfile("/tmp/WR33-008.bin").has_value());
}

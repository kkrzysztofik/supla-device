/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include "sma_channel_helpers.h"

#include <gtest/gtest.h>

using Supla::Linux::Sma::kDefaultPollIntervalSec;
using Supla::Linux::Sma::SerialMedia;
using Supla::PV::makeSmaBusConfig;
using Supla::PV::normalizeSmaPollIntervalSec;
using Supla::PV::smaMappingIs;

TEST(SmaChannelHelpersTest, NormalizesInvalidPollIntervalToDefault) {
  EXPECT_EQ(normalizeSmaPollIntervalSec(0), kDefaultPollIntervalSec);
  EXPECT_EQ(normalizeSmaPollIntervalSec(-1), kDefaultPollIntervalSec);
  EXPECT_EQ(normalizeSmaPollIntervalSec(15), 15);
}

TEST(SmaChannelHelpersTest, BuildsBusConfigWithNormalizedPollInterval) {
  const auto config = makeSmaBusConfig(
      "/dev/ttyUSB0", 19200, SerialMedia::RS232, 7, 0, "WR33-008");

  EXPECT_EQ(config.serialDevice, "/dev/ttyUSB0");
  EXPECT_EQ(config.baud, 19200);
  EXPECT_EQ(config.media, SerialMedia::RS232);
  EXPECT_EQ(config.netAddress, 7);
  EXPECT_EQ(config.pollIntervalSec, kDefaultPollIntervalSec);
  EXPECT_EQ(config.deviceProfile, "WR33-008");
}

TEST(SmaChannelHelpersTest, ComparesNullableMappingsSafely) {
  EXPECT_TRUE(smaMappingIs("power_active", "power_active"));
  EXPECT_FALSE(smaMappingIs("power_active", "voltage"));
  EXPECT_FALSE(smaMappingIs(nullptr, "voltage"));
  EXPECT_FALSE(smaMappingIs("voltage", nullptr));
}

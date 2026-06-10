/*
 Copyright (C) AC SOFTWARE SP. Z O.O.
*/

#include <gtest/gtest.h>

#include "sma_channel_codec.h"
#include "smanet_framer.h"
#include "sma_types.h"

using Supla::Linux::Sma::SmaChannelCodec;
using Supla::Linux::Sma::SmaNetFramer;
using Supla::Linux::Sma::kProtPppSmadata1;

TEST(SmaNetFramerTest, FcsIsStable) {
  const uint8_t payload[] = {0xff, 0x03, 0x40, 0x41, 0x01, 0x00, 0x00, 0x00,
                             0x0a, 0x12, 0x34, 0x56, 0x78};
  const uint16_t checksum1 =
      SmaNetFramer::calcChecksum(payload, sizeof(payload));
  const uint16_t checksum2 =
      SmaNetFramer::calcChecksum(payload, sizeof(payload));
  EXPECT_EQ(checksum1, checksum2);
  EXPECT_NE(checksum1, 0xffff);
}

TEST(SmaNetFramerTest, EncapsulateAndDecode) {
  const uint8_t smadata[] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x0b,
                             0x01, 0x08, 0x00};
  SmaNetFramer framer;
  const auto wire =
      framer.encapsulate(kProtPppSmadata1, smadata, sizeof(smadata));

  SmaNetFramer rx;
  for (uint8_t byte : wire) {
    rx.feed(byte);
    if (auto frame = rx.takeFrame()) {
      EXPECT_EQ(frame->protocolId, kProtPppSmadata1);
      ASSERT_EQ(frame->payload.size(), sizeof(smadata));
      EXPECT_EQ(0, memcmp(frame->payload.data(), smadata, sizeof(smadata)));
      return;
    }
  }
  FAIL() << "No SMANet frame decoded";
}

TEST(SmaChannelCodecTest, ParseFloatSpotChannel) {
  uint8_t data[] = {
      0x01, 0x08,  // ctype CH_SPOT|CH_IN|CH_ANALOG
      0x00,        // cindex
      0x01, 0x00,  // dataset count
      0x00, 0x00, 0x00, 0x00,  // time
      0x00, 0x00, 0x00, 0x00,  // timebase
      0x00, 0x00, 0xfa, 0x43,  // float 500.0
  };

  Supla::Linux::Sma::SmaChannelDescriptor channel;
  channel.ctype = 0x0801;
  channel.cindex = 0;
  channel.ntype = 0x0004;
  channel.gain = 1.0f;
  channel.offset = 0.0f;

  double value = 0.0;
  ASSERT_TRUE(SmaChannelCodec::parseGetDataValue(
      data, sizeof(data), channel, &value));
  EXPECT_NEAR(500.0, value, 0.01);
}

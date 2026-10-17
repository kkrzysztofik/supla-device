/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include "sma_bus.h"

#include <gtest/gtest.h>

#include <memory>
#include <vector>

using Supla::Linux::Sma::SerialMedia;
using Supla::Linux::Sma::SmaBus;
using Supla::Linux::Sma::SmaBusConfig;

TEST(SmaBusTest, AcquireReusesBusForSameSerialConfiguration) {
  SmaBusConfig config;
  config.serialDevice = "/tmp/supla-sma-bus-test-shared";
  config.baud = 9600;
  config.media = SerialMedia::RS485;
  config.netAddress = 1;

  auto first = SmaBus::acquire(config);
  auto second = SmaBus::acquire(config);

  ASSERT_NE(first, nullptr);
  EXPECT_EQ(first, second);
}

TEST(SmaBusTest, AcquireSeparatesBusesByNetAddress) {
  SmaBusConfig firstConfig;
  firstConfig.serialDevice = "/tmp/supla-sma-bus-test-net-a";
  firstConfig.baud = 9600;
  firstConfig.media = SerialMedia::RS485;
  firstConfig.netAddress = 1;

  auto secondConfig = firstConfig;
  secondConfig.netAddress = 2;

  auto first = SmaBus::acquire(firstConfig);
  auto second = SmaBus::acquire(secondConfig);

  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  EXPECT_NE(first, second);
}

TEST(SmaBusTest, InvalidateCachedReadingsMarksCacheInvalidButKeepsValues) {
  auto state = std::make_shared<SmaBus::Subscriber::State>();
  state->cacheValid = true;
  state->valuesByKey["Pac"] = 1234.5;
  state->valuesByKey["E-Total"] = 6789.0;

  SmaBus::Subscriber subscriber;
  subscriber.state = state;
  std::vector<SmaBus::Subscriber> subscribers{subscriber};

  SmaBus::invalidateCachedReadingsForTest(subscribers);

  EXPECT_FALSE(state->cacheValid);
  ASSERT_EQ(state->valuesByKey.size(), 2);
  EXPECT_DOUBLE_EQ(state->valuesByKey["Pac"], 1234.5);
  EXPECT_DOUBLE_EQ(state->valuesByKey["E-Total"], 6789.0);
}

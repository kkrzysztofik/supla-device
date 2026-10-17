/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include "ingecon_bus.h"

#include <gtest/gtest.h>

#include <memory>
#include <vector>

using Supla::Linux::Ingecon::Bus;
using Supla::Linux::Ingecon::BusConfig;

TEST(IngeconBusTest, AcquireReusesBusForSameSerialConfiguration) {
  BusConfig config;
  config.serialDevice = "/tmp/supla-ingecon-bus-test-shared";
  config.baud = 9600;
  config.modbusAddress = 1;

  auto first = Bus::acquire(config);
  auto second = Bus::acquire(config);

  ASSERT_NE(first, nullptr);
  EXPECT_EQ(first, second);
}

TEST(IngeconBusTest, AcquireSeparatesBusesByModbusAddress) {
  BusConfig firstConfig;
  firstConfig.serialDevice = "/tmp/supla-ingecon-bus-test-address-a";
  firstConfig.baud = 9600;
  firstConfig.modbusAddress = 1;

  auto secondConfig = firstConfig;
  secondConfig.modbusAddress = 2;

  auto first = Bus::acquire(firstConfig);
  auto second = Bus::acquire(secondConfig);

  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  EXPECT_NE(first, second);
}

TEST(IngeconBusTest, InvalidateCachedReadingsMarksCacheInvalidButKeepsValues) {
  auto state = std::make_shared<Bus::Subscriber::State>();
  state->cacheValid = true;
  state->readings.pac = 1234;
  state->readings.alarms = 99;

  Bus::Subscriber subscriber;
  subscriber.state = state;
  std::vector<Bus::Subscriber> subscribers{subscriber};

  Bus::invalidateCachedReadingsForTest(subscribers);

  EXPECT_FALSE(state->cacheValid);
  EXPECT_EQ(state->readings.pac, 1234);
  EXPECT_EQ(state->readings.alarms, 99u);
}

TEST(IngeconBusTest, EnergyFilterConfirmsFirstReadingBeforePublish) {
  using Supla::Linux::Ingecon::EnergyFilter;
  using Supla::Linux::Ingecon::filterTotalEnergyKwh;

  EnergyFilter filter;
  // The first reading alone is never published.
  EXPECT_FALSE(filterTotalEnergyKwh(&filter, 58480u).has_value());
  // The second, consistent reading confirms it.
  const auto confirmed = filterTotalEnergyKwh(&filter, 58481u);
  ASSERT_TRUE(confirmed.has_value());
  EXPECT_EQ(*confirmed, 58481u);
  // Steady state keeps publishing.
  const auto next = filterTotalEnergyKwh(&filter, 58482u);
  ASSERT_TRUE(next.has_value());
  EXPECT_EQ(*next, 58482u);
}

TEST(IngeconBusTest, EnergyFilterGarbageFirstReadingIsNeverPublished) {
  using Supla::Linux::Ingecon::EnergyFilter;
  using Supla::Linux::Ingecon::filterTotalEnergyKwh;

  EnergyFilter filter;
  // Garbage as the very first reading (0x8080E470 seen on AAS1340_U) is held
  // back and never reaches cloud history.
  EXPECT_FALSE(filterTotalEnergyKwh(&filter, 0x8080E470u).has_value());
  // A real reading disagrees with it, so it becomes the new candidate and is
  // still not published.
  EXPECT_FALSE(filterTotalEnergyKwh(&filter, 58480u).has_value());
  // A second real reading confirms the real value.
  const auto published = filterTotalEnergyKwh(&filter, 58480u);
  ASSERT_TRUE(published.has_value());
  EXPECT_EQ(*published, 58480u);
}

TEST(IngeconBusTest, EnergyFilterRejectsSpikeAndConfirmsReset) {
  using Supla::Linux::Ingecon::EnergyFilter;
  using Supla::Linux::Ingecon::filterTotalEnergyKwh;

  EnergyFilter filter;
  // Establish a baseline (two consistent readings).
  EXPECT_FALSE(filterTotalEnergyKwh(&filter, 58480u).has_value());
  ASSERT_TRUE(filterTotalEnergyKwh(&filter, 58481u).has_value());

  // Garbage high word observed on AAS1340_U: one-off spike is held back.
  const auto held = filterTotalEnergyKwh(&filter, 0x8080E470u);
  ASSERT_TRUE(held.has_value());
  EXPECT_EQ(*held, 58481u);
  const auto again = filterTotalEnergyKwh(&filter, 58481u);
  ASSERT_TRUE(again.has_value());
  EXPECT_EQ(*again, 58481u);

  // Genuine counter reset is accepted once a second poll confirms it.
  const auto resetPending = filterTotalEnergyKwh(&filter, 10u);
  ASSERT_TRUE(resetPending.has_value());
  EXPECT_EQ(*resetPending, 58481u);
  const auto resetConfirmed = filterTotalEnergyKwh(&filter, 10u);
  ASSERT_TRUE(resetConfirmed.has_value());
  EXPECT_EQ(*resetConfirmed, 10u);
}

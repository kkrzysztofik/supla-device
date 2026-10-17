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

TEST(IngeconBusTest, EnergyFilterRejectsSpikeAndConfirmsReset) {
  using Supla::Linux::Ingecon::EnergyFilter;
  using Supla::Linux::Ingecon::filterTotalEnergyKwh;

  EnergyFilter filter;
  EXPECT_EQ(filterTotalEnergyKwh(&filter, 58480u), 58480u);
  EXPECT_EQ(filterTotalEnergyKwh(&filter, 58481u), 58481u);
  // Garbage high word observed on AAS1340_U: one-off spike is held back.
  EXPECT_EQ(filterTotalEnergyKwh(&filter, 0x8080E470u), 58481u);
  EXPECT_EQ(filterTotalEnergyKwh(&filter, 58481u), 58481u);
  // Genuine counter reset is accepted once a second poll confirms it.
  EXPECT_EQ(filterTotalEnergyKwh(&filter, 10u), 58481u);
  EXPECT_EQ(filterTotalEnergyKwh(&filter, 10u), 10u);

  // Garbage as the very first reading recovers after one confirmation.
  EnergyFilter fresh;
  EXPECT_EQ(filterTotalEnergyKwh(&fresh, 0x8080E470u), 0x8080E470u);
  EXPECT_EQ(filterTotalEnergyKwh(&fresh, 58480u), 0x8080E470u);
  EXPECT_EQ(filterTotalEnergyKwh(&fresh, 58480u), 58480u);
}

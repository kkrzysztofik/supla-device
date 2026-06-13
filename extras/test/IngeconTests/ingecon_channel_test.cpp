/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include "ingecon_inverter.h"
#include "ingecon_measurement.h"
#include "simple_time.h"

#include <gtest/gtest.h>

#include <cmath>

using Supla::Linux::Ingecon::BusConfig;
using Supla::Linux::Ingecon::Readings;
using Supla::PV::IngeconEnergyMapping;
using Supla::PV::IngeconInverter;
using Supla::PV::IngeconMeasurement;

namespace {

BusConfig testConfig() {
  BusConfig config;
  config.serialDevice = "/tmp/supla-ingecon-channel-test";
  config.pollIntervalSec = 15;
  return config;
}

Readings sampleReadings() {
  Readings readings;
  readings.totalEnergyKwh = 12;
  readings.vac = 230;
  readings.iac = 5;
  readings.pac = 1234;
  readings.fac = 5001;
  readings.cosPhi = 998;
  readings.alarms = 42;
  readings.vdc = 310;
  return readings;
}

}  // namespace

TEST(IngeconChannelTest, InverterMapsProductionAsNegativeReverseEnergy) {
  SimpleTime time;
  IngeconInverter inverter(testConfig(), IngeconEnergyMapping::Reverse);
  inverter.setReadingsForTest(sampleReadings(), true);

  inverter.applyReadingsForTest();

  EXPECT_EQ(inverter.getPowerActive(0), -123400000);
  EXPECT_EQ(inverter.getVoltage(0), 23000);
  EXPECT_EQ(inverter.getCurrent(0), 5000u);
  EXPECT_EQ(inverter.getFreq(), 5001);
  EXPECT_EQ(inverter.getPowerFactor(0), 998);
  EXPECT_EQ(inverter.getRvrActEnergy(0), 1200000u);
  EXPECT_EQ(inverter.getFwdActEnergy(0), 0u);
}

TEST(IngeconChannelTest, InverterCanMapEnergyForward) {
  SimpleTime time;
  IngeconInverter inverter(testConfig(), IngeconEnergyMapping::Forward);
  inverter.setReadingsForTest(sampleReadings(), true);

  inverter.applyReadingsForTest();

  EXPECT_EQ(inverter.getFwdActEnergy(0), 1200000u);
  EXPECT_EQ(inverter.getRvrActEnergy(0), 0u);
}

TEST(IngeconChannelTest, MeasurementReturnsSelectedValue) {
  IngeconMeasurement measurement(testConfig(), "alarms");
  measurement.setReadingsForTest(sampleReadings(), true);

  EXPECT_DOUBLE_EQ(measurement.getValue(), 42.0);
}

TEST(IngeconChannelTest, MeasurementReturnsNanWhenStale) {
  IngeconMeasurement measurement(testConfig(), "alarms");
  measurement.setReadingsForTest(sampleReadings(), false);

  measurement.getValue();
  measurement.getValue();
  measurement.getValue();

  EXPECT_TRUE(std::isnan(measurement.getValue()));
}

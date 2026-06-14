/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include "ingecon_dc_meter.h"
#include "ingecon_inverter.h"
#include "ingecon_measurement.h"
#include "simple_time.h"

#include <gtest/gtest.h>

#include <cmath>

using Supla::Linux::Ingecon::BusConfig;
using Supla::Linux::Ingecon::Readings;
using Supla::PV::IngeconDcMeter;
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
  readings.idc = 5;
  return readings;
}

Readings sampleThreePhaseReadings() {
  Readings readings;
  readings.profile = Supla::Linux::Ingecon::Profile::TrifAasV1;
  readings.totalEnergyKwh = 120;
  readings.vac = 229;
  readings.vac2 = 230;
  readings.vac3 = 231;
  readings.vdc = 474;
  readings.idc = 8;
  readings.iac = 5;
  readings.iac2 = 6;
  readings.iac3 = 7;
  readings.pac = 360;
  readings.fac = 5001;
  readings.cosPhi = 998;
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

TEST(IngeconChannelTest, DcMeterMapsVoltageCurrentAndComputedPower) {
  SimpleTime time;
  IngeconDcMeter meter(testConfig());
  meter.setReadingsForTest(sampleReadings(), true);

  meter.applyReadingsForTest();

  EXPECT_EQ(meter.getVoltage(0), 31000);
  EXPECT_EQ(meter.getCurrent(0), 5000u);
  EXPECT_EQ(meter.getPowerActive(0), 155000000);
  EXPECT_EQ(meter.getFreq(), 0);
}

TEST(IngeconChannelTest, DcMeterMapsThreePhaseProfileDcValues) {
  SimpleTime time;
  auto config = testConfig();
  config.profile = Supla::Linux::Ingecon::Profile::TrifAasV1;
  IngeconDcMeter meter(config);
  meter.setReadingsForTest(sampleThreePhaseReadings(), true);

  meter.applyReadingsForTest();

  EXPECT_EQ(meter.getVoltage(0), 47400);
  EXPECT_EQ(meter.getCurrent(0), 8000u);
  EXPECT_EQ(meter.getPowerActive(0), 379200000);
}

TEST(IngeconChannelTest, InverterMapsThreePhaseValues) {
  SimpleTime time;
  auto config = testConfig();
  config.profile = Supla::Linux::Ingecon::Profile::TrifAasV1;
  IngeconInverter inverter(config, IngeconEnergyMapping::Reverse);
  inverter.setReadingsForTest(sampleThreePhaseReadings(), true);

  inverter.applyReadingsForTest();

  EXPECT_EQ(inverter.getPowerActive(0), -12000000);
  EXPECT_EQ(inverter.getPowerActive(1), -12000000);
  EXPECT_EQ(inverter.getPowerActive(2), -12000000);
  EXPECT_EQ(inverter.getVoltage(0), 22900);
  EXPECT_EQ(inverter.getVoltage(1), 23000);
  EXPECT_EQ(inverter.getVoltage(2), 23100);
  EXPECT_EQ(inverter.getCurrent(0), 5000u);
  EXPECT_EQ(inverter.getCurrent(1), 6000u);
  EXPECT_EQ(inverter.getCurrent(2), 7000u);
  EXPECT_EQ(inverter.getFreq(), 5001);
  EXPECT_EQ(inverter.getPowerFactor(0), 998);
  EXPECT_EQ(inverter.getPowerFactor(1), 998);
  EXPECT_EQ(inverter.getPowerFactor(2), 998);
  EXPECT_EQ(inverter.getRvrActEnergy(0), 4000000u);
  EXPECT_EQ(inverter.getRvrActEnergy(1), 4000000u);
  EXPECT_EQ(inverter.getRvrActEnergy(2), 4000000u);
}

TEST(IngeconChannelTest, MeasurementReturnsSelectedValue) {
  IngeconMeasurement measurement(testConfig(), "alarms");
  measurement.setReadingsForTest(sampleReadings(), true);

  EXPECT_DOUBLE_EQ(measurement.getValue(), 42.0);
}

TEST(IngeconChannelTest, MeasurementReturnsThreePhaseValues) {
  IngeconMeasurement voltage(testConfig(), "vac3");
  voltage.setReadingsForTest(sampleThreePhaseReadings(), true);
  EXPECT_DOUBLE_EQ(voltage.getValue(), 231.0);

  IngeconMeasurement current(testConfig(), "iac2");
  current.setReadingsForTest(sampleThreePhaseReadings(), true);
  EXPECT_DOUBLE_EQ(current.getValue(), 6.0);
}

TEST(IngeconChannelTest, MeasurementReturnsNanWhenStale) {
  IngeconMeasurement measurement(testConfig(), "alarms");
  measurement.setReadingsForTest(sampleReadings(), false);

  measurement.getValue();
  measurement.getValue();
  measurement.getValue();

  EXPECT_TRUE(std::isnan(measurement.getValue()));
}

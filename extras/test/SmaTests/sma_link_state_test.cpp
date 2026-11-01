/*
 Copyright (C) 2026 Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include <fcntl.h>
#include <gtest/gtest.h>
#include <stdlib.h>
#include <unistd.h>

#include <cmath>
#include <map>
#include <string>
#include <vector>

#include "sma_bus.h"
#include "sma_dc_meter.h"
#include "sma_inverter.h"
#include "sma_measurement.h"
#include "sma_serial_port.h"
#include "simple_time.h"
#include "smadata_client.h"

using Supla::Linux::Sma::LinkState;
using Supla::Linux::Sma::SerialMedia;
using Supla::Linux::Sma::SmaDataClient;
using Supla::Linux::Sma::SmaSerialPort;
using Supla::PV::SmaDcMeter;
using Supla::PV::SmaInverter;
using Supla::PV::SmaMappedChannel;
using Supla::PV::SmaMeasurement;

namespace {

struct Pty {
  Pty() {
    master = posix_openpt(O_RDWR | O_NOCTTY);
    if (master >= 0 && grantpt(master) == 0 && unlockpt(master) == 0) {
      slave = ptsname(master);
    }
  }
  ~Pty() {
    if (master >= 0) {
      close(master);
    }
  }
  int master = -1;
  std::string slave;
};

SmaMappedChannel mapped(const char* key, const char* mapping) {
  SmaMappedChannel channel;
  channel.key = key;
  channel.descriptor.suplaMapping = mapping;
  return channel;
}

std::vector<SmaMappedChannel> acChannels() {
  return {mapped("Pac", "power_active"), mapped("Uac", "voltage")};
}

const std::map<std::string, double> kAcValues = {{"Pac", 1500.0},
                                                 {"Uac", 230.0}};

template <typename Meter>
void applyStale(Meter* meter, LinkState link) {
  meter->setReadingsForTest(kAcValues, true, LinkState::Ok);
  meter->applyReadingsForTest();
  meter->setReadingsForTest(kAcValues, false, link);
  for (int i = 0; i < 4; ++i) {
    meter->applyReadingsForTest();
  }
}

}  // namespace

// Nobody answers on a pseudo-terminal: like the inverter asleep at night.
TEST(SmaLinkStateTest, SilentLineReceivesNothing) {
  Pty pty;
  ASSERT_FALSE(pty.slave.empty());
  SmaSerialPort port(pty.slave, 9600, SerialMedia::RS232);
  ASSERT_TRUE(port.open());

  SmaDataClient client(port, 0, 1);
  EXPECT_FALSE(client.detectDevice(200).has_value());
  EXPECT_TRUE(client.receivedNothing());
}

// Any byte back means the line is alive: a failure is then a link error.
TEST(SmaLinkStateTest, GarbageOnLineIsNotSilence) {
  Pty pty;
  ASSERT_FALSE(pty.slave.empty());
  SmaSerialPort port(pty.slave, 9600, SerialMedia::RS232);
  ASSERT_TRUE(port.open());
  const uint8_t garbage[] = {0x01, 0x02, 0x03};
  ASSERT_EQ(write(pty.master, garbage, sizeof(garbage)), 3);

  SmaDataClient client(port, 0, 1);
  EXPECT_FALSE(client.detectDevice(200).has_value());
  EXPECT_FALSE(client.receivedNothing());
}

// A read error (here: the other end vanished) is not silence either.
TEST(SmaLinkStateTest, ReadErrorIsNotSilence) {
  Pty pty;
  ASSERT_FALSE(pty.slave.empty());
  SmaSerialPort port(pty.slave, 9600, SerialMedia::RS232);
  ASSERT_TRUE(port.open());
  close(pty.master);
  pty.master = -1;

  SmaDataClient client(port, 0, 1);
  EXPECT_FALSE(client.detectDevice(200).has_value());
  EXPECT_FALSE(client.receivedNothing());
}

TEST(SmaLinkStateTest, InverterZeroesWhenSilent) {
  SimpleTime time;
  SmaInverter inverter("/tmp/supla-sma-link-test", 1200, SerialMedia::RS485,
                       1, 5, acChannels());
  applyStale(&inverter, LinkState::Silent);

  EXPECT_TRUE(inverter.hasMeasurementForTest());
  EXPECT_EQ(inverter.getPowerActive(0), 0);
  EXPECT_EQ(inverter.getVoltage(0), 0);
}

TEST(SmaLinkStateTest, InverterMeasurementUnavailableOnLinkError) {
  SimpleTime time;
  SmaInverter inverter("/tmp/supla-sma-link-test", 1200, SerialMedia::RS485,
                       1, 5, acChannels());
  applyStale(&inverter, LinkState::Error);

  EXPECT_FALSE(inverter.hasMeasurementForTest());
}

TEST(SmaLinkStateTest, DcMeterFollowsInverterRules) {
  SimpleTime time;
  SmaDcMeter silent("/tmp/supla-sma-link-test", 1200, SerialMedia::RS485, 1,
                    5, acChannels());
  applyStale(&silent, LinkState::Silent);
  EXPECT_TRUE(silent.hasMeasurementForTest());

  SmaDcMeter broken("/tmp/supla-sma-link-test", 1200, SerialMedia::RS485, 1,
                    5, acChannels());
  applyStale(&broken, LinkState::Error);
  EXPECT_FALSE(broken.hasMeasurementForTest());
}

TEST(SmaLinkStateTest, LinkMeasurementReportsPollResult) {
  // No sma_channels: the measurement reports the link state.
  SmaMeasurement link("/tmp/supla-sma-link-test", 1200, SerialMedia::RS485, 1,
                      5, {});
  EXPECT_TRUE(std::isnan(link.getValue()));  // no poll finished yet
  link.setReadingsForTest({}, true, LinkState::Ok);
  EXPECT_DOUBLE_EQ(link.getValue(), 1.0);
  link.setReadingsForTest({}, false, LinkState::Silent);
  EXPECT_DOUBLE_EQ(link.getValue(), 0.0);
  link.setReadingsForTest({}, false, LinkState::Error);
  EXPECT_DOUBLE_EQ(link.getValue(), -1.0);
}

TEST(SmaLinkStateTest, ImpedanceStaysUnknownWhenSilent) {
  SmaMeasurement zac("/tmp/supla-sma-link-test", 1200, SerialMedia::RS485, 1,
                     5, {mapped("Zac", "measurement")});
  zac.setReadingsForTest({{"Zac", 0.56}}, false, LinkState::Silent);
  for (int i = 0; i < 3; ++i) {
    zac.getValue();
  }
  EXPECT_TRUE(std::isnan(zac.getValue()));
}

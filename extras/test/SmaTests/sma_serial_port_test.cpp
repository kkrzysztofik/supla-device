/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include "sma_serial_port.h"

#include <gtest/gtest.h>

using Supla::Linux::Sma::SerialMedia;
using Supla::Linux::Sma::SmaSerialPort;

TEST(SmaSerialPortTest, ClosedPortRejectsInvalidReadAndWriteArguments) {
  SmaSerialPort port(
      "/tmp/supla-sma-test-missing-serial", 9600, SerialMedia::RS485);
  uint8_t byte = 0;

  EXPECT_FALSE(port.writeAll(nullptr, 1));
  EXPECT_FALSE(port.writeAll(&byte, 0));
  EXPECT_EQ(port.readSome(nullptr, 1, 1), -1);
  EXPECT_EQ(port.readSome(&byte, 0, 1), -1);
}

TEST(SmaSerialPortTest, OpenFailsForMissingDevicePath) {
  SmaSerialPort port(
      "/tmp/supla-sma-test-missing-serial", 9600, SerialMedia::RS485);

  EXPECT_FALSE(port.open());
  EXPECT_FALSE(port.isOpen());
}

/*
 Copyright (C) Krzysztof Krzysztofik
*/

#include "byd_bangcle.h"
#include "byd_measurement.h"

#include <gtest/gtest.h>

#include <filesystem>

using Supla::Linux::Byd::BangcleCodec;

TEST(BydBangcleTest, EncodeDecodeRoundTrip) {
  const std::filesystem::path tablesPath =
      std::filesystem::path(__FILE__)
          .parent_path()
          .parent_path()
          .parent_path()
          / "porting/linux/byd/data/bangcle_tables.bin";

  BangcleCodec codec(tablesPath.string());
  ASSERT_TRUE(codec.loadTables());

  const std::string plain = R"({"hello":"world","n":42})";
  const std::string encoded = codec.encodeEnvelope(plain);
  EXPECT_FALSE(encoded.empty());
  EXPECT_EQ(encoded.front(), 'F');

  const auto decodedBytes = codec.decodeEnvelope(encoded);
  ASSERT_FALSE(decodedBytes.empty());
  const std::string decoded(decodedBytes.begin(), decodedBytes.end());
  EXPECT_EQ(decoded, plain);
}

TEST(BydMeasurementTest, ReadsMappedFieldFromPoller) {
  Supla::Linux::Byd::BydAccountConfig account;
  account.username = "user@example.com";
  account.password = "secret";
  Supla::Linux::Byd::BydVehicleConfig vehicle;
  vehicle.vin = "TESTVIN1234567890";
  vehicle.pollIntervalSec = 60;

  Supla::PV::BydMeasurement measurement(account, vehicle, "soc");
  Supla::Linux::Byd::BydReadings readings;
  readings.valid = true;
  readings.values["soc"] = 55.0;
  measurement.setReadingsForTest(readings);

  EXPECT_DOUBLE_EQ(measurement.getValue(), 55.0);
}

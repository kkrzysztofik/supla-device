/*
 Copyright (C) Krzysztof Krzysztofik
*/

#include "byd_readings.h"

#include <gtest/gtest.h>

using Supla::Linux::Byd::BydReadingsBuilder;

TEST(BydReadingsTest, RealtimeMapsSocAndChargingFlags) {
  nlohmann::json realtime = {
      {"elecPercent", 72},
      {"enduranceMileage", 320},
      {"gl", -4500},
      {"chargeState", 1},
      {"onlineState", 1},
      {"connectState", 1},
      {"fullHour", 1},
      {"fullMinute", 15},
      {"totalMileage", 12345},
  };

  const auto readings = BydReadingsBuilder::fromSources(
      realtime, {}, {}, {}, {}, true, false, false, false, false);

  EXPECT_TRUE(readings.valid);
  EXPECT_DOUBLE_EQ(readings.values.at("soc"), 72);
  EXPECT_DOUBLE_EQ(readings.values.at("range_km"), 320);
  EXPECT_DOUBLE_EQ(readings.values.at("battery_power_w"), -4500);
  EXPECT_DOUBLE_EQ(readings.values.at("time_to_full_min"), 75);
  EXPECT_TRUE(readings.booleans.at("is_charging"));
  EXPECT_TRUE(readings.booleans.at("is_plugged"));
  EXPECT_TRUE(readings.booleans.at("is_online"));
}

TEST(BydReadingsTest, EnergyEndpointMapsAverageConsumption) {
  nlohmann::json energy = {
      {"nearestEnergyConsumption",
       {{"avgEvConsumption", 16.2}, {"evConsumption", 8.1}}}};

  const auto readings = BydReadingsBuilder::fromSources(
      {}, {}, {}, {}, energy, false, false, false, false, true);

  EXPECT_DOUBLE_EQ(readings.values.at("avg_consumption_ev"), 16.2);
  EXPECT_DOUBLE_EQ(readings.values.at("energy_last_50km_kwh"), 8.1);
}

TEST(BydReadingsTest, FieldAliasCanonicalizesSoc) {
  std::string canonical;
  EXPECT_TRUE(BydReadingsBuilder::parseFieldAlias("elec_percent", &canonical));
  EXPECT_EQ(canonical, "soc");
}

/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include "hat_model.h"

#include <gtest/gtest.h>
#include <SuplaDevice.h>

#include <optional>
#include <utility>
#include <vector>

using namespace Supla::Linux::DisplayHatMini;  // NOLINT

namespace {

std::vector<Page> samplePages() {
  Page values;
  values.items = {{0, Field::PowerActive, "Power", "W"},
                  {1, Field::Primary, "Pump", ""},
                  {2, Field::Primary, "Temp", "C"}};
  Page chart;
  chart.type = PageType::Chart;
  chart.items = {{0, Field::PowerActive, "", "W"}};
  return {values, chart};
}

bool onlyChannel1IsRelay(int channel) { return channel == 1; }

}  // namespace

TEST(HatModelNavigatorTest, PagesWrapBothWaysAndResetCursor) {
  auto pages = samplePages();
  Navigator nav(pages);
  nav.onClick(kX, onlyChannel1IsRelay);
  EXPECT_EQ(nav.cursor(), 1u);
  nav.onClick(kB, onlyChannel1IsRelay);
  EXPECT_EQ(nav.page(), 1u);
  EXPECT_EQ(nav.cursor(), 0u);
  nav.onClick(kB, onlyChannel1IsRelay);
  EXPECT_EQ(nav.page(), 0u);
  nav.onClick(kA, onlyChannel1IsRelay);
  EXPECT_EQ(nav.page(), 1u);
}

TEST(HatModelNavigatorTest, CursorWrapsOnValuesPage) {
  auto pages = samplePages();
  Navigator nav(pages);
  for (int i = 0; i < 3; i++) {
    nav.onClick(kX, onlyChannel1IsRelay);
  }
  EXPECT_EQ(nav.cursor(), 0u);
}

TEST(HatModelNavigatorTest, YTogglesOnlyRelayItems) {
  auto pages = samplePages();
  Navigator nav(pages);
  EXPECT_EQ(nav.onClick(kY, onlyChannel1IsRelay), -1);  // EM field
  nav.onClick(kX, onlyChannel1IsRelay);
  EXPECT_EQ(nav.onClick(kY, onlyChannel1IsRelay), 1);   // relay
  nav.onClick(kX, onlyChannel1IsRelay);
  EXPECT_EQ(nav.onClick(kY, onlyChannel1IsRelay), -1);  // thermometer
  nav.onClick(kB, onlyChannel1IsRelay);
  EXPECT_EQ(nav.onClick(kY, onlyChannel1IsRelay), -1);  // chart page
}

TEST(HatModelNavigatorTest, NoPagesIsSafe) {
  std::vector<Page> none;
  Navigator nav(none);
  EXPECT_EQ(nav.onClick(kB, onlyChannel1IsRelay), -1);
  EXPECT_EQ(nav.onClick(kY, onlyChannel1IsRelay), -1);
}

TEST(HatModelLedTest, StatusColours) {
  std::vector<Alert> none;
  ValueReader noValues = [](int, Field) { return std::optional<double>(); };
  EXPECT_EQ(evaluateLed(none, noValues, STATUS_REGISTERED_AND_READY).color,
            kGreen);
  auto connecting = evaluateLed(none, noValues, STATUS_REGISTER_IN_PROGRESS);
  EXPECT_EQ(connecting.color, kBlue);
  EXPECT_TRUE(connecting.blink);
  EXPECT_EQ(evaluateLed(none, noValues, STATUS_CONFIG_MODE).color, kMagenta);
  EXPECT_EQ(evaluateLed(none, noValues, STATUS_UNKNOWN_SERVER_ADDRESS).color,
            kRed);
}

TEST(HatModelLedTest, FirstMatchingAlertWinsAndMissingValuesSkip) {
  std::vector<Alert> alerts(3);
  alerts[0] = {5, Field::Primary, 10.0, std::nullopt, kCyan, false};  // ch 5
  alerts[1] = {0, Field::Primary, std::nullopt, 100.0, kYellow, true};
  alerts[2] = {0, Field::Primary, std::nullopt, 50.0, kRed, false};
  ValueReader read = [](int ch, Field) {
    return ch == 0 ? std::optional<double>(150) : std::nullopt;
  };
  auto led = evaluateLed(alerts, read, STATUS_REGISTERED_AND_READY);
  EXPECT_EQ(led.color, kYellow);
  EXPECT_TRUE(led.blink);
}

TEST(HatModelLedTest, BelowThresholdIsStrict) {
  std::vector<Alert> alerts = {
      {0, Field::Primary, 20.0, std::nullopt, kRed, false}};
  auto at = [](double v) {
    return ValueReader([v](int, Field) { return std::optional<double>(v); });
  };
  EXPECT_EQ(evaluateLed(alerts, at(19.9), STATUS_REGISTERED_AND_READY).color,
            kRed);
  EXPECT_EQ(evaluateLed(alerts, at(20), STATUS_REGISTERED_AND_READY).color,
            kGreen);
}

TEST(HatModelLedTest, BlinkAlternatesEvery500ms) {
  LedState blinking{kRed, true};
  EXPECT_EQ(ledOutput(blinking, 0), kRed);
  EXPECT_EQ(ledOutput(blinking, 500), kOff);
  EXPECT_EQ(ledOutput(blinking, 1000), kRed);
  EXPECT_EQ(ledOutput({kGreen, false}, 500), kGreen);
}

TEST(HatModelHistoryTest, KeepsLastCapacityValuesOldestFirst) {
  History history;
  for (size_t i = 0; i < History::kCapacity + 5; i++) {
    history.push(static_cast<double>(i));
  }
  EXPECT_EQ(history.size(), History::kCapacity);
  EXPECT_DOUBLE_EQ(history.at(0), 5);
  EXPECT_DOUBLE_EQ(history.at(History::kCapacity - 1),
                   History::kCapacity + 4);
}

TEST(HatModelHistoryTest, ChartRangeAutoScalesUnlessFixed) {
  History history;
  Page page;
  EXPECT_EQ(chartRange(history, page), std::make_pair(0.0, 1.0));  // empty
  history.push(10);
  EXPECT_EQ(chartRange(history, page), std::make_pair(9.0, 11.0));  // flat
  history.push(30);
  EXPECT_EQ(chartRange(history, page), std::make_pair(10.0, 30.0));
  page.min = 0;
  EXPECT_EQ(chartRange(history, page), std::make_pair(0.0, 30.0));
}

TEST(HatModelHistoryTest, ChartRangeIsNeverInvertedWithoutSamples) {
  History history;
  Page page;
  page.min = 5;  // no max: {5, 1} would plot nothing
  EXPECT_EQ(chartRange(history, page), std::make_pair(4.0, 6.0));

  Page zeroHeight;
  zeroHeight.min = 0;
  zeroHeight.max = 0;
  EXPECT_EQ(chartRange(history, zeroHeight), std::make_pair(-1.0, 1.0));
}

TEST(HatModelFormatTest, FormatsValues) {
  Item power{0, Field::PowerActive, "Power", "W"};
  EXPECT_EQ(formatValue(power, 1234.56, false), "1234.6 W");
  EXPECT_EQ(formatValue(power, std::nullopt, false), "--");
  Item pump{1, Field::Primary, "Pump", ""};
  EXPECT_EQ(formatValue(pump, 1, true), "ON");
  EXPECT_EQ(formatValue(pump, 0, true), "OFF");
}

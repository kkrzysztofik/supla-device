/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include "hat_model.h"

#include <gtest/gtest.h>
#include <yaml-cpp/yaml.h>

#include <string>

using namespace Supla::Linux::DisplayHatMini;  // NOLINT

namespace {

const char *kSample = R"(
type: DisplayHatMini
gpio_chip: gpiochip4
backlight_timeout_sec: 0
buttons:
  a: { caption: "HAT A" }
  y:
pages:
  - title: PV
    type: values
    items:
      - { channel: 0, field: power_active, label: Power, unit: W }
      - { channel: 1, label: Pump }
  - title: PV today
    type: chart
    channel: 0
    field: power_active
    sample_sec: 10
    min: 0
led:
  alerts:
    - { channel: 2, below: 20, color: red, blink: true }
)";

bool parse(const std::string &yaml, Config *cfg, std::string *error) {
  return parseConfig(YAML::Load(yaml), cfg, error);
}

}  // namespace

TEST(HatModelConfigTest, ParsesSample) {
  Config cfg;
  std::string error;
  ASSERT_TRUE(parse(kSample, &cfg, &error)) << error;

  EXPECT_EQ(cfg.gpioChip, "gpiochip4");
  EXPECT_EQ(cfg.framebuffer, "/dev/fb1");
  EXPECT_EQ(cfg.backlightTimeoutSec, 0u);
  EXPECT_EQ(cfg.holdTimeMs, 700u);
  ASSERT_TRUE(cfg.buttonCaptions[kA].has_value());
  EXPECT_EQ(*cfg.buttonCaptions[kA], "HAT A");
  EXPECT_FALSE(cfg.buttonCaptions[kB].has_value());
  ASSERT_TRUE(cfg.buttonCaptions[kY].has_value());
  EXPECT_EQ(*cfg.buttonCaptions[kY], "");

  ASSERT_EQ(cfg.pages.size(), 2u);
  EXPECT_EQ(cfg.pages[0].type, PageType::Values);
  ASSERT_EQ(cfg.pages[0].items.size(), 2u);
  EXPECT_EQ(cfg.pages[0].items[0].field, Field::PowerActive);
  EXPECT_EQ(cfg.pages[0].items[1].field, Field::Primary);
  EXPECT_EQ(cfg.pages[1].type, PageType::Chart);
  EXPECT_EQ(cfg.pages[1].sampleSec, 10u);
  ASSERT_TRUE(cfg.pages[1].min.has_value());
  EXPECT_FALSE(cfg.pages[1].max.has_value());

  ASSERT_EQ(cfg.alerts.size(), 1u);
  EXPECT_EQ(cfg.alerts[0].color, kRed);
  EXPECT_TRUE(cfg.alerts[0].blink);
  EXPECT_DOUBLE_EQ(*cfg.alerts[0].below, 20);
}

TEST(HatModelConfigTest, RejectsUnknownNestedKey) {
  Config cfg;
  std::string error;
  EXPECT_FALSE(parse(R"(
pages:
  - title: X
    items:
      - { channel: 0, lable: typo }
)", &cfg, &error));
  EXPECT_NE(error.find("lable"), std::string::npos) << error;
}

TEST(HatModelConfigTest, RejectsBadFieldAndMissingChannel) {
  Config cfg;
  std::string error;
  EXPECT_FALSE(parse("pages: [{items: [{channel: 0, field: watts}]}]",
                     &cfg, &error));
  EXPECT_NE(error.find("watts"), std::string::npos) << error;
  EXPECT_FALSE(parse("pages: [{items: [{label: x}]}]", &cfg, &error));
  EXPECT_NE(error.find("channel"), std::string::npos) << error;
}

TEST(HatModelConfigTest, RejectsAlertWithoutExactlyOneThreshold) {
  Config cfg;
  std::string error;
  EXPECT_FALSE(
      parse("led: {alerts: [{channel: 0, color: red}]}", &cfg, &error));
  EXPECT_FALSE(parse(
      "led: {alerts: [{channel: 0, below: 1, above: 2, color: red}]}",
      &cfg, &error));
  EXPECT_FALSE(parse("led: {alerts: [{channel: 0, below: 1, color: pink}]}",
                     &cfg, &error));
}

TEST(HatModelConfigTest, RejectsEmptyValuesPageAndBadChart) {
  Config cfg;
  std::string error;
  EXPECT_FALSE(parse("pages: [{type: values, items: []}]", &cfg, &error));
  EXPECT_FALSE(parse("pages: [{type: chart}]", &cfg, &error));
  EXPECT_FALSE(parse("pages: [{type: chart, channel: 0, sample_sec: 0}]",
                     &cfg, &error));
  EXPECT_FALSE(parse("pages: [{type: pie, channel: 0}]", &cfg, &error));
}

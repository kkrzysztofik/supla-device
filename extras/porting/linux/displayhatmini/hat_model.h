/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#ifndef EXTRAS_PORTING_LINUX_DISPLAYHATMINI_HAT_MODEL_H_
#define EXTRAS_PORTING_LINUX_DISPLAYHATMINI_HAT_MODEL_H_

#include <yaml-cpp/yaml.h>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Supla {
namespace Linux {
namespace DisplayHatMini {

// BCM pin numbers on the HAT.
enum Button : uint8_t { kA = 0, kB, kX, kY, kButtonCount };
constexpr std::array<unsigned int, kButtonCount> kButtonPins = {5, 6, 16, 24};
constexpr std::array<unsigned int, 3> kLedPins = {17, 27, 22};  // R, G, B

// Bit 0 = red, bit 1 = green, bit 2 = blue.
enum Color : uint8_t {
  kOff = 0,
  kRed = 1,
  kGreen = 2,
  kYellow = 3,
  kBlue = 4,
  kMagenta = 5,
  kCyan = 6,
  kWhite = 7,
};

enum class Field { Primary, PowerActive, FwdActEnergy, Voltage, Current };

struct Item {
  int channel = -1;
  Field field = Field::Primary;
  std::string label;
  std::string unit;
};

enum class PageType { Values, Chart };

struct Page {
  std::string title;
  PageType type = PageType::Values;
  std::vector<Item> items;  // Values: rows. Chart: exactly one series.
  uint32_t sampleSec = 30;
  std::optional<double> min;
  std::optional<double> max;
};

struct Alert {
  int channel = -1;
  Field field = Field::Primary;
  std::optional<double> below;
  std::optional<double> above;
  Color color = kRed;
  bool blink = false;
};

struct Config {
  std::string gpioChip = "gpiochip0";
  std::string framebuffer = "/dev/fb1";
  std::string backlight = "/sys/class/backlight/fb_st7789v";
  uint32_t backlightTimeoutSec = 60;
  uint32_t holdTimeMs = 700;
  uint32_t multiclickTimeMs = 300;
  // nullopt = no ActionTrigger channel for that button.
  std::array<std::optional<std::string>, kButtonCount> buttonCaptions;
  std::vector<Page> pages;
  std::vector<Alert> alerts;
};

// Parses the DisplayHatMini channel node. Top-level keys are trusted (sd4linux
// rejects unknown ones); nested keys are validated here.
bool parseConfig(const YAML::Node &node, Config *out, std::string *error);

// Current value of channel/field; nullopt = missing, offline or no data yet.
using ValueReader = std::function<std::optional<double>(int, Field)>;

// Short-click menu: A/B prev/next page, X next item, Y toggle selected relay.
class Navigator {
 public:
  explicit Navigator(const std::vector<Page> &pages) : pages(pages) {}

  // Returns the relay channel to toggle (Y on a relay item), else -1.
  int onClick(Button button, const std::function<bool(int)> &isRelay);

  size_t page() const { return pageIndex; }
  size_t cursor() const { return cursorIndex; }

 private:
  const std::vector<Page> &pages;
  size_t pageIndex = 0;
  size_t cursorIndex = 0;
};

struct LedState {
  Color color;
  bool blink;
};

// First alert whose value matches wins; otherwise colour from SuplaDevice
// status (green ready, blue blinking connecting, magenta config, red other).
LedState evaluateLed(const std::vector<Alert> &alerts,
                     const ValueReader &read,
                     int deviceStatus);

// Colour to drive at nowMs (blink = 500 ms on / 500 ms off).
Color ledOutput(LedState state, uint32_t nowMs);

// Ring buffer of chart samples; one point per screen column.
class History {
 public:
  static constexpr size_t kCapacity = 320;

  void push(double value) {
    values[head] = value;
    head = (head + 1) % kCapacity;
    if (count < kCapacity) {
      count++;
    }
  }
  size_t size() const { return count; }
  // 0 = oldest.
  double at(size_t i) const {
    return values[(head + kCapacity - count + i) % kCapacity];
  }

 private:
  std::array<double, kCapacity> values{};
  size_t head = 0;
  size_t count = 0;
};

// Y axis range: page min/max when set, else data min/max (never empty/flat).
std::pair<double, double> chartRange(const History &history, const Page &page);

// "1234.6 W", "ON"/"OFF" for relays, "--" when missing.
std::string formatValue(const Item &item,
                        std::optional<double> value,
                        bool relay);

}  // namespace DisplayHatMini
}  // namespace Linux
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_DISPLAYHATMINI_HAT_MODEL_H_

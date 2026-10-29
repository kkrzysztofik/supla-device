# Display HAT Mini sd4linux Integration — Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add an sd4linux extension `DisplayHatMini` that drives the Pimoroni Display HAT Mini's
LCD (LVGL dashboard + charts), 4 buttons (menu + SUPLA action triggers) and RGB LED
(status + alert rules).

**Architecture:** Pure, hardware-free logic (`hat_model`: config parsing, navigation,
LED rules, history, formatting) lives in `extras/porting/linux/displayhatmini/` and is
unit-tested in `sd4linuxtests`. Hardware glue lives in
`extras/porting/linux/extensions/displayhatmini/`: libgpiod v2 (`hat_gpio`), LVGL on
the kernel framebuffer (`hat_display`), and one `HatController` element that ties them
together on the sd4linux main loop.

**Tech Stack:** C++17, yaml-cpp, gtest, libgpiod v2 (pkg-config), LVGL v9.2.2
(FetchContent), Linux fbtft/fbdev.

Design: `docs/plans/2026-10-29-display-hat-mini-design.md`.

---

## Read before starting

- Work in worktree `/home/kmk/dev/supla-device/.worktrees/display-hat-mini` (branch `display-hat-mini`).
- Prefix shell commands with `rtk` (see `~/CLAUDE.md`).
- New files use the header style from `extras/porting/linux/ingecon/ingecon_bus.h`
  (`Copyright (C) Krzysztof Krzysztofik` + GPL-2.0-or-later text). Never "AC SOFTWARE".
- Extension pattern to copy: `extras/porting/linux/extensions/sos_binary/`.
- sd4linux fails on **unmarked top-level channel keys** (`linux_yaml_config.cpp:659-690`):
  every top-level key we accept must go through `config.getAndMarkChannelParameter`.
  Nested keys are not checked by sd4linux → `parseConfig` must reject unknown ones itself.
- `Button::onTimer` runs on the sd4linux **timer thread** (`extras/porting/linux/linux_timers.cpp`),
  so `handleAction` from buttons must only enqueue; all LVGL/relay work happens in `iterateAlways`.
- sd4linux exits with `exit()` (`extras/examples/linux/main.cpp:217`), elements are never
  destroyed → LED/backlight off is done in an `std::atexit` hook.
- libgpiod is not on the dev box: `sudo apt install libgpiod-dev pkg-config` (Ubuntu ≥ 24.04
  and Raspberry Pi OS Bookworm ship libgpiod v2). Check: `pkg-config --modversion libgpiod` ≥ 2.0.

### Deviations from the design doc (decided during planning)

1. Backlight is owned by the kernel display overlay (`led_pin=13`), controlled via
   `/sys/class/backlight/<name>/bl_power`, not libgpiod. New YAML key `backlight`.
2. LVGL via `FetchContent` (repo convention), not a git submodule.
3. Unknown channels in pages/alerts are detected in `onInit` (after all channels exist),
   logged, and shown as `--` instead of failing startup.
4. Buttons + LED + glue merged into one `HatController` element in the extension file
   (no `hat_buttons.cpp` / `hat_led.cpp`).
5. No `soc` field: BYD SoC is a plain measurement channel → default primary value.
6. `channel:` in pages/alerts means the SUPLA channel number. The HAT's action-trigger
   channels take numbers in creation order, so put the `DisplayHatMini` entry **last**.

---

### Task 0: Hardware bring-up on the Pi (manual, de-risks everything else)

**Files:** none (record findings for Task 8's doc)

**Step 1: Enable SPI and the display overlay**

Append to `/boot/firmware/config.txt`:

```
dtparam=spi=on
dtoverlay=fbtft,spi0-1,st7789v,width=240,height=320,rotate=90,dc_pin=9,led_pin=13,speed=40000000
```

Reboot.

**Step 2: Verify framebuffer + backlight**

```bash
ls -l /dev/fb*                         # expect /dev/fb1 (fb0 = HDMI) or fb0 headless
cat /sys/class/graphics/fb1/virtual_size   # expect 320,240
ls /sys/class/backlight/               # note the name, e.g. fb_st7789v
cat /dev/urandom > /dev/fb1            # expect noise on the LCD ("No space left" is fine)
echo 4 | sudo tee /sys/class/backlight/*/bl_power   # backlight off
echo 0 | sudo tee /sys/class/backlight/*/bl_power   # backlight on
```

If colours look inverted / the panel stays white, the generic fbtft st7789v init doesn't
match: try `rotate=270`, then `bgr` param; as last resort switch to
`dtoverlay=mipi-dbi-spi` with an ST7789 init firmware (see Raspberry Pi
`overlays/README`, section `mipi-dbi-spi`). Record whatever works.

**Step 3: Verify GPIO**

```bash
sudo apt install gpiod libgpiod-dev
gpioinfo | grep -E "GPIO(5|6|16|24|17|27|22)\b"   # note the chip name (gpiochip0 / gpiochip4)
gpioget -b pull-up -c gpiochip0 5 6 16 24          # all "active" (1) when released... press A → 5 goes 0
gpioset -c gpiochip0 17=0 &  sleep 1; kill %1      # LED red on while held (active-low)
```

**Step 4:** Write the working overlay line, framebuffer path, backlight name and gpio chip
into a scratch note for Task 8. No commit.

---

### Task 1: `hat_model` — config types and parser

**Files:**
- Create: `extras/porting/linux/displayhatmini/hat_model.h`
- Create: `extras/porting/linux/displayhatmini/hat_model.cpp`
- Create: `extras/test/DisplayHatMiniTests/hat_model_config_test.cpp`
- Modify: `extras/test/CMakeLists.txt` (SD4LINUX_TEST_SRC glob ~line 123, `sd4linuxtests` sources ~line 280, include dirs ~line 327)

**Step 1: Wire the test target**

In `extras/test/CMakeLists.txt`:

```cmake
file(GLOB SD4LINUX_TEST_SRC CONFIGURE_DEPENDS
  LinuxPortTests/*.cpp
  DisplayHatMiniTests/*.cpp
  )
```

Add `../porting/linux/displayhatmini/hat_model.cpp` to `add_executable(sd4linuxtests ...)`
and `${CMAKE_CURRENT_SOURCE_DIR}/../porting/linux/displayhatmini` to
`target_include_directories(sd4linuxtests PRIVATE ...)`.

**Step 2: Write the failing tests**

`extras/test/DisplayHatMiniTests/hat_model_config_test.cpp`:

```cpp
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
  EXPECT_FALSE(parse("led: {alerts: [{channel: 0, color: red}]}", &cfg, &error));
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
```

**Step 3: Write the header**

`extras/porting/linux/displayhatmini/hat_model.h`:

```cpp
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

}  // namespace DisplayHatMini
}  // namespace Linux
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_DISPLAYHATMINI_HAT_MODEL_H_
```

**Step 4: Run tests to verify they fail**

```bash
cmake -S extras/test -B extras/test/build-dhm
rtk cmake --build extras/test/build-dhm -j"$(nproc)" --target sd4linuxtests
```

Expected: link error `undefined reference to ...parseConfig`.

**Step 5: Implement the parser**

`extras/porting/linux/displayhatmini/hat_model.cpp`:

```cpp
/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include "hat_model.h"

#include <initializer_list>

namespace Supla {
namespace Linux {
namespace DisplayHatMini {

namespace {

bool fail(std::string *error, const std::string &message) {
  *error = "DisplayHatMini " + message;
  return false;
}

bool checkKeys(const YAML::Node &node,
               std::initializer_list<const char *> allowed,
               const std::string &where,
               std::string *error) {
  if (!node.IsMap()) {
    return fail(error, where + ": expected a map");
  }
  for (const auto &entry : node) {
    auto key = entry.first.as<std::string>();
    bool known = false;
    for (auto name : allowed) {
      known = known || key == name;
    }
    if (!known) {
      return fail(error, where + ": unknown key \"" + key + "\"");
    }
  }
  return true;
}

bool parseChannel(const YAML::Node &node,
                  int *out,
                  const std::string &where,
                  std::string *error) {
  if (!node || node.as<int>() < 0) {
    return fail(error, where + ": missing or negative \"channel\"");
  }
  *out = node.as<int>();
  return true;
}

bool parseField(const YAML::Node &node,
                Field *out,
                const std::string &where,
                std::string *error) {
  if (!node) {
    *out = Field::Primary;
    return true;
  }
  auto name = node.as<std::string>();
  if (name == "power_active") {
    *out = Field::PowerActive;
  } else if (name == "fwd_act_energy") {
    *out = Field::FwdActEnergy;
  } else if (name == "voltage") {
    *out = Field::Voltage;
  } else if (name == "current") {
    *out = Field::Current;
  } else {
    return fail(error, where + ": unknown field \"" + name + "\"");
  }
  return true;
}

bool parseColor(const YAML::Node &node,
                Color *out,
                const std::string &where,
                std::string *error) {
  static const std::pair<const char *, Color> kColors[] = {
      {"red", kRed},       {"green", kGreen}, {"yellow", kYellow},
      {"blue", kBlue},     {"magenta", kMagenta}, {"cyan", kCyan},
      {"white", kWhite}};
  if (!node) {
    return fail(error, where + ": missing \"color\"");
  }
  auto name = node.as<std::string>();
  for (const auto &color : kColors) {
    if (name == color.first) {
      *out = color.second;
      return true;
    }
  }
  return fail(error, where + ": unknown color \"" + name + "\"");
}

bool parseItem(const YAML::Node &node,
               Item *out,
               const std::string &where,
               std::string *error) {
  if (!checkKeys(node, {"channel", "field", "label", "unit"}, where, error) ||
      !parseChannel(node["channel"], &out->channel, where, error) ||
      !parseField(node["field"], &out->field, where, error)) {
    return false;
  }
  if (node["label"]) {
    out->label = node["label"].as<std::string>();
  }
  if (node["unit"]) {
    out->unit = node["unit"].as<std::string>();
  }
  return true;
}

bool parsePage(const YAML::Node &node,
               Page *out,
               const std::string &where,
               std::string *error) {
  if (!node.IsMap()) {
    return fail(error, where + ": expected a map");
  }
  auto type = node["type"] ? node["type"].as<std::string>() : "values";
  if (node["title"]) {
    out->title = node["title"].as<std::string>();
  }
  if (type == "values") {
    if (!checkKeys(node, {"title", "type", "items"}, where, error)) {
      return false;
    }
    auto items = node["items"];
    if (!items || !items.IsSequence() || items.size() == 0) {
      return fail(error, where + ": \"items\" must be a non-empty list");
    }
    for (size_t i = 0; i < items.size(); i++) {
      Item item;
      if (!parseItem(items[i], &item,
                     where + ".items[" + std::to_string(i) + "]", error)) {
        return false;
      }
      out->items.push_back(item);
    }
    return true;
  }
  if (type == "chart") {
    out->type = PageType::Chart;
    if (!checkKeys(node,
                   {"title", "type", "channel", "field", "unit", "sample_sec",
                    "min", "max"},
                   where, error)) {
      return false;
    }
    Item series;
    if (!parseChannel(node["channel"], &series.channel, where, error) ||
        !parseField(node["field"], &series.field, where, error)) {
      return false;
    }
    if (node["unit"]) {
      series.unit = node["unit"].as<std::string>();
    }
    out->items.push_back(series);
    if (node["sample_sec"]) {
      int sampleSec = node["sample_sec"].as<int>();
      if (sampleSec < 1) {
        return fail(error, where + ": \"sample_sec\" must be >= 1");
      }
      out->sampleSec = sampleSec;
    }
    if (node["min"]) {
      out->min = node["min"].as<double>();
    }
    if (node["max"]) {
      out->max = node["max"].as<double>();
    }
    return true;
  }
  return fail(error, where + ": unknown type \"" + type + "\"");
}

bool parseAlert(const YAML::Node &node,
                Alert *out,
                const std::string &where,
                std::string *error) {
  if (!checkKeys(node,
                 {"channel", "field", "below", "above", "color", "blink"},
                 where, error) ||
      !parseChannel(node["channel"], &out->channel, where, error) ||
      !parseField(node["field"], &out->field, where, error) ||
      !parseColor(node["color"], &out->color, where, error)) {
    return false;
  }
  if (node["below"]) {
    out->below = node["below"].as<double>();
  }
  if (node["above"]) {
    out->above = node["above"].as<double>();
  }
  if (out->below.has_value() == out->above.has_value()) {
    return fail(error, where + ": set exactly one of \"below\" / \"above\"");
  }
  if (node["blink"]) {
    out->blink = node["blink"].as<bool>();
  }
  return true;
}

}  // namespace

bool parseConfig(const YAML::Node &node, Config *out, std::string *error) {
  try {
    Config cfg;
    if (node["gpio_chip"]) {
      cfg.gpioChip = node["gpio_chip"].as<std::string>();
    }
    if (node["framebuffer"]) {
      cfg.framebuffer = node["framebuffer"].as<std::string>();
    }
    if (node["backlight"]) {
      cfg.backlight = node["backlight"].as<std::string>();
    }
    if (node["backlight_timeout_sec"]) {
      cfg.backlightTimeoutSec = node["backlight_timeout_sec"].as<uint32_t>();
    }
    if (node["hold_time_ms"]) {
      cfg.holdTimeMs = node["hold_time_ms"].as<uint32_t>();
    }
    if (node["multiclick_time_ms"]) {
      cfg.multiclickTimeMs = node["multiclick_time_ms"].as<uint32_t>();
    }

    if (auto buttons = node["buttons"]) {
      if (!checkKeys(buttons, {"a", "b", "x", "y"}, "buttons", error)) {
        return false;
      }
      static const char *kNames[kButtonCount] = {"a", "b", "x", "y"};
      for (int i = 0; i < kButtonCount; i++) {
        auto button = buttons[kNames[i]];
        if (!button) {
          continue;
        }
        cfg.buttonCaptions[i] = "";
        if (button.IsNull()) {
          continue;
        }
        std::string where = std::string("buttons.") + kNames[i];
        if (!checkKeys(button, {"caption"}, where, error)) {
          return false;
        }
        if (button["caption"]) {
          cfg.buttonCaptions[i] = button["caption"].as<std::string>();
        }
      }
    }

    if (auto pages = node["pages"]) {
      if (!pages.IsSequence()) {
        return fail(error, "pages: expected a list");
      }
      for (size_t i = 0; i < pages.size(); i++) {
        Page page;
        if (!parsePage(pages[i], &page,
                       "pages[" + std::to_string(i) + "]", error)) {
          return false;
        }
        cfg.pages.push_back(std::move(page));
      }
    }

    if (auto led = node["led"]) {
      if (!checkKeys(led, {"alerts"}, "led", error)) {
        return false;
      }
      if (auto alerts = led["alerts"]) {
        if (!alerts.IsSequence()) {
          return fail(error, "led.alerts: expected a list");
        }
        for (size_t i = 0; i < alerts.size(); i++) {
          Alert alert;
          if (!parseAlert(alerts[i], &alert,
                          "led.alerts[" + std::to_string(i) + "]", error)) {
            return false;
          }
          cfg.alerts.push_back(alert);
        }
      }
    }

    *out = std::move(cfg);
    return true;
  } catch (const YAML::Exception &e) {
    return fail(error, std::string(": ") + e.what());
  }
}

}  // namespace DisplayHatMini
}  // namespace Linux
}  // namespace Supla
```

**Step 6: Run tests to verify they pass**

```bash
rtk cmake --build extras/test/build-dhm -j"$(nproc)" --target sd4linuxtests
extras/test/build-dhm/sd4linuxtests --gtest_filter='HatModel*'
```

Expected: 5 tests PASS.

**Step 7: Commit**

```bash
rtk git add extras/porting/linux/displayhatmini extras/test/DisplayHatMiniTests extras/test/CMakeLists.txt
rtk git commit -m "feat(displayhatmini): config model and parser"
```

---

### Task 2: `hat_model` — navigation

**Files:**
- Modify: `extras/porting/linux/displayhatmini/hat_model.{h,cpp}`
- Create: `extras/test/DisplayHatMiniTests/hat_model_logic_test.cpp`

**Step 1: Write the failing tests**

`extras/test/DisplayHatMiniTests/hat_model_logic_test.cpp` (same license header):

```cpp
#include "hat_model.h"

#include <gtest/gtest.h>

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
```

**Step 2: Run, expect compile failure** (`Navigator` undeclared):
`rtk cmake --build extras/test/build-dhm -j"$(nproc)" --target sd4linuxtests`

**Step 3: Implement**

Append to `hat_model.h` (before the closing namespaces):

```cpp
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
```

Append to `hat_model.cpp`:

```cpp
int Navigator::onClick(Button button,
                       const std::function<bool(int)> &isRelay) {
  if (pages.empty()) {
    return -1;
  }
  const auto &page = pages[pageIndex];
  switch (button) {
    case kA:
      pageIndex = (pageIndex + pages.size() - 1) % pages.size();
      cursorIndex = 0;
      return -1;
    case kB:
      pageIndex = (pageIndex + 1) % pages.size();
      cursorIndex = 0;
      return -1;
    case kX:
      if (page.type == PageType::Values) {
        cursorIndex = (cursorIndex + 1) % page.items.size();
      }
      return -1;
    case kY: {
      if (page.type != PageType::Values) {
        return -1;
      }
      const auto &item = page.items[cursorIndex];
      return item.field == Field::Primary && isRelay(item.channel)
                 ? item.channel
                 : -1;
    }
    default:
      return -1;
  }
}
```

**Step 4: Run** `extras/test/build-dhm/sd4linuxtests --gtest_filter='HatModel*'` → all PASS.

**Step 5: Commit** `rtk git commit -am "feat(displayhatmini): button navigation model"` (add the new test file first).

---

### Task 3: `hat_model` — LED status and alert rules

**Files:** same as Task 2.

**Step 1: Append failing tests** to `hat_model_logic_test.cpp`:

```cpp
#include <SuplaDevice.h>

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
  alerts[0] = {5, Field::Primary, 10.0, std::nullopt, kCyan, false};   // ch 5 missing
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
```

**Step 2: Run, expect compile failure.**

**Step 3: Implement.** Append to `hat_model.h`:

```cpp
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
```

Append to `hat_model.cpp` (add `#include <SuplaDevice.h>` at the top):

```cpp
namespace {

LedState statusLed(int status) {
  switch (status) {
    case STATUS_REGISTERED_AND_READY:
    case STATUS_OFFLINE_MODE:
    case STATUS_SUPLA_PROTOCOL_DISABLED:
      return {kGreen, false};
    case STATUS_INITIALIZED:
    case STATUS_NETWORK_DISCONNECTED:
    case STATUS_REGISTER_IN_PROGRESS:
      return {kBlue, true};
    case STATUS_CONFIG_MODE:
      return {kMagenta, false};
    default:
      return {kRed, false};
  }
}

}  // namespace

LedState evaluateLed(const std::vector<Alert> &alerts,
                     const ValueReader &read,
                     int deviceStatus) {
  for (const auto &alert : alerts) {
    auto value = read(alert.channel, alert.field);
    if (!value) {
      continue;
    }
    if ((alert.below && *value < *alert.below) ||
        (alert.above && *value > *alert.above)) {
      return {alert.color, alert.blink};
    }
  }
  return statusLed(deviceStatus);
}

Color ledOutput(LedState state, uint32_t nowMs) {
  return state.blink && (nowMs / 500) % 2 ? kOff : state.color;
}
```

**Step 4: Run tests → PASS. Step 5: Commit** `feat(displayhatmini): LED status and alert rules`.

---

### Task 4: `hat_model` — history buffer, chart range, value formatting

**Files:** same as Task 2.

**Step 1: Append failing tests:**

```cpp
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

TEST(HatModelFormatTest, FormatsValues) {
  Item power{0, Field::PowerActive, "Power", "W"};
  EXPECT_EQ(formatValue(power, 1234.56, false), "1234.6 W");
  EXPECT_EQ(formatValue(power, std::nullopt, false), "--");
  Item pump{1, Field::Primary, "Pump", ""};
  EXPECT_EQ(formatValue(pump, 1, true), "ON");
  EXPECT_EQ(formatValue(pump, 0, true), "OFF");
}
```

**Step 2: Run, expect compile failure.**

**Step 3: Implement.** Append to `hat_model.h`:

```cpp
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
```

Append to `hat_model.cpp` (add `#include <algorithm>` and `#include <cstdio>`):

```cpp
std::pair<double, double> chartRange(const History &history,
                                     const Page &page) {
  if (history.size() == 0) {
    return {page.min.value_or(0), page.max.value_or(1)};
  }
  double low = history.at(0);
  double high = low;
  for (size_t i = 1; i < history.size(); i++) {
    low = std::min(low, history.at(i));
    high = std::max(high, history.at(i));
  }
  low = page.min.value_or(low);
  high = page.max.value_or(high);
  if (high <= low) {
    return {low - 1, low + 1};
  }
  return {low, high};
}

std::string formatValue(const Item &item,
                        std::optional<double> value,
                        bool relay) {
  if (!value) {
    return "--";
  }
  if (relay) {
    return *value != 0 ? "ON" : "OFF";
  }
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%.1f", *value);
  return item.unit.empty() ? buffer : std::string(buffer) + " " + item.unit;
}
```

Note: empty history with neither min nor max must return `{0, 1}` — the test checks this;
`page.min.value_or(0)`/`value_or(1)` covers it.

**Step 4: Run tests → PASS. Step 5: Commit** `feat(displayhatmini): chart history and value formatting`.

---

### Task 5: `hat_gpio` — libgpiod v2 Io adapter

**Files:**
- Create: `extras/porting/linux/extensions/displayhatmini/hat_gpio.h`
- Create: `extras/porting/linux/extensions/displayhatmini/hat_gpio.cpp`

No unit test (thin hardware wrapper); verified by build in Task 7 and on hardware in Task 11.

**Step 1: Header**

```cpp
/* (license header) */

#ifndef EXTRAS_PORTING_LINUX_EXTENSIONS_DISPLAYHATMINI_HAT_GPIO_H_
#define EXTRAS_PORTING_LINUX_EXTENSIONS_DISPLAYHATMINI_HAT_GPIO_H_

#include <supla/io.h>

#include <string>

#include "hat_model.h"

struct gpiod_chip;
struct gpiod_line_request;

namespace Supla {
namespace Linux {
namespace DisplayHatMini {

// Buttons read as 1 when pressed, LED lines are driven active-low.
// Inputs are read on the timer thread, outputs written on the main loop:
// separate line requests so the two threads never share one.
class HatGpio : public Supla::Io::Base {
 public:
  ~HatGpio() override;

  bool open(const std::string &chipPath);
  void setLed(Color color);

  void customPinMode(int, uint8_t, uint8_t) override {}
  int customDigitalRead(int channelNumber, uint8_t pin) override;

 private:
  void close();

  gpiod_chip *chip = nullptr;
  gpiod_line_request *inputs = nullptr;
  gpiod_line_request *outputs = nullptr;
};

}  // namespace DisplayHatMini
}  // namespace Linux
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_EXTENSIONS_DISPLAYHATMINI_HAT_GPIO_H_
```

**Step 2: Implementation**

```cpp
/* (license header) */

#include "hat_gpio.h"

#include <gpiod.h>

namespace Supla {
namespace Linux {
namespace DisplayHatMini {

namespace {

gpiod_line_request *requestLines(gpiod_chip *chip,
                                 const unsigned int *offsets,
                                 size_t count,
                                 gpiod_line_direction direction) {
  auto settings = gpiod_line_settings_new();
  auto lineConfig = gpiod_line_config_new();
  auto requestConfig = gpiod_request_config_new();
  gpiod_line_request *request = nullptr;
  if (settings && lineConfig && requestConfig) {
    gpiod_line_settings_set_direction(settings, direction);
    // Buttons and LEDs are both active-low on the HAT.
    gpiod_line_settings_set_active_low(settings, true);
    if (direction == GPIOD_LINE_DIRECTION_INPUT) {
      gpiod_line_settings_set_bias(settings, GPIOD_LINE_BIAS_PULL_UP);
    } else {
      gpiod_line_settings_set_output_value(settings,
                                           GPIOD_LINE_VALUE_INACTIVE);
    }
    gpiod_request_config_set_consumer(requestConfig, "supla-displayhatmini");
    if (gpiod_line_config_add_line_settings(
            lineConfig, offsets, count, settings) == 0) {
      request = gpiod_chip_request_lines(chip, requestConfig, lineConfig);
    }
  }
  gpiod_request_config_free(requestConfig);
  gpiod_line_config_free(lineConfig);
  gpiod_line_settings_free(settings);
  return request;
}

}  // namespace

HatGpio::~HatGpio() {
  close();
}

bool HatGpio::open(const std::string &chipPath) {
  chip = gpiod_chip_open(chipPath.c_str());
  if (chip) {
    inputs = requestLines(chip, kButtonPins.data(), kButtonPins.size(),
                          GPIOD_LINE_DIRECTION_INPUT);
    outputs = requestLines(chip, kLedPins.data(), kLedPins.size(),
                           GPIOD_LINE_DIRECTION_OUTPUT);
  }
  if (!inputs || !outputs) {
    close();
    return false;
  }
  return true;
}

void HatGpio::close() {
  if (inputs) {
    gpiod_line_request_release(inputs);
    inputs = nullptr;
  }
  if (outputs) {
    gpiod_line_request_release(outputs);
    outputs = nullptr;
  }
  if (chip) {
    gpiod_chip_close(chip);
    chip = nullptr;
  }
}

int HatGpio::customDigitalRead(int, uint8_t pin) {
  if (!inputs) {
    return 0;
  }
  return gpiod_line_request_get_value(inputs, pin) == GPIOD_LINE_VALUE_ACTIVE;
}

void HatGpio::setLed(Color color) {
  if (!outputs) {
    return;
  }
  auto bit = [color](int mask) {
    return color & mask ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE;
  };
  gpiod_line_value values[] = {bit(kRed), bit(kGreen), bit(kBlue)};
  gpiod_line_request_set_values(outputs, values);
}

}  // namespace DisplayHatMini
}  // namespace Linux
}  // namespace Supla
```

**Step 3: Commit** `feat(displayhatmini): libgpiod v2 Io adapter` (compiled in Task 7).

---

### Task 6: `hat_display` — LVGL renderer

**Files:**
- Create: `extras/porting/linux/extensions/displayhatmini/lv_conf.h`
- Create: `extras/porting/linux/extensions/displayhatmini/hat_display.h`
- Create: `extras/porting/linux/extensions/displayhatmini/hat_display.cpp`

All LVGL calls live in `hat_display.cpp`; nothing else includes `lvgl.h` except its header.
API is LVGL **v9.2** (pinned in Task 7). If a call doesn't compile, check
`_deps/lvgl-src/src/widgets/chart/lv_chart.h` / `src/drivers/display/fb/lv_linux_fbdev.h`
in the build dir before changing anything else.

**Step 1: `lv_conf.h`** (LVGL fills every other option with its defaults)

```c
/* (license header) */

#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16
#define LV_USE_OS LV_OS_NONE
#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB
#define LV_USE_LINUX_FBDEV 1
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1
#define LV_USE_CHART 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_28 1

#endif  // LV_CONF_H
```

**Step 2: Header**

```cpp
/* (license header) */

#ifndef EXTRAS_PORTING_LINUX_EXTENSIONS_DISPLAYHATMINI_HAT_DISPLAY_H_
#define EXTRAS_PORTING_LINUX_EXTENSIONS_DISPLAYHATMINI_HAT_DISPLAY_H_

#include <lvgl.h>

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include "hat_model.h"

namespace Supla {
namespace Linux {
namespace DisplayHatMini {

// The only place that knows about pixels. Swapping the UI means rewriting
// this file; hat_model and the controller stay as they are.
class HatDisplay {
 public:
  bool begin(const std::string &framebuffer, const std::string &backlightPath);
  void setBacklight(bool on);
  void render(const Config &config,
              const Navigator &nav,
              const ValueReader &read,
              const std::function<bool(int)> &isRelay,
              const std::vector<History> &history);
  void tick();
  void shutdown();

 private:
  void buildPage(const Page &page);

  std::string backlightPath;
  lv_display_t *display = nullptr;
  size_t builtPage = static_cast<size_t>(-1);
  std::vector<lv_obj_t *> rows;
  std::vector<lv_obj_t *> values;
  lv_obj_t *chart = nullptr;
  lv_chart_series_t *series = nullptr;
  lv_obj_t *chartValue = nullptr;
};

}  // namespace DisplayHatMini
}  // namespace Linux
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_EXTENSIONS_DISPLAYHATMINI_HAT_DISPLAY_H_
```

**Step 3: Implementation**

```cpp
/* (license header) */

#include "hat_display.h"

#include <supla/time.h>
#include <unistd.h>

#include <cmath>
#include <fstream>

namespace Supla {
namespace Linux {
namespace DisplayHatMini {

namespace {

constexpr int kTitleHeight = 32;
constexpr double kChartScale = 10.0;  // lv_chart stores int32: keep 1 decimal

lv_obj_t *plainBox(lv_obj_t *parent) {
  auto box = lv_obj_create(parent);
  lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(box, 0, 0);
  lv_obj_set_style_radius(box, 0, 0);
  lv_obj_set_style_pad_all(box, 4, 0);
  lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
  return box;
}

lv_obj_t *label(lv_obj_t *parent, const lv_font_t *font) {
  auto obj = lv_label_create(parent);
  lv_obj_set_style_text_font(obj, font, 0);
  lv_obj_set_style_text_color(obj, lv_color_white(), 0);
  return obj;
}

}  // namespace

bool HatDisplay::begin(const std::string &framebuffer,
                       const std::string &backlight) {
  backlightPath = backlight;
  if (access(framebuffer.c_str(), R_OK | W_OK) != 0) {
    return false;
  }
  lv_init();
  lv_tick_set_cb([]() -> uint32_t { return millis(); });
  display = lv_linux_fbdev_create();
  if (!display) {
    return false;
  }
  lv_linux_fbdev_set_file(display, framebuffer.c_str());
  lv_obj_set_style_bg_color(lv_screen_active(), lv_color_black(), 0);
  setBacklight(true);
  return true;
}

void HatDisplay::setBacklight(bool on) {
  // bl_power: 0 = on, 4 = powerdown. Missing path = no backlight control.
  std::ofstream(backlightPath + "/bl_power") << (on ? "0" : "4");
}

void HatDisplay::buildPage(const Page &page) {
  auto screen = lv_screen_active();
  lv_obj_clean(screen);
  rows.clear();
  values.clear();
  chart = nullptr;
  series = nullptr;
  chartValue = nullptr;

  auto title = label(screen, &lv_font_montserrat_20);
  lv_label_set_text(title, page.title.c_str());
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 6, 6);

  if (page.type == PageType::Chart) {
    chartValue = label(screen, &lv_font_montserrat_20);
    lv_obj_align(chartValue, LV_ALIGN_TOP_RIGHT, -6, 6);
    chart = lv_chart_create(screen);
    lv_obj_set_size(chart, LV_PCT(100), LV_VER_RES - kTitleHeight);
    lv_obj_align(chart, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart, History::kCapacity);
    lv_obj_set_style_size(chart, 0, 0, LV_PART_INDICATOR);  // no point dots
    lv_obj_set_style_bg_color(chart, lv_color_black(), 0);
    series = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_YELLOW),
                                 LV_CHART_AXIS_PRIMARY_Y);
    return;
  }

  auto list = plainBox(screen);
  lv_obj_set_size(list, LV_PCT(100), LV_VER_RES - kTitleHeight);
  lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  for (const auto &item : page.items) {
    auto row = plainBox(list);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(row, lv_palette_darken(LV_PALETTE_BLUE, 3), 0);
    auto name = label(row, &lv_font_montserrat_20);
    lv_label_set_text(name, item.label.c_str());
    lv_obj_align(name, LV_ALIGN_LEFT_MID, 0, 0);
    auto value = label(row, &lv_font_montserrat_28);
    lv_obj_align(value, LV_ALIGN_RIGHT_MID, 0, 0);
    rows.push_back(row);
    values.push_back(value);
  }
}

void HatDisplay::render(const Config &config,
                        const Navigator &nav,
                        const ValueReader &read,
                        const std::function<bool(int)> &isRelay,
                        const std::vector<History> &history) {
  if (!display || config.pages.empty()) {
    return;
  }
  const auto &page = config.pages[nav.page()];
  if (builtPage != nav.page()) {
    buildPage(page);
    builtPage = nav.page();
  }

  if (page.type == PageType::Values) {
    for (size_t i = 0; i < page.items.size(); i++) {
      const auto &item = page.items[i];
      bool relay = item.field == Field::Primary && isRelay(item.channel);
      lv_label_set_text(
          values[i],
          formatValue(item, read(item.channel, item.field), relay).c_str());
      lv_obj_set_style_bg_opa(
          rows[i], i == nav.cursor() ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    }
    return;
  }

  const auto &item = page.items[0];
  const auto &samples = history[nav.page()];
  lv_label_set_text(
      chartValue,
      formatValue(item, read(item.channel, item.field), false).c_str());
  auto range = chartRange(samples, page);
  lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y,
                     std::floor(range.first * kChartScale),
                     std::ceil(range.second * kChartScale));
  for (size_t i = 0; i < History::kCapacity; i++) {
    lv_chart_set_value_by_id(
        chart, series, i,
        i < samples.size() ? std::lround(samples.at(i) * kChartScale)
                           : LV_CHART_POINT_NONE);
  }
  lv_chart_refresh(chart);
}

void HatDisplay::tick() {
  if (display) {
    lv_timer_handler();
  }
}

void HatDisplay::shutdown() {
  if (display) {
    lv_obj_clean(lv_screen_active());
    lv_refr_now(display);
  }
  setBacklight(false);
}

}  // namespace DisplayHatMini
}  // namespace Linux
}  // namespace Supla
```

**Step 4: Commit** `feat(displayhatmini): LVGL renderer` (compiled in Task 7).

---

### Task 7: Extension — controller element, factory, CMake

**Files:**
- Create: `extras/porting/linux/extensions/displayhatmini/supla_linux_extension.cmake`
- Create: `extras/porting/linux/extensions/displayhatmini/displayhatmini_extension.cpp`
- Create: `extras/examples/linux/build-displayhatmini.sh`

**Step 1: CMake**

```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(GPIOD REQUIRED IMPORTED_TARGET libgpiod>=2.0)

set(LV_CONF_PATH ${CMAKE_CURRENT_LIST_DIR}/lv_conf.h CACHE PATH "" FORCE)
set(LV_CONF_BUILD_DISABLE_EXAMPLES ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_DEMOS ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_THORVG_INTERNAL ON CACHE BOOL "" FORCE)
FetchContent_Declare(
  lvgl
  GIT_REPOSITORY https://github.com/lvgl/lvgl.git
  GIT_TAG v9.2.2
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(lvgl)

set(_DHM_DIR ${SUPLA_LINUX_PORT_DIR}/displayhatmini)

supla_linux_register_extension(
  NAME displayhatmini
  INIT_FUNCTION initDisplayHatMiniExtension
  SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/displayhatmini_extension.cpp
    ${CMAKE_CURRENT_LIST_DIR}/hat_gpio.cpp
    ${CMAKE_CURRENT_LIST_DIR}/hat_display.cpp
    ${_DHM_DIR}/hat_model.cpp
  INCLUDE_DIRS
    ${SUPLA_LINUX_PORT_DIR}
    ${_DHM_DIR}
    ${CMAKE_CURRENT_LIST_DIR}
    ${_SUPLA_ROOT_FROM_LINUX}/src
  LIBRARIES
    lvgl
    PkgConfig::GPIOD
)
```

If configure complains about `LV_CONF_PATH`, check `_deps/lvgl-src/env_support/cmake/custom.cmake`
for the variable names of the pinned version.

**Step 2: Extension source**

```cpp
/* (license header) */

#include <SuplaDevice.h>
#include <supla-common/proto.h>
#include <supla/action_handler.h>
#include <supla/actions.h>
#include <supla/channels/channel.h>
#include <supla/control/action_trigger.h>
#include <supla/control/button.h>
#include <supla/element.h>
#include <supla/events.h>
#include <supla/log_wrapper.h>
#include <supla/sensor/electricity_meter.h>
#include <supla/sensor/thermometer_driver.h>
#include <supla/time.h>

#include <cmath>
#include <cstdlib>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

#include "hat_display.h"
#include "hat_gpio.h"
#include "hat_model.h"
#include "linux_channel_factory.h"
#include "linux_yaml_config.h"

namespace dhm = Supla::Linux::DisplayHatMini;

namespace {

constexpr int kWakeAction = 100;
constexpr uint32_t kRefreshMs = 250;

std::optional<double> readValue(int channelNumber, dhm::Field field) {
  auto element = Supla::Element::getElementByChannelNumber(channelNumber);
  auto channel = element ? element->getChannel() : nullptr;
  if (!channel || !channel->isStateOnline()) {
    return std::nullopt;
  }
  if (field == dhm::Field::Primary) {
    if (channel->getChannelType() == SUPLA_CHANNELTYPE_RELAY) {
      return channel->getValueBool() ? 1.0 : 0.0;
    }
    double value = channel->getValueDouble();
    if (std::isnan(value) || value == TEMPERATURE_NOT_AVAILABLE) {
      return std::nullopt;
    }
    return value;
  }
  auto meter = dynamic_cast<Supla::Sensor::ElectricityMeter *>(element);
  if (!meter) {
    return std::nullopt;
  }
  double sum = 0;
  for (int phase = 0; phase < 3; phase++) {
    switch (field) {
      case dhm::Field::PowerActive:
        sum += meter->getPowerActive(phase) / 100000.0;
        break;
      case dhm::Field::FwdActEnergy:
        sum += meter->getFwdActEnergy(phase) / 100000.0;
        break;
      case dhm::Field::Current:
        sum += meter->getCurrent(phase) / 1000.0;
        break;
      case dhm::Field::Voltage:
        return meter->getVoltage(0) / 100.0;
      default:
        return std::nullopt;
    }
  }
  return sum;
}

bool isRelay(int channelNumber) {
  auto element = Supla::Element::getElementByChannelNumber(channelNumber);
  return element && element->getChannel() &&
         element->getChannel()->getChannelType() == SUPLA_CHANNELTYPE_RELAY;
}

class HatController : public Supla::Element, public Supla::ActionHandler {
 public:
  explicit HatController(dhm::Config cfg)
      : config(std::move(cfg)),
        nav(config.pages),
        history(config.pages.size()),
        lastSampleMs(config.pages.size(), 0) {}

  bool openGpio() {
    gpioOk = gpio.open("/dev/" + config.gpioChip);
    if (!gpioOk) {
      SUPLA_LOG_ERROR("DisplayHatMini: cannot open GPIO lines on /dev/%s, "
                      "HAT disabled", config.gpioChip.c_str());
    }
    return gpioOk;
  }

  void createButtons() {
    for (int i = 0; i < dhm::kButtonCount; i++) {
      auto button = new Supla::Control::Button(&gpio, dhm::kButtonPins[i]);
      button->setHoldTime(config.holdTimeMs);
      button->setMulticlickTime(config.multiclickTimeMs);
      button->addAction(i, this, Supla::ON_CLICK_1);
      button->addAction(kWakeAction, this, Supla::ON_PRESS);
      if (config.buttonCaptions[i]) {
        auto trigger = new Supla::Control::ActionTrigger();
        trigger->attach(button);
        if (!config.buttonCaptions[i]->empty()) {
          trigger->setInitialCaption(config.buttonCaptions[i]->c_str());
        }
      }
    }
  }

  void onInit() override {
    if (!gpioOk) {
      return;
    }
    auto check = [](int channel) {
      if (!Supla::Element::getElementByChannelNumber(channel)) {
        SUPLA_LOG_ERROR("DisplayHatMini: channel %d not found, shown as --",
                        channel);
      }
    };
    for (const auto &page : config.pages) {
      for (const auto &item : page.items) {
        check(item.channel);
      }
    }
    for (const auto &alert : config.alerts) {
      check(alert.channel);
    }

    displayOk = display.begin(config.framebuffer, config.backlight);
    if (!displayOk) {
      SUPLA_LOG_ERROR("DisplayHatMini: cannot open %s, display disabled. "
                      "Add the fbtft overlay to /boot/firmware/config.txt "
                      "(docs/integrations/display-hat-mini.md)",
                      config.framebuffer.c_str());
    }
    lastActivityMs = millis();
    instance = this;
    std::atexit([] { instance->shutdown(); });
  }

  // Timer thread: only enqueue.
  void handleAction(int, int action) override {
    std::lock_guard<std::mutex> lock(mutex);
    pending.push_back(action);
  }

  void iterateAlways() override {
    if (!gpioOk) {
      return;
    }
    uint32_t now = millis();
    bool changed = processButtons(now);

    if (config.backlightTimeoutSec && backlightOn &&
        now - lastActivityMs > config.backlightTimeoutSec * 1000) {
      backlightOn = false;
      display.setBacklight(false);
    }

    for (size_t i = 0; i < config.pages.size(); i++) {
      const auto &page = config.pages[i];
      if (page.type == dhm::PageType::Chart &&
          (history[i].size() == 0 ||
           now - lastSampleMs[i] >= page.sampleSec * 1000)) {
        lastSampleMs[i] = now;
        if (auto value = readValue(page.items[0].channel,
                                   page.items[0].field)) {
          history[i].push(*value);
        }
      }
    }

    if (changed || now - lastRefreshMs >= kRefreshMs) {
      lastRefreshMs = now;
      auto led = dhm::evaluateLed(config.alerts, readValue,
                                  SuplaDevice.getCurrentStatus());
      gpio.setLed(dhm::ledOutput(led, now));
      if (displayOk && backlightOn) {
        display.render(config, nav, readValue, isRelay, history);
      }
    }
    if (displayOk) {
      display.tick();
    }
  }

 private:
  bool processButtons(uint32_t now) {
    std::vector<int> actions;
    {
      std::lock_guard<std::mutex> lock(mutex);
      actions.swap(pending);
    }
    bool changed = false;
    for (int action : actions) {
      if (action == kWakeAction) {
        if (!backlightOn) {
          // ponytail: time window, not per-press tracking; a press held
          // longer than hold+multiclick on a dark screen still acts.
          ignoreClicksUntil = now + config.holdTimeMs + config.multiclickTimeMs;
          backlightOn = true;
          display.setBacklight(true);
          changed = true;
        }
        lastActivityMs = now;
        continue;
      }
      if (static_cast<int32_t>(ignoreClicksUntil - now) > 0) {
        continue;  // the click that woke the screen does nothing else
      }
      int toggle = nav.onClick(static_cast<dhm::Button>(action), isRelay);
      if (toggle >= 0) {
        auto handler = dynamic_cast<Supla::ActionHandler *>(
            Supla::Element::getElementByChannelNumber(toggle));
        if (handler) {
          handler->handleAction(0, Supla::TOGGLE);
        }
      }
      changed = true;
    }
    return changed;
  }

  void shutdown() {
    gpio.setLed(dhm::kOff);
    if (displayOk) {
      display.shutdown();
    }
  }

  static HatController *instance;

  dhm::Config config;
  dhm::Navigator nav;
  std::vector<dhm::History> history;
  std::vector<uint32_t> lastSampleMs;
  dhm::HatGpio gpio;
  dhm::HatDisplay display;
  std::mutex mutex;
  std::vector<int> pending;
  bool gpioOk = false;
  bool displayOk = false;
  bool backlightOn = true;
  uint32_t lastActivityMs = 0;
  uint32_t lastRefreshMs = 0;
  uint32_t ignoreClicksUntil = 0;
};

HatController *HatController::instance = nullptr;

bool AddDisplayHatMini(const Supla::Linux::ChannelFactoryContext &context) {
  const auto &ch = context.channel;
  auto &yaml = context.config;
  for (auto key : {"gpio_chip", "framebuffer", "backlight",
                   "backlight_timeout_sec", "hold_time_ms",
                   "multiclick_time_ms", "buttons", "pages", "led"}) {
    yaml.getAndMarkChannelParameter(ch, key);
  }

  dhm::Config config;
  std::string error;
  if (!dhm::parseConfig(ch, &config, &error)) {
    SUPLA_LOG_ERROR("Channel[%d] config: %s", context.channelNumber,
                    error.c_str());
    return false;
  }

  auto controller = new HatController(std::move(config));
  if (controller->openGpio()) {
    controller->createButtons();
  }
  // GPIO failure is logged; the rest of the gateway keeps running.
  return true;
}

}  // namespace

namespace Supla {
namespace Linux {

void initDisplayHatMiniExtension() {
  ChannelFactoryRegistry::instance().registerFactory(
      "displayhatmini", "DisplayHatMini", AddDisplayHatMini);
}

}  // namespace Linux
}  // namespace Supla
```

Note `nav(config.pages)` keeps a reference to `config.pages`: `config` is declared before
`nav`, so it is initialised first and never moved afterwards. Keep that member order.

**Step 3: Build script** `extras/examples/linux/build-displayhatmini.sh` (copy of
`build-byd.sh`, `chmod +x`):

```bash
#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
BUILD_DIR="${ROOT}/build-linux-displayhatmini"

cmake -S "${ROOT}/extras/examples/linux" -B "${BUILD_DIR}" \
  -DSUPLA_LINUX_EXTENSION_DIRS="${ROOT}/extras/porting/linux/extensions/displayhatmini"

cmake --build "${BUILD_DIR}" -j"$(nproc)"

echo "Built supla-device-linux with Display HAT Mini extension in ${BUILD_DIR}"
```

**Step 4: Build on the dev box** (needs `libgpiod-dev`)

```bash
rtk err extras/examples/linux/build-displayhatmini.sh
```

Expected: builds `supla-device-linux` with no errors. Fix LVGL API mismatches against the
fetched headers (see Task 6 note) — do not bump the LVGL tag to dodge them.

**Step 5: Lint + unit tests still green**

```bash
./check_cpplint.sh
rtk cmake --build extras/test/build-dhm -j"$(nproc)" --target sd4linuxtests && extras/test/build-dhm/sd4linuxtests
```

**Step 6: Commit**

```bash
rtk git add extras/porting/linux/extensions/displayhatmini extras/examples/linux/build-displayhatmini.sh
rtk git commit -m "feat(displayhatmini): sd4linux extension with buttons, LED and LVGL display"
```

---

### Task 8: Example config + docs

**Files:**
- Create: `extras/examples/linux/supla-device-displayhatmini.yaml`
- Create: `docs/integrations/display-hat-mini.md`
- Modify: `docs/index.md` (add a link next to the other integrations — note the file already has
  uncommitted changes in the main checkout; edit only in this worktree)

**Step 1: Example YAML** — the design's YAML block wrapped in a full config
(`name`, `log_level`, `state_files_path`, `supla:`), with two `VirtualRelay` channels so it runs with no other hardware:

```yaml
name: Display HAT Mini example
log_level: info
state_files_path: "/var/local/user/supla-device-displayhatmini"
security_level: 0

supla:
  server: svrXyz.supla.org
  mail: user@mail.com

channels:
  - type: VirtualRelay          # channel 0
    initial_caption: Pump
  - type: VirtualRelay          # channel 1
    initial_caption: Heater

  - type: DisplayHatMini        # keep last: its action triggers take the next numbers
    gpio_chip: gpiochip0
    backlight: /sys/class/backlight/fb_st7789v
    buttons:
      a: { caption: "HAT A" }
      b: { caption: "HAT B" }
      x: { caption: "HAT X" }
      y: { caption: "HAT Y" }
    pages:
      - title: Relays
        items:
          - { channel: 0, label: Pump }
          - { channel: 1, label: Heater }
      - title: Pump history
        type: chart
        channel: 0
        sample_sec: 5
        min: 0
        max: 1
    led:
      alerts:
        - { channel: 1, above: 0.5, color: yellow }
```

Validate: `./build-linux-displayhatmini/supla-device-linux --config extras/examples/linux/supla-device-displayhatmini.yaml`
on the dev box → config parses; GPIO error logged ("HAT disabled"), process keeps running.
(Check the exact CLI flag in `extras/examples/linux/main.cpp` cxxopts setup.)

**Step 2: `docs/integrations/display-hat-mini.md`** sections:
1. Hardware & pins (table from the design doc).
2. Pi setup: the working overlay line from Task 0, `raspi-config` SPI, `sudo usermod -aG gpio,video <service user>`, and that the backlight path comes from `ls /sys/class/backlight`.
3. Build: `extras/examples/linux/build-displayhatmini.sh`, `apt install libgpiod-dev pkg-config`.
4. YAML reference: every key with default (from `Config` in `hat_model.h`), fields list with units (`power_active` W, `fwd_act_energy` kWh, `voltage` V of phase 1, `current` A summed), colours list, button mapping, channel-numbering note (put the entry last).
5. Behaviour: status colours, alert first-match, backlight timeout + wake-click swallowing, failures table from the design doc.
6. Hardware smoke checklist (Task 9).

**Step 3: Commit** `docs(displayhatmini): integration guide and example config`.

---

### Task 9: On-hardware smoke test (manual, on the Pi)

Build on the Pi with `extras/examples/linux/build-displayhatmini.sh`, run with the example
config (adjusted `gpio_chip`/`backlight` from Task 0), and tick each:

- [ ] Startup: LED blinks blue while connecting, turns green after registration.
- [ ] LCD shows "Relays" page; B → "Pump history" chart; A → back.
- [ ] X moves the highlight; Y on Pump toggles it, app shows the change within ~1 s.
- [ ] Toggle Heater from the app → LED turns yellow (alert) while on, green when off.
- [ ] Hold A ~1 s and double-click B → action triggers visible in the SUPLA app (configure an action on them first).
- [ ] Wait `backlight_timeout_sec` → backlight off; press X → backlight on, page unchanged.
- [ ] Unplug network → LED goes blue blinking / red; replug → green.
- [ ] `systemctl stop` / Ctrl-C → LED off, screen black, backlight off.
- [ ] Remove overlay line, reboot → log shows the fb error, buttons + LED still work.

Fix anything found (TDD in `hat_model` where the bug is logic), then commit.

---

### Task 10: Finish

Use superpowers:verification-before-completion (full `sd4linuxtests`, `check_cpplint.sh`,
extension build), then superpowers:finishing-a-development-branch.

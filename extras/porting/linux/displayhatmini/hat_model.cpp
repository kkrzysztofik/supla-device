/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include "hat_model.h"

#include <SuplaDevice.h>

#include <algorithm>
#include <cstdio>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

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
      {"red", kRed},     {"green", kGreen},   {"yellow", kYellow},
      {"blue", kBlue},   {"magenta", kMagenta}, {"cyan", kCyan},
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

}  // namespace DisplayHatMini
}  // namespace Linux
}  // namespace Supla

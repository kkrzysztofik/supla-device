/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include "hat_display.h"

#include <supla/time.h>
#include <unistd.h>

#include <cmath>
#include <fstream>
#include <string>
#include <vector>

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

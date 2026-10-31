/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

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

/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include "hat_gpio.h"

#include <gpiod.h>

#include <string>

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

/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

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

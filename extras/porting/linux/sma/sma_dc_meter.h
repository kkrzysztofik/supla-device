/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#ifndef EXTRAS_PORTING_LINUX_SMA_SMA_DC_METER_H_
#define EXTRAS_PORTING_LINUX_SMA_SMA_DC_METER_H_

#include <map>
#include <string>

#include "sma_inverter.h"

namespace Supla {
namespace PV {

class SmaDcMeter : public SmaInverter {
 public:
  using SmaInverter::SmaInverter;

 protected:
  void applyMappedReadings(
      const std::map<std::string, double>& values) override;
};

}  // namespace PV
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SMA_SMA_DC_METER_H_

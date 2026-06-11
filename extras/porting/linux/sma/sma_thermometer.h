/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#ifndef EXTRAS_PORTING_LINUX_SMA_SMA_THERMOMETER_H_
#define EXTRAS_PORTING_LINUX_SMA_SMA_THERMOMETER_H_

#include <supla/sensor/thermometer.h>

#include <string>
#include <vector>

#include "sma_bus_client.h"
#include "sma_meter_channel.h"

namespace Supla {
namespace PV {

class SmaThermometer : public Supla::Sensor::Thermometer {
 public:
  SmaThermometer(std::string serialDevice,
                 int baud,
                 Supla::Linux::Sma::SerialMedia media,
                 uint16_t netAddress,
                 int pollIntervalSec,
                 std::vector<SmaMappedChannel> channels);
  ~SmaThermometer() override;

  void onInit() override;
  double getValue() override;

 private:
  std::string channelKey_;
  Supla::Linux::Sma::SmaBusClient busClient_;
  int staleReadCounter_ = 0;
};

}  // namespace PV
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SMA_SMA_THERMOMETER_H_

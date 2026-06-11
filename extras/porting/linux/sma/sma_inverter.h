/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#ifndef EXTRAS_PORTING_LINUX_SMA_SMA_INVERTER_H_
#define EXTRAS_PORTING_LINUX_SMA_SMA_INVERTER_H_

#include <supla/sensor/electricity_meter.h>

#include <map>
#include <string>
#include <vector>

#include "sma_bus_client.h"
#include "sma_meter_channel.h"

namespace Supla {
namespace PV {

class SmaInverter : public Supla::Sensor::ElectricityMeter {
 public:
  SmaInverter(std::string serialDevice,
              int baud,
              Supla::Linux::Sma::SerialMedia media,
              uint16_t netAddress,
              int pollIntervalSec,
              std::vector<SmaMappedChannel> channels);
  ~SmaInverter() override;

  void onInit() override;
  void iterateAlways() override;

 protected:
  virtual void applyMappedReadings(const std::map<std::string, double>& values);

  Supla::Linux::Sma::SmaBusClient busClient_;
  int pollIntervalSec_;
  int invDisabledCounter_ = 0;

 private:
  void applyReadingsToChannel();
  void setZeroValues();
};

}  // namespace PV
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SMA_SMA_INVERTER_H_

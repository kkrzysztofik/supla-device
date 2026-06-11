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

#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "sma_cinfo_parser.h"
#include "sma_types.h"

namespace Supla {
namespace PV {

struct SmaMappedChannel {
  Supla::Linux::Sma::SmaChannelDescriptor descriptor;
  std::string key;
  std::string smaName;
  bool resolveByName = false;
};

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

 private:
  struct CachedReadings {
    bool valid = false;
  };

  void workerLoop();
  void applyReadingsToChannel();
  void setZeroValues();

  std::string serialDevice_;
  int baud_;
  Supla::Linux::Sma::SerialMedia media_;
  uint16_t netAddress_;
  int pollIntervalSec_;
  std::vector<SmaMappedChannel> channels_;

  std::thread worker_;
  std::atomic<bool> stopWorker_{false};
  std::mutex cacheMutex_;
  CachedReadings cache_;
  std::map<std::string, double> valuesByKey_;
  int invDisabledCounter_ = 0;
  bool cinfoChecked_ = false;
  bool useNameBasedConfig_ = false;
  std::vector<Supla::Linux::Sma::SmaChannelInfo> channelCatalog_;
  bool channelsResolved_ = false;
};

}  // namespace PV
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SMA_SMA_INVERTER_H_

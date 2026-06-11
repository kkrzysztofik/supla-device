/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#ifndef EXTRAS_PORTING_LINUX_SMA_SMA_BUS_CLIENT_H_
#define EXTRAS_PORTING_LINUX_SMA_SMA_BUS_CLIENT_H_

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "sma_bus.h"
#include "sma_meter_channel.h"

namespace Supla {
namespace Linux {
namespace Sma {

class SmaBusClient {
 public:
  SmaBusClient(void* owner,
               SmaBusConfig config,
               std::vector<Supla::PV::SmaMappedChannel> channels);
  ~SmaBusClient();

  void attach();
  void detach();

  bool copyReadings(std::map<std::string, double>* values, bool* valid) const;
  const std::vector<Supla::PV::SmaMappedChannel>& channels() const;

 private:
  void* owner_;
  SmaBusConfig config_;
  std::vector<Supla::PV::SmaMappedChannel> channels_;

  std::shared_ptr<SmaBus> bus_;
  mutable std::mutex cacheMutex_;
  std::map<std::string, double> valuesByKey_;
  bool cacheValid_ = false;
};

}  // namespace Sma
}  // namespace Linux
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SMA_SMA_BUS_CLIENT_H_

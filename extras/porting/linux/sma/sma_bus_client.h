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
  SmaBusClient(const SmaBusClient&) = delete;
  SmaBusClient& operator=(const SmaBusClient&) = delete;
  SmaBusClient(SmaBusClient&&) = delete;
  SmaBusClient& operator=(SmaBusClient&&) = delete;
  ~SmaBusClient();

  void attach();
  void detach();

  bool copyReadings(std::map<std::string, double>* values, bool* valid) const;
  bool copyChannels(std::vector<Supla::PV::SmaMappedChannel>* channels) const;

 private:
  SmaBusConfig config_;
  std::shared_ptr<SmaBus::Subscriber::State> state_;

  std::shared_ptr<SmaBus> bus_;
};

void shutdownAllClients();

}  // namespace Sma
}  // namespace Linux
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SMA_SMA_BUS_CLIENT_H_

/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include "sma_bus_client.h"

namespace Supla {
namespace Linux {
namespace Sma {

SmaBusClient::SmaBusClient(void* owner,
                           SmaBusConfig config,
                           std::vector<Supla::PV::SmaMappedChannel> channels)
    : owner_(owner),
      config_(std::move(config)),
      channels_(std::move(channels)) {}

SmaBusClient::~SmaBusClient() {
  detach();
}

void SmaBusClient::attach() {
  if (bus_) {
    return;
  }

  bus_ = SmaBus::acquire(config_);
  SmaBus::Subscriber subscriber;
  subscriber.owner = owner_;
  subscriber.channels = &channels_;
  subscriber.cacheMutex = &cacheMutex_;
  subscriber.valuesByKey = &valuesByKey_;
  subscriber.cacheValid = &cacheValid_;
  bus_->subscribe(subscriber);
}

void SmaBusClient::detach() {
  if (bus_) {
    bus_->unsubscribe(owner_);
    bus_.reset();
  }
}

const std::vector<Supla::PV::SmaMappedChannel>& SmaBusClient::channels()
    const {
  return channels_;
}

bool SmaBusClient::copyReadings(std::map<std::string, double>* values,
                                bool* valid) const {
  if (values == nullptr || valid == nullptr) {
    return false;
  }

  std::lock_guard<std::mutex> lock(cacheMutex_);
  *values = valuesByKey_;
  *valid = cacheValid_;
  return true;
}

}  // namespace Sma
}  // namespace Linux
}  // namespace Supla

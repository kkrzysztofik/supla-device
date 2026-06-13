/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include "sma_bus_client.h"

#include <algorithm>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace {

std::mutex gClientsMutex;
std::vector<Supla::Linux::Sma::SmaBusClient*> gClients;

}  // namespace

namespace Supla {
namespace Linux {
namespace Sma {

SmaBusClient::SmaBusClient(void* owner,
                           SmaBusConfig config,
                           std::vector<Supla::PV::SmaMappedChannel> channels)
    : config_(std::move(config)),
      state_(std::make_shared<SmaBus::Subscriber::State>()) {
  state_->owner = owner;
  state_->channels = std::move(channels);
  std::lock_guard<std::mutex> lock(gClientsMutex);
  gClients.push_back(this);
}

SmaBusClient::~SmaBusClient() {
  detach();
  std::lock_guard<std::mutex> lock(gClientsMutex);
  gClients.erase(std::remove(gClients.begin(), gClients.end(), this),
                 gClients.end());
}

void SmaBusClient::attach() {
  if (bus_) {
    return;
  }

  bus_ = SmaBus::acquire(config_);
  SmaBus::Subscriber subscriber;
  subscriber.state = state_;
  bus_->subscribe(subscriber);
}

void SmaBusClient::detach() {
  if (bus_) {
    bus_->unsubscribe(state_->owner);
    bus_.reset();
  }
}

bool SmaBusClient::copyChannels(
    std::vector<Supla::PV::SmaMappedChannel>* channels) const {
  if (channels == nullptr) {
    return false;
  }

  std::lock_guard<std::mutex> lock(state_->mutex);
  *channels = state_->channels;
  return true;
}

bool SmaBusClient::copyReadings(std::map<std::string, double>* values,
                                bool* valid) const {
  if (values == nullptr || valid == nullptr) {
    return false;
  }

  std::lock_guard<std::mutex> lock(state_->mutex);
  *values = state_->valuesByKey;
  *valid = state_->cacheValid;
  return true;
}

void shutdownAllClients() {
  std::vector<SmaBusClient*> clients;
  {
    std::lock_guard<std::mutex> lock(gClientsMutex);
    clients = gClients;
    for (auto* client : clients) {
      if (client) {
        client->detach();
      }
    }
  }
}

}  // namespace Sma
}  // namespace Linux
}  // namespace Supla

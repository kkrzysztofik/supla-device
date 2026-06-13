/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include "ingecon_bus_client.h"

#include <algorithm>
#include <memory>
#include <mutex>
#include <vector>
#include <utility>

namespace Supla {
namespace Linux {
namespace Ingecon {

namespace {

std::mutex gClientsMutex;
std::vector<BusClient*> gClients;

}  // namespace

BusClient::BusClient(void* owner, BusConfig config)
    : config_(std::move(config)),
      state_(std::make_shared<Bus::Subscriber::State>()) {
  state_->owner = owner;
  std::lock_guard<std::mutex> lock(gClientsMutex);
  gClients.push_back(this);
}

BusClient::~BusClient() {
  detach();
  std::lock_guard<std::mutex> lock(gClientsMutex);
  gClients.erase(std::remove(gClients.begin(), gClients.end(), this),
                 gClients.end());
}

void BusClient::attach() {
  if (bus_) {
    return;
  }

  bus_ = Bus::acquire(config_);
  Bus::Subscriber subscriber;
  subscriber.state = state_;
  bus_->subscribe(subscriber);
}

void BusClient::detach() {
  if (bus_) {
    bus_->unsubscribe(state_->owner);
    bus_.reset();
  }
}

bool BusClient::copyReadings(Readings* readings, bool* valid) const {
  if (readings == nullptr || valid == nullptr || !state_) {
    return false;
  }

  std::lock_guard<std::mutex> lock(state_->mutex);
  *valid = state_->cacheValid;
  if (state_->cacheValid) {
    *readings = state_->readings;
  }
  return true;
}

#ifdef SUPLA_TEST
void BusClient::setReadingsForTest(const Readings& readings, bool valid) {
  std::lock_guard<std::mutex> lock(state_->mutex);
  state_->readings = readings;
  state_->cacheValid = valid;
}
#endif

void shutdownAllClients() {
  std::vector<BusClient*> clients;
  {
    std::lock_guard<std::mutex> lock(gClientsMutex);
    clients = gClients;
  }
  for (auto* client : clients) {
    if (client != nullptr) {
      client->detach();
    }
  }
}

}  // namespace Ingecon
}  // namespace Linux
}  // namespace Supla

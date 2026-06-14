/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include "ingecon_bus_client.h"

#include <memory>
#include <mutex>
#include <utility>

#include "linux_shared_bus_client_helpers.h"

namespace Supla {
namespace Linux {
namespace Ingecon {

namespace {

Supla::Linux::SharedBusClientRegistry<BusClient> gClients;

}  // namespace

BusClient::BusClient(void* owner, BusConfig config)
    : config_(std::move(config)),
      state_(std::make_shared<Bus::Subscriber::State>()) {
  state_->owner = owner;
  gClients.add(this);
}

BusClient::~BusClient() {
  detach();
  gClients.remove(this);
}

void BusClient::attach() {
  Supla::Linux::attachSharedBusClient(config_, state_, &bus_);
}

void BusClient::detach() {
  Supla::Linux::detachSharedBusClient(state_, &bus_);
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
  Supla::Linux::shutdownSharedBusClients(gClients);
}

}  // namespace Ingecon
}  // namespace Linux
}  // namespace Supla

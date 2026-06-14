/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#ifndef EXTRAS_PORTING_LINUX_LINUX_SHARED_BUS_CLIENT_HELPERS_H_
#define EXTRAS_PORTING_LINUX_LINUX_SHARED_BUS_CLIENT_HELPERS_H_

#include <algorithm>
#include <memory>
#include <mutex>
#include <vector>

namespace Supla {
namespace Linux {

template <typename ClientT>
class SharedBusClientRegistry {
 public:
  void add(ClientT* client) {
    std::lock_guard<std::mutex> lock(mutex_);
    clients_.push_back(client);
  }

  void remove(ClientT* client) {
    std::lock_guard<std::mutex> lock(mutex_);
    clients_.erase(std::remove(clients_.begin(), clients_.end(), client),
                   clients_.end());
  }

  std::vector<ClientT*> snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return clients_;
  }

 private:
  mutable std::mutex mutex_;
  std::vector<ClientT*> clients_;
};

template <typename BusT, typename ConfigT, typename StateT>
void attachSharedBusClient(const ConfigT& config,
                           const std::shared_ptr<StateT>& state,
                           std::shared_ptr<BusT>* bus) {
  if (bus == nullptr || *bus || !state) {
    return;
  }

  *bus = BusT::acquire(config);
  typename BusT::Subscriber subscriber;
  subscriber.state = state;
  (*bus)->subscribe(subscriber);
}

template <typename BusT, typename StateT>
void detachSharedBusClient(const std::shared_ptr<StateT>& state,
                           std::shared_ptr<BusT>* bus) {
  if (bus == nullptr || !*bus) {
    return;
  }

  void* owner = state ? state->owner : nullptr;
  (*bus)->unsubscribe(owner);
  bus->reset();
}

template <typename ClientT>
void shutdownSharedBusClients(
    const SharedBusClientRegistry<ClientT>& registry) {
  std::vector<ClientT*> clients = registry.snapshot();
  for (auto* client : clients) {
    if (client != nullptr) {
      client->detach();
    }
  }
}

}  // namespace Linux
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_LINUX_SHARED_BUS_CLIENT_HELPERS_H_

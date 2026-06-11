/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include "sma_bus.h"

#include <supla/log_wrapper.h>

#include <algorithm>
#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "sma_serial_port.h"
#include "smadata_client.h"

namespace Supla {
namespace Linux {
namespace Sma {

namespace {

struct SmaBusKey {
  std::string serialDevice;
  int baud = 0;
  SerialMedia media = SerialMedia::RS485;
  uint16_t netAddress = 0;

  bool operator<(const SmaBusKey& other) const {
    if (serialDevice != other.serialDevice) {
      return serialDevice < other.serialDevice;
    }
    if (baud != other.baud) {
      return baud < other.baud;
    }
    if (media != other.media) {
      return media < other.media;
    }
    return netAddress < other.netAddress;
  }
};

std::mutex gRegistryMutex;
std::map<SmaBusKey, std::weak_ptr<SmaBus>> gBuses;

bool subscriberUsesNameResolution(const SmaBus::Subscriber& subscriber) {
  if (subscriber.channels == nullptr) {
    return false;
  }
  for (const auto& mapped : *subscriber.channels) {
    if (mapped.resolveByName) {
      return true;
    }
  }
  return false;
}

void resolveSubscriberChannels(SmaBus::Subscriber* subscriber,
                               const std::vector<SmaChannelInfo>& catalog) {
  if (subscriber == nullptr || subscriber->channels == nullptr) {
    return;
  }

  for (auto& mapped : *subscriber->channels) {
    if (!mapped.resolveByName || mapped.descriptor.ctype != 0) {
      continue;
    }
    const std::string& lookupName =
        mapped.smaName.empty() ? mapped.key : mapped.smaName;
    const auto* info = SmaCinfoParser::findByName(catalog, lookupName);
    if (info == nullptr) {
      SUPLA_LOG_WARNING("SmaBus: SMA channel \"%s\" not in CINFO",
                        lookupName.c_str());
      continue;
    }
    const char* suplaMapping = mapped.descriptor.suplaMapping;
    mapped.descriptor = info->descriptor;
    mapped.descriptor.suplaMapping = suplaMapping;
  }
}

}  // namespace

std::shared_ptr<SmaBus> SmaBus::acquire(const SmaBusConfig& config) {
  const SmaBusKey key{config.serialDevice,
                      config.baud,
                      config.media,
                      config.netAddress};

  std::lock_guard<std::mutex> lock(gRegistryMutex);
  auto it = gBuses.find(key);
  if (it != gBuses.end()) {
    if (auto bus = it->second.lock()) {
      return bus;
    }
  }

  auto bus = std::shared_ptr<SmaBus>(new SmaBus(config));
  gBuses[key] = bus;
  return bus;
}

SmaBus::SmaBus(SmaBusConfig config) : config_(std::move(config)) {}

void SmaBus::subscribe(Subscriber subscriber) {
  if (subscriber.owner == nullptr) {
    return;
  }

  {
    std::lock_guard<std::mutex> lock(subscribersMutex_);
    subscribers_.push_back(subscriber);
  }
  startWorkerIfNeeded();
}

void SmaBus::unsubscribe(void* owner) {
  {
    std::lock_guard<std::mutex> lock(subscribersMutex_);
    subscribers_.erase(
        std::remove_if(subscribers_.begin(),
                       subscribers_.end(),
                       [owner](const Subscriber& subscriber) {
                         return subscriber.owner == owner;
                       }),
        subscribers_.end());
  }
  stopWorkerIfIdle();
}

void SmaBus::startWorkerIfNeeded() {
  std::lock_guard<std::mutex> lock(subscribersMutex_);
  if (workerRunning_) {
    return;
  }
  stopWorker_ = false;
  workerRunning_ = true;
  worker_ = std::thread([this]() { workerLoop(); });
}

void SmaBus::stopWorkerIfIdle() {
  bool shouldStop = false;
  {
    std::lock_guard<std::mutex> lock(subscribersMutex_);
    shouldStop = subscribers_.empty() && workerRunning_;
  }

  if (!shouldStop) {
    return;
  }

  stopWorker_ = true;
  if (worker_.joinable()) {
    worker_.join();
  }
  workerRunning_ = false;
  channelCatalog_.clear();
  cinfoChecked_ = false;
}

void SmaBus::workerLoop() {
  SmaSerialPort port(config_.serialDevice, config_.baud, config_.media);
  const uint16_t masterAddr = 0;
  const int pollIntervalSec =
      config_.pollIntervalSec > 0 ? config_.pollIntervalSec : 15;

  while (!stopWorker_) {
    std::vector<Subscriber> subscribers;
    {
      std::lock_guard<std::mutex> lock(subscribersMutex_);
      if (subscribers_.empty()) {
        break;
      }
      for (auto& subscriber : subscribers_) {
        resolveSubscriberChannels(&subscriber, channelCatalog_);
      }
      subscribers = subscribers_;
    }

    if (!port.isOpen() && !port.open()) {
      SUPLA_LOG_WARNING("SmaBus: failed to open %s",
                        config_.serialDevice.c_str());
      std::this_thread::sleep_for(std::chrono::seconds(5));
      continue;
    }

    SmaDataClient client(port, masterAddr, config_.netAddress);

    bool useNameBasedConfig = false;
    for (const auto& subscriber : subscribers) {
      if (subscriberUsesNameResolution(subscriber)) {
        useNameBasedConfig = true;
        break;
      }
    }

    if (useNameBasedConfig && channelCatalog_.empty()) {
      auto catalog = client.fetchChannelList();
      if (!catalog) {
        SUPLA_LOG_WARNING(
            "SmaBus: CMD_GET_CINFO failed — check net_address and wiring");
        const int backoff = client.backoffSec();
        port.close();
        std::this_thread::sleep_for(
            std::chrono::seconds(backoff > 0 ? backoff : pollIntervalSec));
        continue;
      }
      channelCatalog_ = std::move(*catalog);
    } else if (!cinfoChecked_) {
      if (!client.verifyCinfo()) {
        SUPLA_LOG_WARNING(
            "SmaBus: CMD_GET_CINFO failed — check net_address and wiring");
      }
      cinfoChecked_ = true;
    }

    std::map<std::pair<uint16_t, uint8_t>, double> bulkValues;
    bool pollOk = true;

    if (useNameBasedConfig) {
      if (!client.readSpotChannelsBulk(channelCatalog_, &bulkValues)) {
        SUPLA_LOG_DEBUG("SmaBus: bulk spot read failed");
        pollOk = false;
      }
    }

    for (auto& subscriber : subscribers) {
      std::map<std::string, double> readings;
      if (!pollOk) {
        if (subscriber.cacheMutex != nullptr && subscriber.cacheValid != nullptr) {
          std::lock_guard<std::mutex> lock(*subscriber.cacheMutex);
          *subscriber.cacheValid = false;
        }
        continue;
      }

      if (subscriber.channels == nullptr) {
        continue;
      }

      if (useNameBasedConfig) {
        for (const auto& mapped : *subscriber.channels) {
          if (mapped.descriptor.ctype == 0 && mapped.resolveByName) {
            continue;
          }
          const auto descriptorKey = std::make_pair(mapped.descriptor.ctype,
                                                    mapped.descriptor.cindex);
          const auto it = bulkValues.find(descriptorKey);
          if (it == bulkValues.end()) {
            SUPLA_LOG_DEBUG("SmaBus: no bulk value for channel %s",
                            mapped.key.c_str());
            pollOk = false;
            break;
          }
          readings[mapped.key] = it->second;
        }
      } else {
        for (const auto& mapped : *subscriber.channels) {
          double value = 0.0;
          if (!client.readChannel(mapped.descriptor, &value)) {
            SUPLA_LOG_DEBUG("SmaBus: read failed for channel %s",
                            mapped.key.c_str());
            pollOk = false;
            break;
          }
          readings[mapped.key] = value;
        }
      }

      if (!pollOk) {
        if (subscriber.cacheMutex != nullptr && subscriber.cacheValid != nullptr) {
          std::lock_guard<std::mutex> lock(*subscriber.cacheMutex);
          *subscriber.cacheValid = false;
        }
        continue;
      }

      if (subscriber.cacheMutex != nullptr && subscriber.valuesByKey != nullptr &&
          subscriber.cacheValid != nullptr) {
        std::lock_guard<std::mutex> lock(*subscriber.cacheMutex);
        *subscriber.valuesByKey = readings;
        *subscriber.cacheValid = true;
      }
    }

    if (!pollOk) {
      const int backoff = client.backoffSec();
      port.close();
      std::this_thread::sleep_for(
          std::chrono::seconds(backoff > 0 ? backoff : pollIntervalSec));
      continue;
    }

    std::this_thread::sleep_for(std::chrono::seconds(pollIntervalSec));
  }

  port.close();
  workerRunning_ = false;
}

}  // namespace Sma
}  // namespace Linux
}  // namespace Supla

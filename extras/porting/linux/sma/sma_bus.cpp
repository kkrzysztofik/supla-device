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
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "sma_profile_loader.h"
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
  if (!subscriber.state) {
    return false;
  }

  std::lock_guard<std::mutex> lock(subscriber.state->mutex);
  for (const auto& mapped : subscriber.state->channels) {
    if (mapped.resolveByName) {
      return true;
    }
  }
  return false;
}

bool resolveSubscriberChannels(SmaBus::Subscriber* subscriber,
                               const std::vector<SmaChannelInfo>& catalog) {
  if (subscriber == nullptr || !subscriber->state) {
    return true;
  }

  bool allResolved = true;
  std::lock_guard<std::mutex> lock(subscriber->state->mutex);
  for (auto& mapped : subscriber->state->channels) {
    if (!mapped.resolveByName || mapped.descriptor.ctype != 0) {
      continue;
    }
    const std::string& lookupName =
        mapped.smaName.empty() ? mapped.key : mapped.smaName;
    const auto* info = SmaCinfoParser::findByName(catalog, lookupName);
    if (info == nullptr) {
      SUPLA_LOG_WARNING("SmaBus: SMA channel \"%s\" not in channel catalog",
                        lookupName.c_str());
      allResolved = false;
      continue;
    }
    const char* suplaMapping = mapped.descriptor.suplaMapping;
    mapped.descriptor = info->descriptor;
    mapped.descriptor.suplaMapping = suplaMapping;
    SUPLA_LOG_VERBOSE(
        "SmaBus: resolved \"%s\" ctype=0x%04x cindex=%u ntype=0x%04x gain=%g",
        lookupName.c_str(),
        mapped.descriptor.ctype,
        mapped.descriptor.cindex,
        mapped.descriptor.ntype,
        static_cast<double>(mapped.descriptor.gain));
  }
  return allResolved;
}

std::optional<std::vector<SmaChannelInfo>> loadChannelCatalog(
    SmaDataClient& client,
    const SmaBusConfig& config,
    const SmaDetectedDevice& detected) {
  if (!config.deviceProfile.empty()) {
    if (auto catalog = SmaProfileLoader::resolveProfile(config.deviceProfile)) {
      SUPLA_LOG_INFO("SmaBus: using configured profile \"%s\" (%zu channels)",
                     config.deviceProfile.c_str(),
                     catalog->size());
      return catalog;
    }
    SUPLA_LOG_WARNING("SmaBus: profile \"%s\" not found",
                      config.deviceProfile.c_str());
  }

  if (auto catalog = client.fetchChannelList()) {
    return catalog;
  }

  if (auto catalog = SmaProfileLoader::resolveProfile(detected.type)) {
    SUPLA_LOG_INFO("SmaBus: CMD_GET_CINFO unavailable, using profile for %s",
                   detected.type.c_str());
    return catalog;
  }

  SUPLA_LOG_WARNING(
      "SmaBus: no profile for detected device type \"%s\" — export "
      "yasdi/build/devices/%s.bin to ./sma-profiles/",
      detected.type.c_str(),
      detected.type.c_str());

  return std::nullopt;
}

int retryDelaySec(const SmaDataClient& client, int pollIntervalSec) {
  const int backoff = client.backoffSec();
  return backoff > 0 ? backoff : pollIntervalSec;
}

void closeAndSleep(SmaSerialPort* port, int delaySec) {
  if (port != nullptr) {
    port->close();
  }
  std::this_thread::sleep_for(std::chrono::seconds(delaySec));
}

bool subscribersUseNameResolution(
    const std::vector<SmaBus::Subscriber>& subscribers) {
  for (const auto& subscriber : subscribers) {
    if (subscriberUsesNameResolution(subscriber)) {
      return true;
    }
  }
  return false;
}

std::vector<Supla::PV::SmaMappedChannel> copySubscriberChannels(
    const std::shared_ptr<SmaBus::Subscriber::State>& state) {
  std::lock_guard<std::mutex> lock(state->mutex);
  return state->channels;
}

void storeSubscriberReadings(
    const std::shared_ptr<SmaBus::Subscriber::State>& state,
    const std::map<std::string, double>& readings,
    bool valid) {
  std::lock_guard<std::mutex> lock(state->mutex);
  state->cacheValid = valid;
  if (valid) {
    state->valuesByKey = readings;
  }
}

bool readSubscriberChannels(
    SmaDataClient* client,
    bool useNameBasedConfig,
    const std::map<std::string, double>& bulkValues,
    const std::vector<Supla::PV::SmaMappedChannel>& channels,
    std::map<std::string, double>* readings) {
  if (client == nullptr || readings == nullptr) {
    return false;
  }

  for (const auto& mapped : channels) {
    const std::string& lookupName =
        mapped.smaName.empty() ? mapped.key : mapped.smaName;

    if (useNameBasedConfig) {
      if (mapped.descriptor.ctype == 0 && mapped.resolveByName) {
        continue;
      }

      const auto it = bulkValues.find(lookupName);
      if (it != bulkValues.end()) {
        (*readings)[mapped.key] = it->second;
        SUPLA_LOG_VERBOSE("SmaBus: subscriber value %s (%s) = %.6f",
                          mapped.key.c_str(),
                          lookupName.c_str(),
                          it->second);
        continue;
      }
    }

    double value = 0.0;
    if (!client->readChannel(mapped.descriptor, &value)) {
      if (useNameBasedConfig) {
        SUPLA_LOG_DEBUG("SmaBus: read failed for channel %s (%s)",
                        mapped.key.c_str(),
                        lookupName.c_str());
      } else {
        SUPLA_LOG_DEBUG("SmaBus: read failed for channel %s",
                        mapped.key.c_str());
      }
      return false;
    }

    (*readings)[mapped.key] = value;
    if (useNameBasedConfig) {
      SUPLA_LOG_VERBOSE(
          "SmaBus: subscriber value %s (%s) = %.6f (single-channel)",
          mapped.key.c_str(),
          lookupName.c_str(),
          value);
    }
  }

  return true;
}

}  // namespace

std::shared_ptr<SmaBus> SmaBus::acquire(const SmaBusConfig& config) {
  const SmaBusKey key{
      config.serialDevice, config.baud, config.media, config.netAddress};

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

SmaBus::SmaBus(SmaBusConfig config) : config_(std::move(config)) {
}

void SmaBus::subscribe(Subscriber subscriber) {
  if (!subscriber.state || subscriber.state->owner == nullptr) {
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
    subscribers_.erase(std::remove_if(subscribers_.begin(),
                                      subscribers_.end(),
                                      [owner](const Subscriber& subscriber) {
                                        return subscriber.state &&
                                               subscriber.state->owner == owner;
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
  if (worker_.joinable()) {
    worker_.join();
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
  {
    std::lock_guard<std::mutex> lock(subscribersMutex_);
    workerRunning_ = false;
  }
  channelCatalog_.clear();
  cinfoChecked_ = false;
}

void SmaBus::workerLoop() {
  SmaSerialPort port(config_.serialDevice, config_.baud, config_.media);
  const uint16_t masterAddr = 0;
  const int pollIntervalSec = config_.pollIntervalSec > 0
                                  ? config_.pollIntervalSec
                                  : kDefaultPollIntervalSec;

  while (!stopWorker_) {
    std::vector<Subscriber> subscribers;
    {
      std::lock_guard<std::mutex> lock(subscribersMutex_);
      if (subscribers_.empty()) {
        break;
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

    SUPLA_LOG_DEBUG(
        "SmaBus: poll cycle on %s @ %d, net_address=0x%04x, profile=\"%s\"",
        config_.serialDevice.c_str(),
        config_.baud,
        config_.netAddress,
        config_.deviceProfile.empty() ? "(auto)"
                                      : config_.deviceProfile.c_str());

    std::optional<SmaDetectedDevice> detected;
    detected =
        client.bringOnline(config_.netAddress, 20000, config_.deviceProfile);
    if (!detected) {
      const int delaySec = retryDelaySec(client, pollIntervalSec);
      SUPLA_LOG_WARNING("SmaBus: SMANet login failed, backoff %d s (errors=%d)",
                        delaySec,
                        client.backoffSec());
      closeAndSleep(&port, delaySec);
      continue;
    }

    const bool useNameBasedConfig = subscribersUseNameResolution(subscribers);

    if (useNameBasedConfig && channelCatalog_.empty()) {
      auto catalog = loadChannelCatalog(client, config_, *detected);
      if (!catalog) {
        SUPLA_LOG_WARNING(
            "SmaBus: no channel catalog — check net_address, wiring, or set "
            "device.profile (see sma/README.md)");
        closeAndSleep(&port, retryDelaySec(client, pollIntervalSec));
        continue;
      }
      channelCatalog_ = std::move(*catalog);
      if (!config_.deviceProfile.empty()) {
        cinfoChecked_ = true;
      }
    } else if (!cinfoChecked_) {
      if (!client.verifyCinfo()) {
        SUPLA_LOG_DEBUG("SmaBus: CMD_GET_CINFO probe failed (optional)");
      }
      cinfoChecked_ = true;
    }

    {
      std::lock_guard<std::mutex> lock(subscribersMutex_);
      for (auto& subscriber : subscribers_) {
        resolveSubscriberChannels(&subscriber, channelCatalog_);
      }
      subscribers = subscribers_;
    }

    std::map<std::string, double> bulkValues;
    bool bulkOk = true;

    if (useNameBasedConfig) {
      bulkOk = client.readSpotChannelsBulk(channelCatalog_, &bulkValues);
      if (!bulkOk) {
        bulkValues.clear();
      }
    }

    bool anySubscriberUpdated = false;

    for (auto& subscriber : subscribers) {
      std::map<std::string, double> readings;
      auto state = subscriber.state;

      if (!state) {
        continue;
      }
      auto channels = copySubscriberChannels(state);
      if (channels.empty()) {
        continue;
      }

      if (!readSubscriberChannels(
              &client, useNameBasedConfig, bulkValues, channels, &readings)) {
        storeSubscriberReadings(state, readings, false);
        continue;
      }

      storeSubscriberReadings(state, readings, true);
      anySubscriberUpdated = true;
    }

    if (!anySubscriberUpdated) {
      const int delaySec = retryDelaySec(client, pollIntervalSec);
      SUPLA_LOG_WARNING("SmaBus: poll failed, backoff %d s (errors=%d)",
                        delaySec,
                        client.backoffSec());
      closeAndSleep(&port, delaySec);
      continue;
    }

    SUPLA_LOG_DEBUG("SmaBus: poll OK, sleeping %d s", pollIntervalSec);
    std::this_thread::sleep_for(std::chrono::seconds(pollIntervalSec));
  }

  port.close();
  {
    std::lock_guard<std::mutex> lock(subscribersMutex_);
    workerRunning_ = false;
  }
}

}  // namespace Sma
}  // namespace Linux
}  // namespace Supla

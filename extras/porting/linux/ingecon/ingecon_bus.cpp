/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include "ingecon_bus.h"

#include <supla/log_wrapper.h>

#include <algorithm>
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "ingecon_modbus_rtu.h"
#include "ingecon_serial_port.h"

namespace Supla {
namespace Linux {
namespace Ingecon {

namespace {

constexpr uint16_t kMainRegisterAddress = 0;
constexpr uint16_t kDisplayFwRegisterAddress = 200;

struct BusKey {
  std::string serialDevice;
  int baud = 0;
  uint8_t modbusAddress = 0;

  bool operator<(const BusKey& other) const {
    if (serialDevice != other.serialDevice) {
      return serialDevice < other.serialDevice;
    }
    if (baud != other.baud) {
      return baud < other.baud;
    }
    return modbusAddress < other.modbusAddress;
  }
};

std::mutex gRegistryMutex;
std::map<BusKey, std::weak_ptr<Bus>> gBuses;

void storeSubscriberReadings(
    const std::shared_ptr<Bus::Subscriber::State>& state,
    const Readings& readings,
    bool valid) {
  if (!state) {
    return;
  }
  std::lock_guard<std::mutex> lock(state->mutex);
  state->cacheValid = valid;
  if (valid) {
    state->readings = readings;
  }
}

void invalidateSubscriberReadings(const std::vector<Bus::Subscriber>& subs) {
  for (const auto& sub : subs) {
    if (!sub.state) {
      continue;
    }
    std::lock_guard<std::mutex> lock(sub.state->mutex);
    sub.state->cacheValid = false;
  }
}

}  // namespace

std::shared_ptr<Bus> Bus::acquire(const BusConfig& config) {
  const BusKey key{config.serialDevice, config.baud, config.modbusAddress};

  std::lock_guard<std::mutex> lock(gRegistryMutex);
  auto it = gBuses.find(key);
  if (it != gBuses.end()) {
    if (auto bus = it->second.lock()) {
      return bus;
    }
  }

  auto bus = std::shared_ptr<Bus>(new Bus(config));
  gBuses[key] = bus;
  return bus;
}

Bus::Bus(BusConfig config)
    : config_(std::move(config)),
      serialPort_(config_.serialDevice, config_.baud) {
  config_.pollIntervalSec = normalizePollIntervalSec(config_.pollIntervalSec);
  config_.timeoutMs = normalizeTimeoutMs(config_.timeoutMs);
  config_.retries = normalizeRetries(config_.retries);
}

#ifdef SUPLA_TEST
void Bus::invalidateCachedReadingsForTest(
    const std::vector<Subscriber>& subscribers) {
  invalidateSubscriberReadings(subscribers);
}
#endif

void Bus::subscribe(Subscriber subscriber) {
  if (!subscriber.state || subscriber.state->owner == nullptr) {
    return;
  }

  {
    std::lock_guard<std::mutex> lock(subscribersMutex_);
    subscribers_.push_back(subscriber);
  }
  startWorkerIfNeeded();
}

void Bus::unsubscribe(void* owner) {
  {
    std::lock_guard<std::mutex> lock(subscribersMutex_);
    subscribers_.erase(std::remove_if(subscribers_.begin(),
                                      subscribers_.end(),
                                      [owner](const Subscriber& sub) {
                                        return sub.state &&
                                               sub.state->owner == owner;
                                      }),
                       subscribers_.end());
  }
  stopWorkerIfIdle();
}

void Bus::startWorkerIfNeeded() {
  std::lock_guard<std::mutex> lock(subscribersMutex_);
  if (workerRunning_.load() || subscribers_.empty()) {
    return;
  }
  stopWorker_ = false;
  worker_ = std::thread(&Bus::workerLoop, this);
  workerRunning_.store(true);
}

void Bus::stopWorkerIfIdle() {
  bool shouldStop = false;
  {
    std::lock_guard<std::mutex> lock(subscribersMutex_);
    shouldStop = workerRunning_.load() && subscribers_.empty();
  }
  if (!shouldStop) {
    return;
  }

  stopWorker_ = true;
  if (worker_.joinable()) {
    worker_.join();
  }
  stopWorker_ = false;

  std::lock_guard<std::mutex> lock(subscribersMutex_);
  workerRunning_.store(false);
}

void Bus::workerLoop() {
  SUPLA_LOG_INFO("IngeconBus: worker started for %s address=%u",
                 config_.serialDevice.c_str(),
                 config_.modbusAddress);

  while (!stopWorker_) {
    std::vector<Subscriber> subscribersCopy;
    {
      std::lock_guard<std::mutex> lock(subscribersMutex_);
      subscribersCopy = subscribers_;
    }

    Readings readings;
    const bool valid = poll(&readings);
    if (!valid) {
      invalidateSubscriberReadings(subscribersCopy);
    }
    for (const auto& sub : subscribersCopy) {
      storeSubscriberReadings(sub.state, readings, valid);
    }

    const int sleepSlices = config_.pollIntervalSec * 10;
    for (int i = 0; i < sleepSlices && !stopWorker_; ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }

  SUPLA_LOG_INFO("IngeconBus: worker stopped for %s address=%u",
                 config_.serialDevice.c_str(),
                 config_.modbusAddress);
}

bool Bus::poll(Readings* readings) {
  if (readings == nullptr) {
    return false;
  }

  for (int attempt = 0; attempt <= config_.retries; ++attempt) {
    std::vector<uint16_t> mainRegisters;
    SUPLA_LOG_DEBUG("IngeconBus: poll attempt %d/%d for %s address=%u",
                    attempt + 1,
                    config_.retries + 1,
                    config_.serialDevice.c_str(),
                    config_.modbusAddress);
    if (!readInputBlock(
            kMainRegisterAddress, kMainInputRegisterCount, &mainRegisters)) {
      SUPLA_LOG_DEBUG("IngeconBus: main block read failed on attempt %d",
                      attempt + 1);
      continue;
    }

    Readings parsed;
    if (!parseMainInputRegisters(mainRegisters, &parsed)) {
      SUPLA_LOG_WARNING("IngeconBus: main block parse failed");
      continue;
    }

    std::vector<uint16_t> displayRegisters;
    if (readInputBlock(kDisplayFwRegisterAddress,
                       kDisplayFwRegisterCount,
                       &displayRegisters)) {
      parseDisplayFwRegisters(displayRegisters, &parsed);
    } else {
      SUPLA_LOG_DEBUG("IngeconBus: optional display FW block unavailable");
    }

    *readings = parsed;
    SUPLA_LOG_DEBUG("IngeconBus: poll successful on attempt %d", attempt + 1);
    return true;
  }

  SUPLA_LOG_WARNING("IngeconBus: poll failed for %s address=%u",
                    config_.serialDevice.c_str(),
                    config_.modbusAddress);
  return false;
}

bool Bus::readInputBlock(uint16_t address,
                          uint16_t count,
                          std::vector<uint16_t>* registers) {
  if (registers == nullptr) {
    return false;
  }

  if (!serialPort_.isOpen()) {
    if (!serialPort_.open()) {
      return false;
    }
  }

  const auto request =
      buildReadInputRegistersRequest(config_.modbusAddress, address, count);
  SUPLA_LOG_VERBOSE(
      "IngeconBus: TX input registers device=%s slave=%u register=%u "
      "protocolAddress=%u count=%u frame=[%s]",
      config_.serialDevice.c_str(),
      config_.modbusAddress,
      static_cast<unsigned>(30001 + address),
      address,
      count,
      bytesToHex(request.data(), request.size()).c_str());
  serialPort_.flushRx();
  if (!serialPort_.writeAll(request.data(), request.size())) {
    SUPLA_LOG_WARNING("IngeconBus: write failed for register=%u count=%u",
                      static_cast<unsigned>(30001 + address),
                      count);
    return false;
  }

  const size_t expectedFrameLen = static_cast<size_t>(count) * 2 + 5;
  std::vector<uint8_t> response(expectedFrameLen);
  size_t received = 0;
  while (received < expectedFrameLen) {
    const ssize_t read =
        serialPort_.readSome(response.data() + received,
                      expectedFrameLen - received,
                      config_.timeoutMs);
    if (read < 0) {
      SUPLA_LOG_WARNING(
          "IngeconBus: serial read error register=%u received=%zu expected=%zu",
          static_cast<unsigned>(30001 + address),
          received,
          expectedFrameLen);
      return false;
    }
    if (read == 0) {
      SUPLA_LOG_DEBUG(
          "IngeconBus: serial read timeout register=%u received=%zu expected=%zu",
          static_cast<unsigned>(30001 + address),
          received,
          expectedFrameLen);
      break;
    }
    received += static_cast<size_t>(read);
    SUPLA_LOG_VERBOSE(
        "IngeconBus: RX chunk register=%u bytes=%zd total=%zu/%zu data=[%s]",
        static_cast<unsigned>(30001 + address),
        read,
        received,
        expectedFrameLen,
        bytesToHex(response.data(), received).c_str());
  }

  const bool parsed = parseReadInputRegistersResponse(response.data(),
                                                      received,
                                                      config_.modbusAddress,
                                                      count,
                                                      registers);
  if (parsed) {
    SUPLA_LOG_DEBUG(
        "IngeconBus: RX parsed register=%u count=%u frame=[%s]",
        static_cast<unsigned>(30001 + address),
        count,
        bytesToHex(response.data(), received).c_str());
  } else {
    SUPLA_LOG_WARNING(
        "IngeconBus: RX parse failed register=%u count=%u received=%zu "
        "frame=[%s]",
        static_cast<unsigned>(30001 + address),
        count,
        received,
        bytesToHex(response.data(), received).c_str());
  }
  serialPort_.close();
  return parsed;
}

}  // namespace Ingecon
}  // namespace Linux
}  // namespace Supla

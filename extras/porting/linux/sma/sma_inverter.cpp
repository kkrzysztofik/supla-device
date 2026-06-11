/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include "sma_inverter.h"

#include <supla/log_wrapper.h>
#include <supla/time.h>

#include <chrono>
#include <cstring>
#include <map>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "smadata_client.h"
#include "sma_cinfo_parser.h"
#include "sma_serial_port.h"

namespace Supla {
namespace PV {

namespace {

bool mappingIsPower(const char* mapping) {
  return mapping != nullptr && std::strcmp(mapping, "power_active") == 0;
}

bool mappingIsFwdEnergy(const char* mapping) {
  return mapping != nullptr && std::strcmp(mapping, "fwd_act_energy") == 0;
}

bool mappingIsVoltage(const char* mapping) {
  return mapping != nullptr && std::strcmp(mapping, "voltage") == 0;
}

bool mappingIsCurrent(const char* mapping) {
  return mapping != nullptr && std::strcmp(mapping, "current") == 0;
}

bool mappingIsFrequency(const char* mapping) {
  return mapping != nullptr && std::strcmp(mapping, "frequency") == 0;
}

}  // namespace

SmaInverter::SmaInverter(std::string serialDevice,
                           int baud,
                           Supla::Linux::Sma::SerialMedia media,
                           uint16_t netAddress,
                           int pollIntervalSec,
                           std::vector<SmaMappedChannel> channels)
    : serialDevice_(std::move(serialDevice)),
      baud_(baud),
      media_(media),
      netAddress_(netAddress),
      pollIntervalSec_(pollIntervalSec > 0 ? pollIntervalSec : 15),
      channels_(std::move(channels)) {
  for (const auto& mapped : channels_) {
    if (mapped.resolveByName) {
      useNameBasedConfig_ = true;
      break;
    }
  }
  refreshRateSec = pollIntervalSec_;
  extChannel.setFlag(SUPLA_CHANNEL_FLAG_PHASE2_UNSUPPORTED);
  extChannel.setFlag(SUPLA_CHANNEL_FLAG_PHASE3_UNSUPPORTED);
}

SmaInverter::~SmaInverter() {
  stopWorker_ = true;
  if (worker_.joinable()) {
    worker_.join();
  }
}

void SmaInverter::onInit() {
  Supla::Sensor::ElectricityMeter::onInit();
  stopWorker_ = false;
  worker_ = std::thread([this]() { workerLoop(); });
}

void SmaInverter::setZeroValues() {
  setPowerActive(0, 0);
  setCurrent(0, 0);
  setVoltage(0, 0);
  setFreq(0);
}

void SmaInverter::applyReadingsToChannel() {
  std::map<std::string, double> values;
  bool valid = false;
  {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    values = valuesByKey_;
    valid = cache_.valid;
  }

  if (!valid) {
    invDisabledCounter_++;
    if (invDisabledCounter_ > 3) {
      setZeroValues();
      updateChannelValues();
    }
    return;
  }

  invDisabledCounter_ = 0;
  bool hasPower = false;

  for (const auto& mapped : channels_) {
    auto it = values.find(mapped.key);
    if (it == values.end()) {
      continue;
    }
    const double value = it->second;
    const char* mapping = mapped.descriptor.suplaMapping;
    if (mappingIsPower(mapping)) {
      setPowerActive(0, static_cast<_supla_int_t>(value * 100000.0));
      hasPower = true;
    } else if (mappingIsFwdEnergy(mapping)) {
      setFwdActEnergy(
          0, static_cast<unsigned _supla_int64_t>(value * 100000.0));
    } else if (mappingIsVoltage(mapping)) {
      setVoltage(0, static_cast<unsigned _supla_int16_t>(value * 100.0));
    } else if (mappingIsCurrent(mapping)) {
      setCurrent(0, static_cast<unsigned _supla_int16_t>(value * 1000.0));
    } else if (mappingIsFrequency(mapping)) {
      setFreq(static_cast<unsigned _supla_int16_t>(value * 100.0));
      hasPower = true;
    }
  }

  if (!hasPower) {
    invDisabledCounter_++;
  }

  updateChannelValues();
}

void SmaInverter::iterateAlways() {
  const uint32_t pollMs =
      static_cast<uint32_t>(pollIntervalSec_ * 1000);
  if (lastReadTime == 0 || millis() - lastReadTime > pollMs) {
    lastReadTime = millis();
    applyReadingsToChannel();
  }
  Supla::Sensor::ElectricityMeter::iterateAlways();
}

void SmaInverter::workerLoop() {
  Supla::Linux::Sma::SmaSerialPort port(serialDevice_, baud_, media_);
  uint16_t masterAddr = 0;

  while (!stopWorker_) {
    if (!port.isOpen() && !port.open()) {
      SUPLA_LOG_WARNING("SmaInverter: failed to open %s",
                        serialDevice_.c_str());
      std::this_thread::sleep_for(std::chrono::seconds(5));
      continue;
    }

    Supla::Linux::Sma::SmaDataClient client(port, masterAddr, netAddress_);

    if (useNameBasedConfig_ && channelCatalog_.empty()) {
      auto catalog = client.fetchChannelList();
      if (!catalog) {
        SUPLA_LOG_WARNING(
            "SmaInverter: CMD_GET_CINFO failed — check net_address and wiring");
        const int backoff = client.backoffSec();
        port.close();
        std::this_thread::sleep_for(
            std::chrono::seconds(backoff > 0 ? backoff : pollIntervalSec_));
        continue;
      }
      channelCatalog_ = std::move(*catalog);
    } else if (!cinfoChecked_) {
      if (!client.verifyCinfo()) {
        SUPLA_LOG_WARNING(
            "SmaInverter: CMD_GET_CINFO failed — check net_address and wiring");
      }
      cinfoChecked_ = true;
    }

    if (useNameBasedConfig_ && !channelsResolved_) {
      for (auto& mapped : channels_) {
        if (!mapped.resolveByName) {
          continue;
        }
        const std::string& lookupName =
            mapped.smaName.empty() ? mapped.key : mapped.smaName;
        const auto* info =
            Supla::Linux::Sma::SmaCinfoParser::findByName(channelCatalog_,
                                                          lookupName);
        if (info == nullptr) {
          SUPLA_LOG_WARNING("SmaInverter: SMA channel \"%s\" not in CINFO",
                            lookupName.c_str());
          continue;
        }
        const char* suplaMapping = mapped.descriptor.suplaMapping;
        mapped.descriptor = info->descriptor;
        mapped.descriptor.suplaMapping = suplaMapping;
      }
      channelsResolved_ = true;
    }

    std::map<std::string, double> readings;
    bool pollOk = true;

    if (useNameBasedConfig_) {
      std::map<std::pair<uint16_t, uint8_t>, double> bulkValues;
      if (!client.readSpotChannelsBulk(channelCatalog_, &bulkValues)) {
        SUPLA_LOG_DEBUG("SmaInverter: bulk spot read failed");
        pollOk = false;
      } else {
        for (const auto& mapped : channels_) {
          if (mapped.descriptor.ctype == 0 && mapped.resolveByName) {
            continue;
          }
          const auto key = std::make_pair(mapped.descriptor.ctype,
                                          mapped.descriptor.cindex);
          const auto it = bulkValues.find(key);
          if (it == bulkValues.end()) {
            SUPLA_LOG_DEBUG("SmaInverter: no bulk value for channel %s",
                            mapped.key.c_str());
            pollOk = false;
            break;
          }
          readings[mapped.key] = it->second;
        }
      }
    } else {
      for (const auto& mapped : channels_) {
        double value = 0.0;
        if (!client.readChannel(mapped.descriptor, &value)) {
          SUPLA_LOG_DEBUG("SmaInverter: read failed for channel %s",
                          mapped.key.c_str());
          pollOk = false;
          break;
        }
        readings[mapped.key] = value;
      }
    }

    if (!pollOk) {
      const int backoff = client.backoffSec();
      port.close();
      std::this_thread::sleep_for(
          std::chrono::seconds(backoff > 0 ? backoff : pollIntervalSec_));
      continue;
    }

    {
      std::lock_guard<std::mutex> lock(cacheMutex_);
      valuesByKey_ = readings;
      cache_.valid = true;
    }

    std::this_thread::sleep_for(std::chrono::seconds(pollIntervalSec_));
  }

  port.close();
}

}  // namespace PV
}  // namespace Supla

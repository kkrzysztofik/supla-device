/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include <supla/log_wrapper.h>

#include <string>
#include <vector>

#include "linux_channel_factory.h"
#include "linux_yaml_config.h"
#include "sma_inverter.h"
#include "sma_types.h"

namespace {

Supla::Linux::Sma::SerialMedia parseMedia(const std::string& media) {
  if (media == "RS485" || media == "rs485") {
    return Supla::Linux::Sma::SerialMedia::RS485;
  }
  return Supla::Linux::Sma::SerialMedia::RS232;
}

const char* parseSuplaMapping(const std::string& mapping) {
  if (mapping == "power_active" || mapping == "pac") {
    return "power_active";
  }
  if (mapping == "fwd_act_energy" || mapping == "totwh") {
    return "fwd_act_energy";
  }
  if (mapping == "voltage" || mapping == "uac") {
    return "voltage";
  }
  if (mapping == "current" || mapping == "iac") {
    return "current";
  }
  if (mapping == "frequency" || mapping == "fac") {
    return "frequency";
  }
  return nullptr;
}

bool AddSmaInverter(const Supla::Linux::ChannelFactoryContext& context) {
  const auto& ch = context.channel;
  auto& config = context.config;

  std::string devicePath;
  int baud = 9600;
  std::string mediaStr = "RS485";
  uint16_t netAddress = 1;
  int pollIntervalSec = 15;

  if (!ch["serial"] || !ch["serial"]["device"]) {
    SUPLA_LOG_ERROR("Channel[%d] config: SmaInverter missing serial.device",
                    context.channelNumber);
    return false;
  }

  config.markChannelParameterUsed();
  devicePath = ch["serial"]["device"].as<std::string>();

  if (ch["serial"]["baud"]) {
    config.markChannelParameterUsed();
    baud = ch["serial"]["baud"].as<int>();
  }
  if (ch["serial"]["media"]) {
    config.markChannelParameterUsed();
    mediaStr = ch["serial"]["media"].as<std::string>();
  }

  if (ch["device"] && ch["device"]["net_address"]) {
    config.markChannelParameterUsed();
    netAddress = static_cast<uint16_t>(ch["device"]["net_address"].as<int>());
  }

  if (ch["poll_interval_sec"]) {
    config.markChannelParameterUsed();
    pollIntervalSec = ch["poll_interval_sec"].as<int>();
  }

  if (!ch["sma_channels"]) {
    SUPLA_LOG_ERROR("Channel[%d] config: SmaInverter missing sma_channels",
                    context.channelNumber);
    return false;
  }

  std::vector<Supla::PV::SmaMappedChannel> mappedChannels;
  for (const auto& entry : ch["sma_channels"]) {
    const std::string key = entry.first.as<std::string>();
    const YAML::Node& node = entry.second;
    Supla::PV::SmaMappedChannel mapped;
    mapped.key = key;

    if (!node["ctype"] || !node["cindex"]) {
      SUPLA_LOG_ERROR(
          "Channel[%d] config: sma_channels.%s requires ctype and cindex",
          context.channelNumber,
          key.c_str());
      return false;
    }

    config.markChannelParameterUsed();
    mapped.descriptor.ctype = static_cast<uint16_t>(node["ctype"].as<int>());
    mapped.descriptor.cindex = static_cast<uint8_t>(node["cindex"].as<int>());

    if (node["ntype"]) {
      config.markChannelParameterUsed();
      mapped.descriptor.ntype = static_cast<uint16_t>(node["ntype"].as<int>());
    }
    if (node["gain"]) {
      config.markChannelParameterUsed();
      mapped.descriptor.gain = node["gain"].as<float>();
    }
    if (node["offset"]) {
      config.markChannelParameterUsed();
      mapped.descriptor.offset = node["offset"].as<float>();
    }

    std::string suplaMapping;
    if (node["supla"]) {
      config.markChannelParameterUsed();
      suplaMapping = node["supla"].as<std::string>();
    } else {
      suplaMapping = key;
    }

    mapped.descriptor.suplaMapping = parseSuplaMapping(suplaMapping);
    if (mapped.descriptor.suplaMapping == nullptr) {
      SUPLA_LOG_ERROR(
          "Channel[%d] config: unknown supla mapping \"%s\" for sma_channels.%s",
          context.channelNumber,
          suplaMapping.c_str(),
          key.c_str());
      return false;
    }

    mappedChannels.push_back(mapped);
  }

  if (mappedChannels.empty()) {
    SUPLA_LOG_ERROR("Channel[%d] config: SmaInverter has no sma_channels",
                    context.channelNumber);
    return false;
  }

  SUPLA_LOG_INFO(
      "Channel[%d] config: adding SmaInverter on %s, net_address=0x%04x",
      context.channelNumber,
      devicePath.c_str(),
      netAddress);

  auto inverter = new Supla::PV::SmaInverter(devicePath,
                                             baud,
                                             parseMedia(mediaStr),
                                             netAddress,
                                             pollIntervalSec,
                                             std::move(mappedChannels));
  return config.addCommonChannelParameters(ch, inverter);
}

}  // namespace

namespace Supla {
namespace Linux {

void initSmaExtension() {
  ChannelFactoryRegistry::instance().registerFactory("sma",
                                                     "SmaInverter",
                                                     AddSmaInverter);
}

}  // namespace Linux
}  // namespace Supla

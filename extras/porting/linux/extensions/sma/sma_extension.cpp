/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include <supla/log_wrapper.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "linux_channel_factory.h"
#include "linux_yaml_config.h"
#include "sma_bus.h"
#include "sma_dc_meter.h"
#include "sma_inverter.h"
#include "sma_measurement.h"
#include "sma_thermometer.h"
#include "sma_types.h"

namespace {

enum class SmaMappingKind { Meter, Thermometer, Measurement };

Supla::Linux::Sma::SerialMedia parseMedia(const std::string& media) {
  if (media == "RS485" || media == "rs485") {
    return Supla::Linux::Sma::SerialMedia::RS485;
  }
  return Supla::Linux::Sma::SerialMedia::RS232;
}

const char* parseSuplaMapping(const std::string& mapping, SmaMappingKind kind) {
  switch (kind) {
    case SmaMappingKind::Meter:
      if (mapping == "power_active" || mapping == "pac") {
        return "power_active";
      }
      if (mapping == "rvr_act_energy" || mapping == "totwh") {
        return "rvr_act_energy";
      }
      if (mapping == "fwd_act_energy") {
        return "fwd_act_energy";
      }
      if (mapping == "voltage" || mapping == "uac" || mapping == "upv") {
        return "voltage";
      }
      if (mapping == "current" || mapping == "iac" || mapping == "ipv") {
        return "current";
      }
      if (mapping == "frequency" || mapping == "fac") {
        return "frequency";
      }
      return nullptr;
    case SmaMappingKind::Thermometer:
      if (mapping == "temperature" || mapping == "tkk") {
        return "temperature";
      }
      return nullptr;
    case SmaMappingKind::Measurement:
      if (mapping == "measurement" || mapping == "value" || mapping == "gpm" ||
          mapping == "impedance" || mapping == "zac" || mapping == "riso") {
        return "measurement";
      }
      return nullptr;
  }
  return nullptr;
}

struct SmaSerialConfig {
  std::string devicePath;
  int baud = 9600;
  std::string mediaStr = "RS485";
  uint16_t netAddress = 1;
  int pollIntervalSec = Supla::Linux::Sma::kDefaultPollIntervalSec;
  std::string deviceProfile;
};

bool parseSmaSerialConfig(const Supla::Linux::ChannelFactoryContext& context,
                          SmaSerialConfig* out) {
  const auto& ch = context.channel;
  auto& config = context.config;

  if (!ch["serial"] || !ch["serial"]["device"]) {
    SUPLA_LOG_ERROR("Channel[%d] config: missing serial.device",
                    context.channelNumber);
    return false;
  }

  config.markChannelParameterUsed();
  out->devicePath = ch["serial"]["device"].as<std::string>();

  if (ch["serial"]["baud"]) {
    config.markChannelParameterUsed();
    out->baud = ch["serial"]["baud"].as<int>();
  }
  if (ch["serial"]["media"]) {
    config.markChannelParameterUsed();
    out->mediaStr = ch["serial"]["media"].as<std::string>();
  }
  if (ch["device"] && ch["device"]["net_address"]) {
    config.markChannelParameterUsed();
    out->netAddress =
        static_cast<uint16_t>(ch["device"]["net_address"].as<int>());
  }
  if (ch["poll_interval_sec"]) {
    config.markChannelParameterUsed();
    out->pollIntervalSec = ch["poll_interval_sec"].as<int>();
  }
  if (ch["device"] && ch["device"]["profile"]) {
    config.markChannelParameterUsed();
    out->deviceProfile = ch["device"]["profile"].as<std::string>();
  }
  return true;
}

bool parseSmaChannels(
    const Supla::Linux::ChannelFactoryContext& context,
    SmaMappingKind mappingKind,
    std::vector<Supla::PV::SmaMappedChannel>* mappedChannels) {
  const auto& ch = context.channel;
  auto& config = context.config;

  if (!ch["sma_channels"]) {
    SUPLA_LOG_ERROR("Channel[%d] config: missing sma_channels",
                    context.channelNumber);
    return false;
  }

  for (const auto& entry : ch["sma_channels"]) {
    const std::string key = entry.first.as<std::string>();
    const YAML::Node& node = entry.second;
    Supla::PV::SmaMappedChannel mapped;
    mapped.key = key;

    std::string suplaMapping;

    if (node.IsScalar()) {
      config.markChannelParameterUsed();
      mapped.resolveByName = true;
      mapped.smaName = key;
      suplaMapping = node.as<std::string>();
    } else if (node.IsMap()) {
      if (!node["ctype"] || !node["cindex"]) {
        SUPLA_LOG_ERROR(
            "Channel[%d] config: sma_channels.%s requires ctype and cindex "
            "or a scalar SUPLA mapping",
            context.channelNumber,
            key.c_str());
        return false;
      }

      config.markChannelParameterUsed();
      mapped.descriptor.ctype = static_cast<uint16_t>(node["ctype"].as<int>());
      mapped.descriptor.cindex = static_cast<uint8_t>(node["cindex"].as<int>());

      if (node["ntype"]) {
        config.markChannelParameterUsed();
        mapped.descriptor.ntype =
            static_cast<uint16_t>(node["ntype"].as<int>());
      }
      if (node["gain"]) {
        config.markChannelParameterUsed();
        mapped.descriptor.gain = node["gain"].as<float>();
      }
      if (node["offset"]) {
        config.markChannelParameterUsed();
        mapped.descriptor.offset = node["offset"].as<float>();
      }

      if (node["supla"]) {
        config.markChannelParameterUsed();
        suplaMapping = node["supla"].as<std::string>();
      } else {
        suplaMapping = key;
      }
    } else {
      SUPLA_LOG_ERROR(
          "Channel[%d] config: sma_channels.%s must be a scalar or map",
          context.channelNumber,
          key.c_str());
      return false;
    }

    mapped.descriptor.suplaMapping =
        parseSuplaMapping(suplaMapping, mappingKind);
    if (mapped.descriptor.suplaMapping == nullptr) {
      SUPLA_LOG_ERROR(
          "Channel[%d] config: unknown supla mapping \"%s\" for "
          "sma_channels.%s",
          context.channelNumber,
          suplaMapping.c_str(),
          key.c_str());
      return false;
    }

    mappedChannels->push_back(mapped);
  }

  if (mappedChannels->empty()) {
    SUPLA_LOG_ERROR("Channel[%d] config: sma_channels is empty",
                    context.channelNumber);
    return false;
  }

  return true;
}

bool parseSingleSmaChannel(
    const Supla::Linux::ChannelFactoryContext& context,
    SmaMappingKind mappingKind,
    std::vector<Supla::PV::SmaMappedChannel>* mappedChannels) {
  if (!parseSmaChannels(context, mappingKind, mappedChannels)) {
    return false;
  }
  if (mappedChannels->size() != 1) {
    SUPLA_LOG_ERROR(
        "Channel[%d] config: expected exactly one sma_channels entry",
        context.channelNumber);
    return false;
  }
  return true;
}

void applyGpmYamlOptions(const Supla::Linux::ChannelFactoryContext& context,
                         Supla::PV::SmaMeasurement* measurement) {
  const auto& ch = context.channel;
  auto& config = context.config;

  if (ch["default_unit_after_value"]) {
    config.markChannelParameterUsed();
    const std::string unit = ch["default_unit_after_value"].as<std::string>();
    measurement->setDefaultUnitAfterValue(unit.c_str());
  }
  if (ch["default_unit_before_value"]) {
    config.markChannelParameterUsed();
    const std::string unit = ch["default_unit_before_value"].as<std::string>();
    measurement->setDefaultUnitBeforeValue(unit.c_str());
  }
  if (ch["default_value_precision"]) {
    config.markChannelParameterUsed();
    measurement->setDefaultValuePrecision(
        ch["default_value_precision"].as<int>());
  }
}

bool AddSmaMeter(const Supla::Linux::ChannelFactoryContext& context,
                 const char* typeName,
                 bool useDcMeter) {
  SmaSerialConfig serialConfig;
  if (!parseSmaSerialConfig(context, &serialConfig)) {
    return false;
  }

  std::vector<Supla::PV::SmaMappedChannel> mappedChannels;
  if (!parseSmaChannels(context, SmaMappingKind::Meter, &mappedChannels)) {
    return false;
  }

  SUPLA_LOG_INFO("Channel[%d] config: adding %s on %s, net_address=0x%04x",
                 context.channelNumber,
                 typeName,
                 serialConfig.devicePath.c_str(),
                 serialConfig.netAddress);

  std::unique_ptr<Supla::PV::SmaInverter> meter;
  if (useDcMeter) {
    meter.reset(new Supla::PV::SmaDcMeter(serialConfig.devicePath,
                                          serialConfig.baud,
                                          parseMedia(serialConfig.mediaStr),
                                          serialConfig.netAddress,
                                          serialConfig.pollIntervalSec,
                                          std::move(mappedChannels),
                                          serialConfig.deviceProfile));
  } else {
    meter.reset(new Supla::PV::SmaInverter(serialConfig.devicePath,
                                           serialConfig.baud,
                                           parseMedia(serialConfig.mediaStr),
                                           serialConfig.netAddress,
                                           serialConfig.pollIntervalSec,
                                           std::move(mappedChannels),
                                           serialConfig.deviceProfile));
  }

  const bool added =
      context.config.addCommonChannelParameters(context.channel, meter.get());
  if (added) {
    meter.release();
  }
  return added;
}

bool AddSmaInverter(const Supla::Linux::ChannelFactoryContext& context) {
  return AddSmaMeter(context, "SmaInverter", false);
}

bool AddSmaDcMeter(const Supla::Linux::ChannelFactoryContext& context) {
  return AddSmaMeter(context, "SmaDcMeter", true);
}

bool AddSmaThermometer(const Supla::Linux::ChannelFactoryContext& context) {
  SmaSerialConfig serialConfig;
  if (!parseSmaSerialConfig(context, &serialConfig)) {
    return false;
  }

  std::vector<Supla::PV::SmaMappedChannel> mappedChannels;
  if (!parseSingleSmaChannel(
          context, SmaMappingKind::Thermometer, &mappedChannels)) {
    return false;
  }

  SUPLA_LOG_INFO(
      "Channel[%d] config: adding SmaThermometer on %s, net_address=0x%04x",
      context.channelNumber,
      serialConfig.devicePath.c_str(),
      serialConfig.netAddress);

  std::unique_ptr<Supla::PV::SmaThermometer> thermometer(
      new Supla::PV::SmaThermometer(serialConfig.devicePath,
                                    serialConfig.baud,
                                    parseMedia(serialConfig.mediaStr),
                                    serialConfig.netAddress,
                                    serialConfig.pollIntervalSec,
                                    std::move(mappedChannels),
                                    serialConfig.deviceProfile));
  const bool added = context.config.addCommonChannelParameters(
      context.channel, thermometer.get());
  if (added) {
    thermometer.release();
  }
  return added;
}

bool AddSmaMeasurement(const Supla::Linux::ChannelFactoryContext& context) {
  SmaSerialConfig serialConfig;
  if (!parseSmaSerialConfig(context, &serialConfig)) {
    return false;
  }

  std::vector<Supla::PV::SmaMappedChannel> mappedChannels;
  if (!parseSingleSmaChannel(
          context, SmaMappingKind::Measurement, &mappedChannels)) {
    return false;
  }

  SUPLA_LOG_INFO(
      "Channel[%d] config: adding SmaMeasurement on %s, net_address=0x%04x",
      context.channelNumber,
      serialConfig.devicePath.c_str(),
      serialConfig.netAddress);

  std::unique_ptr<Supla::PV::SmaMeasurement> measurement(
      new Supla::PV::SmaMeasurement(serialConfig.devicePath,
                                    serialConfig.baud,
                                    parseMedia(serialConfig.mediaStr),
                                    serialConfig.netAddress,
                                    serialConfig.pollIntervalSec,
                                    std::move(mappedChannels),
                                    serialConfig.deviceProfile));
  applyGpmYamlOptions(context, measurement.get());
  const bool added = context.config.addCommonChannelParameters(
      context.channel, measurement.get());
  if (added) {
    measurement.release();
  }
  return added;
}

}  // namespace

namespace Supla {
namespace Linux {

void initSmaExtension() {
  ChannelFactoryRegistry::instance().registerFactory(
      "sma", "SmaInverter", AddSmaInverter);
  ChannelFactoryRegistry::instance().registerFactory(
      "sma", "SmaDcMeter", AddSmaDcMeter);
  ChannelFactoryRegistry::instance().registerFactory(
      "sma", "SmaThermometer", AddSmaThermometer);
  ChannelFactoryRegistry::instance().registerFactory(
      "sma", "SmaMeasurement", AddSmaMeasurement);
}

}  // namespace Linux
}  // namespace Supla

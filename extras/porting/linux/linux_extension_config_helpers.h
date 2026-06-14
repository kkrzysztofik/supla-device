/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#ifndef EXTRAS_PORTING_LINUX_LINUX_EXTENSION_CONFIG_HELPERS_H_
#define EXTRAS_PORTING_LINUX_LINUX_EXTENSION_CONFIG_HELPERS_H_

#include <supla/log_wrapper.h>

#include <memory>
#include <string>

#include "linux_channel_factory.h"
#include "linux_yaml_config.h"

namespace Supla {
namespace Linux {

struct ExtensionSerialConfig {
  std::string devicePath;
  int baud = 9600;
};

inline bool parseExtensionSerialConfig(
    const ChannelFactoryContext& context,
    ExtensionSerialConfig* out) {
  if (out == nullptr) {
    return false;
  }

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

  return true;
}

inline void parseExtensionPollIntervalSec(
    const ChannelFactoryContext& context,
    int* pollIntervalSec) {
  if (pollIntervalSec == nullptr) {
    return;
  }

  const auto& ch = context.channel;
  auto& config = context.config;

  if (ch["poll_interval_sec"]) {
    config.markChannelParameterUsed();
    *pollIntervalSec = ch["poll_interval_sec"].as<int>();
  }
}

template <typename MeasurementT>
void applyGpmYamlOptions(const ChannelFactoryContext& context,
                         MeasurementT* measurement) {
  if (measurement == nullptr) {
    return;
  }

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

template <typename ChannelT>
bool addConfiguredChannel(const ChannelFactoryContext& context,
                          std::unique_ptr<ChannelT> channel) {
  const bool added =
      context.config.addCommonChannelParameters(context.channel, channel.get());
  if (added) {
    channel.release();
  }
  return added;
}

}  // namespace Linux
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_LINUX_EXTENSION_CONFIG_HELPERS_H_

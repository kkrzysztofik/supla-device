/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include <supla/log_wrapper.h>

#include <cstdlib>
#include <memory>
#include <string>
#include <utility>

#include "ingecon_bus_client.h"
#include "ingecon_dc_meter.h"
#include "ingecon_inverter.h"
#include "ingecon_measurement.h"
#include "linux_channel_factory.h"
#include "linux_yaml_config.h"

namespace {

bool ingeconShutdownRegistered = false;

struct IngeconYamlConfig {
  Supla::Linux::Ingecon::BusConfig bus;
  Supla::PV::IngeconEnergyMapping energyMapping =
      Supla::PV::IngeconEnergyMapping::Reverse;
  std::string valueKey;
};

bool parseBusConfig(const Supla::Linux::ChannelFactoryContext& context,
                    IngeconYamlConfig* out) {
  const auto& ch = context.channel;
  auto& config = context.config;

  if (out == nullptr) {
    return false;
  }
  if (!ch["serial"] || !ch["serial"]["device"]) {
    SUPLA_LOG_ERROR("Channel[%d] config: missing serial.device",
                    context.channelNumber);
    return false;
  }

  config.markChannelParameterUsed();
  out->bus.serialDevice = ch["serial"]["device"].as<std::string>();

  if (ch["serial"]["baud"]) {
    config.markChannelParameterUsed();
    out->bus.baud = ch["serial"]["baud"].as<int>();
  }
  if (ch["serial"]["rts_toggle"]) {
    config.markChannelParameterUsed();
    out->bus.rtsToggle = ch["serial"]["rts_toggle"].as<bool>();
  }
  if (ch["device"] && ch["device"]["modbus_address"]) {
    config.markChannelParameterUsed();
    const int address = ch["device"]["modbus_address"].as<int>();
    if (address < 1 || address > 247) {
      SUPLA_LOG_ERROR("Channel[%d] config: modbus_address out of range",
                      context.channelNumber);
      return false;
    }
    out->bus.modbusAddress = static_cast<uint8_t>(address);
  }
  if (ch["device"] && ch["device"]["profile"]) {
    config.markChannelParameterUsed();
    const auto profile = ch["device"]["profile"].as<std::string>();
    if (!Supla::Linux::Ingecon::parseProfileName(profile, &out->bus.profile)) {
      SUPLA_LOG_ERROR("Channel[%d] config: invalid Ingecon profile \"%s\"",
                      context.channelNumber,
                      profile.c_str());
      return false;
    }
  }
  if (ch["poll_interval_sec"]) {
    config.markChannelParameterUsed();
    out->bus.pollIntervalSec = ch["poll_interval_sec"].as<int>();
  }
  if (ch["timeout_ms"]) {
    config.markChannelParameterUsed();
    out->bus.timeoutMs = ch["timeout_ms"].as<int>();
  }
  if (ch["retries"]) {
    config.markChannelParameterUsed();
    out->bus.retries = ch["retries"].as<int>();
  }
  return true;
}

bool AddIngeconInverter(const Supla::Linux::ChannelFactoryContext& context) {
  IngeconYamlConfig parsed;
  if (!parseBusConfig(context, &parsed)) {
    return false;
  }

  const auto& ch = context.channel;
  auto& config = context.config;
  if (ch["energy_mapping"]) {
    config.markChannelParameterUsed();
    const auto mapping = ch["energy_mapping"].as<std::string>();
    if (mapping == "forward") {
      parsed.energyMapping = Supla::PV::IngeconEnergyMapping::Forward;
    } else if (mapping == "reverse") {
      parsed.energyMapping = Supla::PV::IngeconEnergyMapping::Reverse;
    } else {
      SUPLA_LOG_ERROR("Channel[%d] config: invalid energy_mapping \"%s\"",
                      context.channelNumber,
                      mapping.c_str());
      return false;
    }
  }

  SUPLA_LOG_INFO(
      "Channel[%d] config: adding IngeconInverter on %s, address=%u, "
      "profile=%s",
                 context.channelNumber,
                 parsed.bus.serialDevice.c_str(),
                 parsed.bus.modbusAddress,
                 Supla::Linux::Ingecon::profileToString(parsed.bus.profile));

  std::unique_ptr<Supla::PV::IngeconInverter> inverter(
      new Supla::PV::IngeconInverter(parsed.bus, parsed.energyMapping));
  const bool added =
      context.config.addCommonChannelParameters(context.channel, inverter.get());
  if (added) {
    inverter.release();
  }
  return added;
}

bool AddIngeconDcMeter(const Supla::Linux::ChannelFactoryContext& context) {
  IngeconYamlConfig parsed;
  if (!parseBusConfig(context, &parsed)) {
    return false;
  }

  SUPLA_LOG_INFO(
      "Channel[%d] config: adding IngeconDcMeter on %s, address=%u, "
      "profile=%s",
      context.channelNumber,
      parsed.bus.serialDevice.c_str(),
      parsed.bus.modbusAddress,
      Supla::Linux::Ingecon::profileToString(parsed.bus.profile));

  std::unique_ptr<Supla::PV::IngeconDcMeter> meter(
      new Supla::PV::IngeconDcMeter(parsed.bus));
  const bool added =
      context.config.addCommonChannelParameters(context.channel, meter.get());
  if (added) {
    meter.release();
  }
  return added;
}

void applyMeasurementYamlOptions(
    const Supla::Linux::ChannelFactoryContext& context,
    Supla::PV::IngeconMeasurement* measurement) {
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

bool AddIngeconMeasurement(const Supla::Linux::ChannelFactoryContext& context) {
  IngeconYamlConfig parsed;
  if (!parseBusConfig(context, &parsed)) {
    return false;
  }

  const auto& ch = context.channel;
  auto& config = context.config;
  if (!ch["ingecon_value"]) {
    SUPLA_LOG_ERROR("Channel[%d] config: missing ingecon_value",
                    context.channelNumber);
    return false;
  }
  config.markChannelParameterUsed();
  parsed.valueKey = ch["ingecon_value"].as<std::string>();

  SUPLA_LOG_INFO(
      "Channel[%d] config: adding IngeconMeasurement on %s, address=%u, "
      "profile=%s, value=%s",
      context.channelNumber,
      parsed.bus.serialDevice.c_str(),
      parsed.bus.modbusAddress,
      Supla::Linux::Ingecon::profileToString(parsed.bus.profile),
      parsed.valueKey.c_str());

  std::unique_ptr<Supla::PV::IngeconMeasurement> measurement(
      new Supla::PV::IngeconMeasurement(parsed.bus, parsed.valueKey));
  applyMeasurementYamlOptions(context, measurement.get());

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

void initIngeconExtension() {
  if (!ingeconShutdownRegistered) {
    std::atexit(&Supla::Linux::Ingecon::shutdownAllClients);
    ingeconShutdownRegistered = true;
  }

  ChannelFactoryRegistry::instance().registerFactory(
      "ingecon", "IngeconInverter", AddIngeconInverter);
  ChannelFactoryRegistry::instance().registerFactory(
      "ingecon", "IngeconDcMeter", AddIngeconDcMeter);
  ChannelFactoryRegistry::instance().registerFactory(
      "ingecon", "IngeconMeasurement", AddIngeconMeasurement);
}

}  // namespace Linux
}  // namespace Supla

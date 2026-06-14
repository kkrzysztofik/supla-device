/*
 Copyright (C) Krzysztof Krzysztofik
*/

#include <supla/log_wrapper.h>

#include <cstdlib>
#include <memory>
#include <string>

#include "byd_binary.h"
#include "byd_config.h"
#include "byd_measurement.h"
#include "byd_meter.h"
#include "byd_thermometer.h"
#include "linux_channel_factory.h"
#include "linux_extension_config_helpers.h"

namespace {

bool bydShutdownRegistered = false;

bool parseBydCommon(const Supla::Linux::ChannelFactoryContext& context,
                    Supla::Linux::Byd::BydAccountConfig* account,
                    Supla::Linux::Byd::BydVehicleConfig* vehicle,
                    std::string* field) {
  if (!Supla::Linux::Byd::parseAccountConfig(context.config, context.channel,
                                             account)) {
    SUPLA_LOG_ERROR("Channel[%d] config: missing BYD account credentials",
                    context.channelNumber);
    return false;
  }
  if (!Supla::Linux::Byd::parseVehicleConfig(context, vehicle)) {
    return false;
  }
  return Supla::Linux::Byd::parseBydField(context, field);
}

bool AddBydMeasurement(const Supla::Linux::ChannelFactoryContext& context) {
  Supla::Linux::Byd::BydAccountConfig account;
  Supla::Linux::Byd::BydVehicleConfig vehicle;
  std::string field;
  if (!parseBydCommon(context, &account, &vehicle, &field)) {
    return false;
  }

  SUPLA_LOG_INFO("Channel[%d] config: adding BydMeasurement vin=%s field=%s",
                 context.channelNumber,
                 vehicle.vin.c_str(),
                 field.c_str());

  std::unique_ptr<Supla::PV::BydMeasurement> channel(
      new Supla::PV::BydMeasurement(account, vehicle, field));
  Supla::Linux::applyGpmYamlOptions(context, channel.get());
  return Supla::Linux::addConfiguredChannel(context, std::move(channel));
}

bool AddBydMeter(const Supla::Linux::ChannelFactoryContext& context) {
  Supla::Linux::Byd::BydAccountConfig account;
  Supla::Linux::Byd::BydVehicleConfig vehicle;
  std::string field;
  if (!parseBydCommon(context, &account, &vehicle, &field)) {
    return false;
  }

  SUPLA_LOG_INFO("Channel[%d] config: adding BydMeter vin=%s field=%s",
                 context.channelNumber,
                 vehicle.vin.c_str(),
                 field.c_str());

  std::unique_ptr<Supla::PV::BydMeter> channel(
      new Supla::PV::BydMeter(account, vehicle, field));
  return Supla::Linux::addConfiguredChannel(context, std::move(channel));
}

bool AddBydThermometer(const Supla::Linux::ChannelFactoryContext& context) {
  Supla::Linux::Byd::BydAccountConfig account;
  Supla::Linux::Byd::BydVehicleConfig vehicle;
  std::string field;
  if (!parseBydCommon(context, &account, &vehicle, &field)) {
    return false;
  }

  SUPLA_LOG_INFO("Channel[%d] config: adding BydThermometer vin=%s field=%s",
                 context.channelNumber,
                 vehicle.vin.c_str(),
                 field.c_str());

  std::unique_ptr<Supla::PV::BydThermometer> channel(
      new Supla::PV::BydThermometer(account, vehicle, field));
  return Supla::Linux::addConfiguredChannel(context, std::move(channel));
}

bool AddBydBinary(const Supla::Linux::ChannelFactoryContext& context) {
  Supla::Linux::Byd::BydAccountConfig account;
  Supla::Linux::Byd::BydVehicleConfig vehicle;
  std::string field;
  if (!parseBydCommon(context, &account, &vehicle, &field)) {
    return false;
  }

  SUPLA_LOG_INFO("Channel[%d] config: adding BydBinary vin=%s field=%s",
                 context.channelNumber,
                 vehicle.vin.c_str(),
                 field.c_str());

  std::unique_ptr<Supla::PV::BydBinary> channel(
      new Supla::PV::BydBinary(account, vehicle, field));
  return Supla::Linux::addConfiguredChannel(context, std::move(channel));
}

}  // namespace

namespace Supla {
namespace Linux {

void initBydExtension() {
  if (!bydShutdownRegistered) {
    std::atexit(&Supla::Linux::Byd::shutdownAllBydPollers);
    bydShutdownRegistered = true;
  }

  ChannelFactoryRegistry::instance().registerFactory(
      "byd", "BydMeasurement", AddBydMeasurement);
  ChannelFactoryRegistry::instance().registerFactory("byd", "BydMeter",
                                                     AddBydMeter);
  ChannelFactoryRegistry::instance().registerFactory(
      "byd", "BydThermometer", AddBydThermometer);
  ChannelFactoryRegistry::instance().registerFactory("byd", "BydBinary",
                                                     AddBydBinary);
}

}  // namespace Linux
}  // namespace Supla

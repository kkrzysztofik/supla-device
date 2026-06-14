/*
 Copyright (C) Krzysztof Krzysztofik
*/

#ifndef EXTRAS_PORTING_LINUX_BYD_BYD_CONFIG_H_
#define EXTRAS_PORTING_LINUX_BYD_BYD_CONFIG_H_

#include <yaml-cpp/yaml.h>

#include <cstdlib>
#include <string>

#include "byd_types.h"
#include "byd_readings.h"
#include "linux_channel_factory.h"
#include "linux_yaml_config.h"

namespace Supla {
namespace Linux {
namespace Byd {

inline std::string envOrEmpty(const char* name) {
  const char* value = std::getenv(name);
  return value != nullptr ? std::string(value) : std::string();
}

inline bool parseAccountConfig(const LinuxYamlConfig& yamlConfig,
                               const YAML::Node& channel,
                               BydAccountConfig* out) {
  if (out == nullptr) {
    return false;
  }

  const YAML::Node rootByd = yamlConfig.yamlRoot()["byd"];
  const YAML::Node source = channel["byd"] ? channel["byd"] : rootByd;

  out->username = source["email"] ? source["email"].as<std::string>()
                                  : envOrEmpty("BYD_USERNAME");
  if (out->username.empty() && source["username"]) {
    out->username = source["username"].as<std::string>();
  }
  out->password = source["password"] ? source["password"].as<std::string>()
                                     : envOrEmpty("BYD_PASSWORD");

  if (source["base_url"]) {
    out->baseUrl = source["base_url"].as<std::string>();
  } else if (source["region"]) {
    out->baseUrl = regionToBaseUrl(source["region"].as<std::string>());
  } else if (!envOrEmpty("BYD_BASE_URL").empty()) {
    out->baseUrl = envOrEmpty("BYD_BASE_URL");
  }

  if (source["country_code"]) {
    out->countryCode = source["country_code"].as<std::string>();
  } else if (!envOrEmpty("BYD_COUNTRY_CODE").empty()) {
    out->countryCode = envOrEmpty("BYD_COUNTRY_CODE");
  }

  if (source["language"]) {
    out->language = source["language"].as<std::string>();
  }
  if (source["time_zone"]) {
    out->timeZone = source["time_zone"].as<std::string>();
  }
  if (source["poll_interval_sec"]) {
    out->pollIntervalSec = source["poll_interval_sec"].as<int>();
  }
  if (source["bangcle_tables_path"]) {
    out->bangcleTablesPath = source["bangcle_tables_path"].as<std::string>();
  }

  return !out->username.empty() && !out->password.empty();
}

inline bool parseVehicleConfig(const ChannelFactoryContext& context,
                               BydVehicleConfig* out) {
  if (out == nullptr) {
    return false;
  }
  const auto& ch = context.channel;
  if (!ch["vin"]) {
    SUPLA_LOG_ERROR("Channel[%d] config: missing vin", context.channelNumber);
    return false;
  }
  context.config.markChannelParameterUsed();
  out->vin = ch["vin"].as<std::string>();
  if (ch["poll_interval_sec"]) {
    context.config.markChannelParameterUsed();
    out->pollIntervalSec = ch["poll_interval_sec"].as<int>();
  } else if (context.config.yamlRoot()["byd"] &&
             context.config.yamlRoot()["byd"]["poll_interval_sec"]) {
    out->pollIntervalSec =
        context.config.yamlRoot()["byd"]["poll_interval_sec"].as<int>();
  }
  if (ch["energy_type"]) {
    context.config.markChannelParameterUsed();
    out->energyType =
        static_cast<EnergyType>(ch["energy_type"].as<int>());
  }
  if (ch["model_name"]) {
    context.config.markChannelParameterUsed();
    out->modelName = ch["model_name"].as<std::string>();
  }
  return !out->vin.empty();
}

inline bool parseBydField(const ChannelFactoryContext& context,
                          std::string* field) {
  if (field == nullptr) {
    return false;
  }
  const auto& ch = context.channel;
  if (!ch["byd_field"]) {
    SUPLA_LOG_ERROR("Channel[%d] config: missing byd_field",
                    context.channelNumber);
    return false;
  }
  context.config.markChannelParameterUsed();
  *field = ch["byd_field"].as<std::string>();
  std::string canonical;
  if (!BydReadingsBuilder::parseFieldAlias(*field, &canonical)) {
    SUPLA_LOG_ERROR("Channel[%d] config: unknown byd_field \"%s\"",
                    context.channelNumber,
                    field->c_str());
    return false;
  }
  *field = canonical;
  return true;
}

}  // namespace Byd
}  // namespace Linux
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_BYD_BYD_CONFIG_H_

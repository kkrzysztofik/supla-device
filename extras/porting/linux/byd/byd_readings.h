/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#ifndef EXTRAS_PORTING_LINUX_BYD_BYD_READINGS_H_
#define EXTRAS_PORTING_LINUX_BYD_BYD_READINGS_H_

#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace Supla {
namespace Linux {
namespace Byd {

struct BydReadings {
  std::map<std::string, double> values;
  std::map<std::string, bool> booleans;
  bool valid = false;
};

class BydReadingsBuilder {
 public:
  static BydReadings fromSources(const nlohmann::json& realtime,
                                 const nlohmann::json& charging,
                                 const nlohmann::json& hvac,
                                 const nlohmann::json& gps,
                                 const nlohmann::json& energy,
                                 bool hasRealtime,
                                 bool hasCharging,
                                 bool hasHvac,
                                 bool hasGps,
                                 bool hasEnergy);

  static bool parseFieldAlias(const std::string& field, std::string* canonical);

 private:
  static std::optional<double> jsonNumber(const nlohmann::json& value);
  static std::optional<double> getNumber(const nlohmann::json& obj,
                                         const char* key);
  static int getInt(const nlohmann::json& obj, const char* key, int fallback);
  static void putNumber(BydReadings* out,
                        const std::string& key,
                        const nlohmann::json& obj,
                        const char* jsonKey);
  static void putBool(BydReadings* out, const std::string& key, bool value);
  static std::optional<double> parseConsumptionEv(const std::string& text);
};

}  // namespace Byd
}  // namespace Linux
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_BYD_BYD_READINGS_H_

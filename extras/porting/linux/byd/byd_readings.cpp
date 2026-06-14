/*
 Copyright (C) Krzysztof Krzysztofik
*/

#include "byd_readings.h"

#include "byd_types.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace Supla {
namespace Linux {
namespace Byd {

namespace {

const std::map<std::string, std::string> kFieldAliases = {
    {"soc", "soc"},
    {"elec_percent", "soc"},
    {"range_km", "range_km"},
    {"endurance_mileage", "range_km"},
    {"battery_power_w", "battery_power_w"},
    {"gl", "battery_power_w"},
    {"is_charging", "is_charging"},
    {"is_plugged", "is_plugged"},
    {"is_charger_connected", "is_plugged"},
    {"is_online", "is_online"},
    {"is_connected", "is_connected"},
    {"time_to_full_min", "time_to_full_min"},
    {"time_to_full_h", "time_to_full_h"},
    {"charge_rate", "charge_rate"},
    {"rate", "charge_rate"},
    {"odometer_km", "odometer_km"},
    {"total_mileage", "odometer_km"},
    {"cabin_temp_c", "cabin_temp_c"},
    {"temp_in_car", "cabin_temp_c"},
    {"outside_temp_c", "outside_temp_c"},
    {"temp_out_car", "outside_temp_c"},
    {"setpoint_driver_c", "setpoint_driver_c"},
    {"main_setting_temp_new", "setpoint_driver_c"},
    {"setpoint_passenger_c", "setpoint_passenger_c"},
    {"copilot_setting_temp_new", "setpoint_passenger_c"},
    {"consumption_ev_kwh_per_100km", "consumption_ev_kwh_per_100km"},
    {"avg_consumption_ev", "avg_consumption_ev"},
    {"energy_last_50km_kwh", "energy_last_50km_kwh"},
    {"latitude", "latitude"},
    {"longitude", "longitude"},
    {"speed_kmh", "speed_kmh"},
    {"heading_deg", "heading_deg"},
    {"fuel_percent", "fuel_percent"},
    {"fuel_range_km", "fuel_range_km"},
    {"pm25_interior", "pm25_interior"},
    {"pm25_exterior", "pm25_exterior"},
};

ChargingState toChargingState(int value) {
  if (value == 1) {
    return ChargingState::Charging;
  }
  if (value == 15) {
    return ChargingState::Connected;
  }
  if (value == 0) {
    return ChargingState::NotCharging;
  }
  return ChargingState::Unknown;
}

}  // namespace

bool BydReadingsBuilder::parseFieldAlias(const std::string& field,
                                         std::string* canonical) {
  const auto it = kFieldAliases.find(field);
  if (it == kFieldAliases.end()) {
    return false;
  }
  if (canonical != nullptr) {
    *canonical = it->second;
  }
  return true;
}

std::optional<double> BydReadingsBuilder::jsonNumber(
    const nlohmann::json& value) {
  if (value.is_number()) {
    return value.get<double>();
  }
  if (value.is_string()) {
    const std::string text = value.get<std::string>();
    if (text.empty() || text == "--") {
      return std::nullopt;
    }
    char* end = nullptr;
    const double parsed = std::strtod(text.c_str(), &end);
    if (end == text.c_str()) {
      return std::nullopt;
    }
    return parsed;
  }
  return std::nullopt;
}

std::optional<double> BydReadingsBuilder::getNumber(const nlohmann::json& obj,
                                                    const char* key) {
  if (!obj.contains(key)) {
    return std::nullopt;
  }
  return jsonNumber(obj.at(key));
}

void BydReadingsBuilder::putNumber(BydReadings* out,
                                   const std::string& key,
                                   const nlohmann::json& obj,
                                   const char* jsonKey) {
  const auto value = getNumber(obj, jsonKey);
  if (value.has_value() && std::isfinite(*value)) {
    const bool isTemperatureKey =
        key == "cabin_temp_c" || key == "outside_temp_c" ||
        key == "setpoint_driver_c" || key == "setpoint_passenger_c";
    if (isTemperatureKey && (*value == -129.0 || *value <= -120.0)) {
      return;
    }
    if (*value < 0 && (key == "range_km" || key == "soc" || key == "odometer_km")) {
      return;
    }
    if (key == "charge_rate" && *value <= -9.0) {
      return;
    }
    out->values[key] = *value;
  }
}

void BydReadingsBuilder::putBool(BydReadings* out,
                                 const std::string& key,
                                 bool value) {
  out->booleans[key] = value;
}

std::optional<double> BydReadingsBuilder::parseConsumptionEv(
    const std::string& text) {
  if (text.empty() || text == "--") {
    return std::nullopt;
  }
  const auto slash = text.find('/');
  const std::string evPart = slash == std::string::npos ? text : text.substr(0, slash);
  const auto space = evPart.find(' ');
  std::string numberPart = evPart;
  if (space != std::string::npos) {
    numberPart = evPart.substr(0, space);
  }
  char* end = nullptr;
  const double parsed = std::strtod(numberPart.c_str(), &end);
  if (end == numberPart.c_str()) {
    return std::nullopt;
  }
  return parsed;
}

BydReadings BydReadingsBuilder::fromSources(const nlohmann::json& realtime,
                                            const nlohmann::json& charging,
                                            const nlohmann::json& hvac,
                                            const nlohmann::json& gps,
                                            const nlohmann::json& energy,
                                            bool hasRealtime,
                                            bool hasCharging,
                                            bool hasHvac,
                                            bool hasGps,
                                            bool hasEnergy) {
  BydReadings out;
  out.valid = hasRealtime || hasCharging || hasHvac || hasGps || hasEnergy;

  if (hasRealtime && realtime.is_object()) {
    putNumber(&out, "soc", realtime, "elecPercent");
    if (!out.values.count("soc")) {
      putNumber(&out, "soc", realtime, "powerBattery");
    }
    putNumber(&out, "range_km", realtime, "enduranceMileage");
    if (!out.values.count("range_km")) {
      putNumber(&out, "range_km", realtime, "evEndurance");
    }
    putNumber(&out, "battery_power_w", realtime, "gl");
    putNumber(&out, "odometer_km", realtime, "totalMileage");
    putNumber(&out, "speed_kmh", realtime, "speed");
    putNumber(&out, "charge_rate", realtime, "rate");
    putNumber(&out, "cabin_temp_c", realtime, "tempInCar");
    putNumber(&out, "fuel_percent", realtime, "oilPercent");
    putNumber(&out, "fuel_range_km", realtime, "oilEndurance");

    const int chargeState = realtime.value("chargeState", realtime.value("chargingState", -1));
    const ChargingState state = toChargingState(chargeState);
    putBool(&out, "is_charging", state == ChargingState::Charging);
    putBool(&out, "is_plugged",
            state == ChargingState::Charging || state == ChargingState::Connected);
    putBool(&out, "is_online", realtime.value("onlineState", -1) == 1);
    putBool(&out, "is_connected", realtime.value("connectState", -1) == 1);

    const auto fullHour = getNumber(realtime, "fullHour");
    const auto fullMinute = getNumber(realtime, "fullMinute");
    if (fullHour.has_value() && fullMinute.has_value() && *fullHour >= 0 &&
        *fullMinute >= 0) {
      out.values["time_to_full_h"] = *fullHour;
      out.values["time_to_full_min"] = *fullHour * 60 + *fullMinute;
    }

    if (realtime.contains("nearestEnergyConsumption") &&
        realtime["nearestEnergyConsumption"].is_string()) {
      const auto parsed =
          parseConsumptionEv(realtime["nearestEnergyConsumption"].get<std::string>());
      if (parsed.has_value()) {
        out.values["consumption_ev_kwh_per_100km"] = *parsed;
      }
    }
    if (realtime.contains("totalConsumptionEn") &&
        realtime["totalConsumptionEn"].is_string()) {
      const auto parsed =
          parseConsumptionEv(realtime["totalConsumptionEn"].get<std::string>());
      if (parsed.has_value() && !out.values.count("consumption_ev_kwh_per_100km")) {
        out.values["consumption_ev_kwh_per_100km"] = *parsed;
      }
    }
  }

  if (hasCharging && charging.is_object()) {
    putNumber(&out, "soc", charging, "elecPercent");
    if (!out.values.count("soc")) {
      putNumber(&out, "soc", charging, "soc");
    }
    const int chargingState = charging.value("chargingState", -1);
    if (chargingState == 1) {
      putBool(&out, "is_charging", true);
    }
    const auto fullHour = getNumber(charging, "fullHour");
    const auto fullMinute = getNumber(charging, "fullMinute");
    if (fullHour.has_value() && fullMinute.has_value() && *fullHour >= 0 &&
        *fullMinute >= 0) {
      out.values["time_to_full_h"] = *fullHour;
      out.values["time_to_full_min"] = *fullHour * 60 + *fullMinute;
    }
  }

  if (hasHvac && hvac.is_object()) {
    putNumber(&out, "cabin_temp_c", hvac, "tempInCar");
    putNumber(&out, "outside_temp_c", hvac, "tempOutCar");
    putNumber(&out, "setpoint_driver_c", hvac, "mainSettingTempNew");
    putNumber(&out, "setpoint_passenger_c", hvac, "copilotSettingTempNew");
    putNumber(&out, "pm25_interior", hvac, "pm");
    putNumber(&out, "pm25_exterior", hvac, "pm25StateOutCar");
  }

  if (hasGps && gps.is_object()) {
    putNumber(&out, "latitude", gps, "latitude");
    putNumber(&out, "longitude", gps, "longitude");
    putNumber(&out, "speed_kmh", gps, "speed");
    putNumber(&out, "heading_deg", gps, "direction");
  }

  if (hasEnergy && energy.is_object()) {
    if (energy.contains("nearestEnergyConsumption") &&
        energy["nearestEnergyConsumption"].is_object()) {
      const auto& nearest = energy["nearestEnergyConsumption"];
      putNumber(&out, "avg_consumption_ev", nearest, "avgEvConsumption");
      putNumber(&out, "energy_last_50km_kwh", nearest, "evConsumption");
    }
    if (energy.contains("cumulativeEnergyConsumption") &&
        energy["cumulativeEnergyConsumption"].is_object()) {
      const auto& cumulative = energy["cumulativeEnergyConsumption"];
      if (!out.values.count("avg_consumption_ev")) {
        putNumber(&out, "avg_consumption_ev", cumulative, "avgEvConsumption");
      }
    }
  }

  return out;
}

}  // namespace Byd
}  // namespace Linux
}  // namespace Supla

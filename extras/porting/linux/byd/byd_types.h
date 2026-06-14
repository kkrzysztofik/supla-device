/*
 Copyright (C) Krzysztof Krzysztofik

 Wire-format and reading-key types for the native BYD cloud client.
 Crypto/envelope behaviour is derived from the pyBYD reference library (MIT).
*/

#ifndef EXTRAS_PORTING_LINUX_BYD_BYD_TYPES_H_
#define EXTRAS_PORTING_LINUX_BYD_BYD_TYPES_H_

#include <string>

namespace Supla {
namespace Linux {
namespace Byd {

constexpr int kDefaultPollIntervalSec = 60;
constexpr int kEnergyPollEveryCycles = 5;
constexpr int kHvacPollEveryCycles = 2;
constexpr int kGpsPollEveryCycles = 3;

constexpr const char* kUserAgent = "okhttp/4.12.0";
constexpr const char* kAppName = "supla-device-byd+1";

enum class EnergyType { Unknown = -1, Ev = 0, Ice = 1, Hybrid = 2 };

enum class ChargingState { Unknown = -1, NotCharging = 0, Charging = 1, Connected = 15 };

struct BydAccountConfig {
  std::string username;
  std::string password;
  std::string baseUrl = "https://dilinkappoversea-eu.byd.auto";
  std::string countryCode = "NL";
  std::string language = "en";
  std::string timeZone = "Europe/Amsterdam";
  std::string appVersion = "3.2.2";
  std::string appInnerVersion = "322";
  std::string softType = "0";
  std::string tboxVersion = "3";
  std::string isAuto = "1";
  std::string ostype = "and";
  std::string imei = "BANGCLE01234";
  std::string mac = "00:00:00:00:00:00";
  std::string model = "POCO F1";
  std::string sdk = "35";
  std::string mod = "Xiaomi";
  std::string imeiMd5;
  std::string mobileBrand = "XIAOMI";
  std::string mobileModel = "POCO F1";
  std::string deviceType = "0";
  std::string networkType = "wifi";
  std::string osType = "15";
  std::string osVersion = "35";
  int pollIntervalSec = kDefaultPollIntervalSec;
  double sessionTtlSec = 12 * 3600;
  std::string bangcleTablesPath;
};

struct BydVehicleConfig {
  std::string vin;
  EnergyType energyType = EnergyType::Ev;
  std::string modelName;
  int pollIntervalSec = kDefaultPollIntervalSec;
};

std::string regionToBaseUrl(const std::string& region);
std::string accountKey(const BydAccountConfig& account);

}  // namespace Byd
}  // namespace Linux
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_BYD_BYD_TYPES_H_

/*
 Copyright (C) Krzysztof Krzysztofik
*/

#ifndef EXTRAS_PORTING_LINUX_BYD_BYD_CLOUD_CLIENT_H_
#define EXTRAS_PORTING_LINUX_BYD_BYD_CLOUD_CLIENT_H_

#include <nlohmann/json.hpp>

#include <mutex>
#include <string>

#include "byd_bangcle.h"
#include "byd_http_client.h"
#include "byd_session.h"
#include "byd_types.h"

namespace Supla {
namespace Linux {
namespace Byd {

class BydCloudClient {
 public:
  explicit BydCloudClient(BydAccountConfig config);
  ~BydCloudClient() = default;

  bool ensureLoggedIn(std::string* error);
  bool fetchRealtime(const BydVehicleConfig& vehicle,
                     nlohmann::json* out,
                     std::string* error);
  bool fetchChargingHomepage(const std::string& vin,
                             nlohmann::json* out,
                             std::string* error);
  bool fetchHvacStatus(const std::string& vin,
                       nlohmann::json* out,
                       std::string* error);
  bool fetchGps(const std::string& vin,
                nlohmann::json* out,
                std::string* error);
  bool fetchEnergyConsumption(const BydVehicleConfig& vehicle,
                              nlohmann::json* out,
                              std::string* error);

#ifdef SUPLA_TEST
  void setSessionForTest(const BydSession& session) {
    std::lock_guard<std::mutex> lock(mutex_);
    session_ = session;
    hasSession_ = true;
  }
#endif

 private:
  std::string randomHex16() const;
  bool postSecure(const std::string& endpoint,
                  const nlohmann::json& outerPayload,
                  nlohmann::json* outerResponse,
                  std::string* error);
  bool login(std::string* error);
  bool postTokenJson(const std::string& endpoint,
                     nlohmann::json inner,
                     nlohmann::json* decoded,
                     const std::string& vin,
                     std::string* error);
  bool triggerAndPoll(const std::string& triggerEndpoint,
                      const std::string& pollEndpoint,
                      const std::string& vin,
                      EnergyType energyType,
                      bool includeEnergyType,
                      nlohmann::json* out,
                      std::string* error);

  BydAccountConfig config_;
  BangcleCodec codec_;
  BydHttpClient http_;
  std::mutex mutex_;
  BydSession session_;
  bool hasSession_ = false;
};

}  // namespace Byd
}  // namespace Linux
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_BYD_BYD_CLOUD_CLIENT_H_

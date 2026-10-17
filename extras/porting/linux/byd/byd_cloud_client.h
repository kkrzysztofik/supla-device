/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#ifndef EXTRAS_PORTING_LINUX_BYD_BYD_CLOUD_CLIENT_H_
#define EXTRAS_PORTING_LINUX_BYD_BYD_CLOUD_CLIENT_H_

#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
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
  // Injects the HTTP transport instead of the libcurl one.
  BydCloudClient(BydAccountConfig config,
                 std::unique_ptr<BydHttpTransport> transport);
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
  // Callers of postTokenJson()/triggerAndPoll() already hold mutex_, so the
  // session must be validated through the unlocked helper to avoid
  // re-locking the non-recursive mutex on the same thread.
  bool ensureLoggedInLocked(std::string* error);
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
                     std::string* error,
                     bool allowRetry = true);
  bool triggerAndPoll(const std::string& triggerEndpoint,
                      const std::string& pollEndpoint,
                      const std::string& vin,
                      EnergyType energyType,
                      bool includeEnergyType,
                      nlohmann::json* out,
                      std::string* error);

  BydAccountConfig config_;
  BangcleCodec codec_;
  std::unique_ptr<BydHttpTransport> http_;
  std::mutex mutex_;
  BydSession session_;
  bool hasSession_ = false;
};

}  // namespace Byd
}  // namespace Linux
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_BYD_BYD_CLOUD_CLIENT_H_

/*
 Copyright (C) Krzysztof Krzysztofik
*/

#include "byd_cloud_client.h"

#include "byd_aes.h"
#include "byd_hash.h"
#include "byd_sign.h"

#include <supla/log_wrapper.h>

#include <openssl/rand.h>

#include <chrono>
#include <thread>

namespace Supla {
namespace Linux {
namespace Byd {

namespace {

constexpr const char* kSessionExpiredCodes[] = {"1002", "1005", "1010"};

bool isSessionExpiredCode(const std::string& code) {
  for (const char* expired : kSessionExpiredCodes) {
    if (code == expired) {
      return true;
    }
  }
  return false;
}

nlohmann::json buildInnerBase(const BydAccountConfig& config,
                              int64_t nowMs,
                              const std::string& vin,
                              const std::string& requestSerial) {
  nlohmann::json inner = {
      {"deviceType", config.deviceType},
      {"imeiMD5", config.imeiMd5.empty() ? md5Hex(config.username) : config.imeiMd5},
      {"networkType", config.networkType},
      {"random", ""},
      {"timeStamp", std::to_string(nowMs)},
      {"version", config.appInnerVersion},
  };
  if (!vin.empty()) {
    inner["vin"] = vin;
  }
  if (!requestSerial.empty()) {
    inner["requestSerial"] = requestSerial;
  }
  return inner;
}

std::string jsonCompact(const nlohmann::json& value) {
  return value.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
}

bool isRealtimeReady(const nlohmann::json& payload) {
  if (payload.is_null() || !payload.is_object() || payload.empty()) {
    return false;
  }
  if (payload.value("onlineState", -1) == 2) {
    return false;
  }
  static const char* kTireFields[] = {
      "leftFrontTirepressure", "rightFrontTirepressure",
      "leftRearTirepressure",  "rightRearTirepressure",
  };
  for (const char* field : kTireFields) {
    if (payload.value(field, 0.0) > 0.0) {
      return true;
    }
  }
  if (payload.value("time", 0) > 0) {
    return true;
  }
  return payload.value("enduranceMileage", 0.0) > 0.0;
}

bool isGpsReady(const nlohmann::json& payload) {
  if (payload.is_null() || !payload.is_object() || payload.empty()) {
    return false;
  }
  if (payload.size() == 1 && payload.contains("requestSerial")) {
    return false;
  }
  return true;
}

}  // namespace

BydCloudClient::BydCloudClient(BydAccountConfig config)
    : config_(std::move(config)), codec_(config_.bangcleTablesPath) {
  if (config_.imeiMd5.empty()) {
    config_.imeiMd5 = md5Hex(config_.username);
  }
  if (!codec_.loadTables()) {
    SUPLA_LOG_WARNING("BYD: Bangcle tables failed to load");
  }
}

std::string BydCloudClient::randomHex16() const {
  unsigned char bytes[16];
  if (RAND_bytes(bytes, sizeof(bytes)) != 1) {
    return "0123456789ABCDEF0123456789ABCDEF";
  }
  static const char* kHex = "0123456789ABCDEF";
  std::string out;
  out.reserve(32);
  for (unsigned char byte : bytes) {
    out.push_back(kHex[(byte >> 4) & 0xF]);
    out.push_back(kHex[byte & 0xF]);
  }
  return out;
}

bool BydCloudClient::postSecure(const std::string& endpoint,
                                const nlohmann::json& outerPayload,
                                nlohmann::json* outerResponse,
                                std::string* error) {
  if (!codec_.isLoaded()) {
    if (error) {
      *error = "bangcle codec not loaded";
    }
    return false;
  }

  const std::string encoded = codec_.encodeEnvelope(jsonCompact(outerPayload));
  const nlohmann::json body = {{"request", encoded}};
  const std::string url = config_.baseUrl + endpoint;

  int status = 0;
  std::string responseBody;
  std::string httpError;
  if (!http_.postJson(url, jsonCompact(body), kUserAgent, &status, &responseBody,
                      &httpError)) {
    if (error) {
      *error = httpError;
    }
    return false;
  }
  if (status != 200) {
    if (error) {
      *error = "HTTP " + std::to_string(status) + ": " + responseBody.substr(0, 200);
    }
    return false;
  }

  nlohmann::json bodyJson;
  try {
    bodyJson = nlohmann::json::parse(responseBody);
  } catch (const nlohmann::json::exception& ex) {
    if (error) {
      *error = std::string("invalid JSON: ") + ex.what();
    }
    return false;
  }

  if (!bodyJson.contains("response") || !bodyJson["response"].is_string()) {
    if (error) {
      *error = "missing response field";
    }
    return false;
  }

  const auto decodedBytes =
      codec_.decodeEnvelope(bodyJson["response"].get<std::string>());
  if (decodedBytes.empty()) {
    if (error) {
      *error = "bangcle decode failed";
    }
    return false;
  }

  std::string decodedText(decodedBytes.begin(), decodedBytes.end());
  while (!decodedText.empty() &&
         (decodedText.front() == ' ' || decodedText.front() == '\n')) {
    decodedText.erase(decodedText.begin());
  }
  if (decodedText.rfind("F{", 0) == 0 || decodedText.rfind("F[", 0) == 0) {
    decodedText.erase(0, 1);
  }

  try {
    *outerResponse = nlohmann::json::parse(decodedText);
  } catch (const nlohmann::json::exception& ex) {
    if (error) {
      *error = std::string("response not JSON: ") + ex.what();
    }
    return false;
  }
  return true;
}

bool BydCloudClient::login(std::string* error) {
  const int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();
  const std::string randomHex = randomHex16();
  const std::string reqTimestamp = std::to_string(nowMs);
  const std::string serviceTime = reqTimestamp;

  nlohmann::json inner = {
      {"agreeStatus", "0"},
      {"agreementType", "[1,2]"},
      {"appInnerVersion", config_.appInnerVersion},
      {"appVersion", config_.appVersion},
      {"deviceName", config_.mobileBrand + config_.mobileModel},
      {"deviceType", config_.deviceType},
      {"imeiMD5", config_.imeiMd5},
      {"isAuto", config_.isAuto},
      {"mobileBrand", config_.mobileBrand},
      {"mobileModel", config_.mobileModel},
      {"networkType", config_.networkType},
      {"osType", config_.osType},
      {"osVersion", config_.osVersion},
      {"random", randomHex},
      {"softType", config_.softType},
      {"timeStamp", reqTimestamp},
      {"timeZone", config_.timeZone},
  };

  const std::string encryData =
      aesEncryptHex(jsonCompact(inner), pwdLoginKey(config_.password));
  if (encryData.empty()) {
    if (error) {
      *error = "login AES encrypt failed";
    }
    return false;
  }

  std::map<std::string, std::string> signFields;
  for (auto it = inner.begin(); it != inner.end(); ++it) {
    signFields[it.key()] = it.value().is_string() ? it.value().get<std::string>()
                                                  : jsonCompact(it.value());
  }
  signFields["appName"] = kAppName;
  signFields["countryCode"] = config_.countryCode;
  signFields["functionType"] = "pwdLogin";
  signFields["identifier"] = config_.username;
  signFields["identifierType"] = "0";
  signFields["language"] = config_.language;
  signFields["reqTimestamp"] = reqTimestamp;

  nlohmann::json outer = {
      {"appName", kAppName},
      {"countryCode", config_.countryCode},
      {"encryData", encryData},
      {"functionType", "pwdLogin"},
      {"identifier", config_.username},
      {"identifierType", "0"},
      {"imeiMD5", config_.imeiMd5},
      {"isAuto", config_.isAuto},
      {"language", config_.language},
      {"reqTimestamp", reqTimestamp},
      {"sign", sha1Mixed(buildSignString(signFields, md5Hex(config_.password)))},
      {"signKey", config_.password},
      {"ostype", config_.ostype},
      {"imei", config_.imei},
      {"mac", config_.mac},
      {"model", config_.model},
      {"sdk", config_.sdk},
      {"mod", config_.mod},
      {"serviceTime", serviceTime},
  };

  std::map<std::string, std::string> checkFields;
  for (auto it = outer.begin(); it != outer.end(); ++it) {
    checkFields[it.key()] = it.value().is_string() ? it.value().get<std::string>()
                                                     : jsonCompact(it.value());
  }
  outer["checkcode"] = computeCheckcodeJson(jsonCompact(outer));

  nlohmann::json response;
  if (!postSecure("/app/account/login", outer, &response, error)) {
    return false;
  }

  if (response.value("code", "") != "0") {
    if (error) {
      *error = "login failed code=" + response.value("code", "") + " msg=" +
               response.value("message", "");
    }
    return false;
  }

  if (!response.contains("respondData") ||
      !response["respondData"].is_string()) {
    if (error) {
      *error = "login missing respondData";
    }
    return false;
  }

  const std::string plaintext =
      aesDecryptUtf8(response["respondData"].get<std::string>(),
                     pwdLoginKey(config_.password));
  nlohmann::json tokenJson;
  try {
    tokenJson = nlohmann::json::parse(plaintext);
  } catch (const nlohmann::json::exception& ex) {
    if (error) {
      *error = std::string("login token JSON invalid: ") + ex.what();
    }
    return false;
  }

  const auto& token = tokenJson["token"];
  if (!token.contains("userId") || !token.contains("signToken") ||
      !token.contains("encryToken")) {
    if (error) {
      *error = "login missing token fields";
    }
    return false;
  }

  session_ = BydSession(token["userId"].get<std::string>(),
                        token["signToken"].get<std::string>(),
                        token["encryToken"].get<std::string>(),
                        config_.sessionTtlSec);
  hasSession_ = true;
  return true;
}

bool BydCloudClient::ensureLoggedIn(std::string* error) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (hasSession_ && !session_.isExpired()) {
    return true;
  }
  return login(error);
}

bool BydCloudClient::postTokenJson(const std::string& endpoint,
                                   nlohmann::json inner,
                                   nlohmann::json* decoded,
                                   const std::string& vin,
                                   std::string* error) {
  if (!ensureLoggedIn(error)) {
    return false;
  }

  const int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();
  const std::string reqTimestamp = std::to_string(nowMs);
  inner["random"] = randomHex16();

  const std::string contentKey = session_.contentKey();
  const std::string signKey = session_.signKey();
  const std::string encryData = aesEncryptHex(jsonCompact(inner), contentKey);
  if (encryData.empty()) {
    if (error) {
      *error = "AES encrypt failed";
    }
    return false;
  }

  std::map<std::string, std::string> signFields;
  for (auto it = inner.begin(); it != inner.end(); ++it) {
    if (it.value().is_string()) {
      signFields[it.key()] = it.value().get<std::string>();
    } else if (it.value().is_number_integer()) {
      signFields[it.key()] = std::to_string(it.value().get<int64_t>());
    } else {
      signFields[it.key()] = jsonCompact(it.value());
    }
  }
  signFields["countryCode"] = config_.countryCode;
  signFields["identifier"] = session_.userId();
  signFields["imeiMD5"] = config_.imeiMd5;
  signFields["language"] = config_.language;
  signFields["reqTimestamp"] = reqTimestamp;

  nlohmann::json outer = {
      {"countryCode", config_.countryCode},
      {"encryData", encryData},
      {"identifier", session_.userId()},
      {"imeiMD5", config_.imeiMd5},
      {"language", config_.language},
      {"reqTimestamp", reqTimestamp},
      {"sign", sha1Mixed(buildSignString(signFields, signKey))},
      {"ostype", config_.ostype},
      {"imei", config_.imei},
      {"mac", config_.mac},
      {"model", config_.model},
      {"sdk", config_.sdk},
      {"mod", config_.mod},
      {"serviceTime", reqTimestamp},
  };
  outer["checkcode"] = computeCheckcodeJson(jsonCompact(outer));

  nlohmann::json response;
  if (!postSecure(endpoint, outer, &response, error)) {
    return false;
  }

  const std::string code = response.value("code", "");
  if (code != "0") {
    if (isSessionExpiredCode(code)) {
      hasSession_ = false;
      if (ensureLoggedIn(error)) {
        return postTokenJson(endpoint, std::move(inner), decoded, vin, error);
      }
    }
    if (error) {
      *error = endpoint + " failed code=" + code + " msg=" +
               response.value("message", "");
    }
    return false;
  }

  if (!response.contains("respondData") ||
      !response["respondData"].is_string()) {
    *decoded = nlohmann::json::object();
    return true;
  }

  const std::string plaintext =
      aesDecryptUtf8(response["respondData"].get<std::string>(), contentKey);
  if (plaintext.empty()) {
    *decoded = nlohmann::json::object();
    return true;
  }
  try {
    *decoded = nlohmann::json::parse(plaintext);
  } catch (const nlohmann::json::exception& ex) {
    if (error) {
      *error = std::string("respondData not JSON: ") + ex.what();
    }
    return false;
  }
  return true;
}

bool BydCloudClient::triggerAndPoll(const std::string& triggerEndpoint,
                                    const std::string& pollEndpoint,
                                    const std::string& vin,
                                    EnergyType energyType,
                                    bool includeEnergyType,
                                    nlohmann::json* out,
                                    std::string* error) {
  const int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();
  nlohmann::json inner =
      buildInnerBase(config_, nowMs, vin, std::string());
  if (includeEnergyType) {
    inner["energyType"] = std::to_string(static_cast<int>(energyType));
    inner["tboxVersion"] = config_.tboxVersion;
  }

  nlohmann::json triggerPayload;
  std::string localError;
  if (!postTokenJson(triggerEndpoint, inner, &triggerPayload, vin, &localError)) {
    if (error) {
      *error = localError;
    }
    return false;
  }

  std::string requestSerial;
  if (triggerPayload.contains("requestSerial") &&
      triggerPayload["requestSerial"].is_string()) {
    requestSerial = triggerPayload["requestSerial"].get<std::string>();
  }

  auto readyFn = [&](const nlohmann::json& payload) {
    if (pollEndpoint.find("vehicleRealTime") != std::string::npos) {
      return isRealtimeReady(payload);
    }
    return isGpsReady(payload);
  };

  for (int attempt = 0; attempt < 10; ++attempt) {
    if (attempt > 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    }
    nlohmann::json pollInner =
        buildInnerBase(config_, nowMs, vin, requestSerial);
    if (includeEnergyType) {
      pollInner["energyType"] = std::to_string(static_cast<int>(energyType));
      pollInner["tboxVersion"] = config_.tboxVersion;
    }
    nlohmann::json pollPayload;
    if (!postTokenJson(pollEndpoint, pollInner, &pollPayload, vin, &localError)) {
      if (error) {
        *error = localError;
      }
      return false;
    }
    if (pollPayload.contains("requestSerial") &&
        pollPayload["requestSerial"].is_string()) {
      requestSerial = pollPayload["requestSerial"].get<std::string>();
    }
    if (readyFn(pollPayload)) {
      *out = pollPayload;
      return true;
    }
  }

  if (error) {
    *error = pollEndpoint + " poll timeout";
  }
  return false;
}

bool BydCloudClient::fetchRealtime(const BydVehicleConfig& vehicle,
                                   nlohmann::json* out,
                                   std::string* error) {
  std::lock_guard<std::mutex> lock(mutex_);
  return triggerAndPoll("/vehicleInfo/vehicle/vehicleRealTimeRequest",
                        "/vehicleInfo/vehicle/vehicleRealTimeResult",
                        vehicle.vin,
                        vehicle.energyType,
                        true,
                        out,
                        error);
}

bool BydCloudClient::fetchChargingHomepage(const std::string& vin,
                                           nlohmann::json* out,
                                           std::string* error) {
  std::lock_guard<std::mutex> lock(mutex_);
  nlohmann::json inner = buildInnerBase(
      config_,
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count(),
      vin,
      std::string());
  return postTokenJson("/control/smartCharge/homePage", inner, out, vin, error);
}

bool BydCloudClient::fetchHvacStatus(const std::string& vin,
                                     nlohmann::json* out,
                                     std::string* error) {
  std::lock_guard<std::mutex> lock(mutex_);
  nlohmann::json inner = buildInnerBase(
      config_,
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count(),
      vin,
      std::string());
  nlohmann::json decoded;
  if (!postTokenJson("/control/getStatusNow", inner, &decoded, vin, error)) {
    return false;
  }
  if (decoded.contains("statusNow")) {
    *out = decoded["statusNow"];
  } else {
    *out = decoded;
  }
  return true;
}

bool BydCloudClient::fetchGps(const std::string& vin,
                              nlohmann::json* out,
                              std::string* error) {
  std::lock_guard<std::mutex> lock(mutex_);
  return triggerAndPoll("/control/getGpsInfo",
                        "/control/getGpsInfoResult",
                        vin,
                        EnergyType::Ev,
                        false,
                        out,
                        error);
}

bool BydCloudClient::fetchEnergyConsumption(const BydVehicleConfig& vehicle,
                                            nlohmann::json* out,
                                            std::string* error) {
  std::lock_guard<std::mutex> lock(mutex_);
  nlohmann::json inner = buildInnerBase(
      config_,
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count(),
      vehicle.vin,
      std::string());
  inner["powerType"] = std::to_string(static_cast<int>(vehicle.energyType));
  inner["requestType"] = 0;
  if (!vehicle.modelName.empty()) {
    inner["autoModelNameOut"] = vehicle.modelName;
  }
  return postTokenJson("/vehicleInfo/vehicle/getEnergyConsumption", inner, out,
                       vehicle.vin, error);
}

}  // namespace Byd
}  // namespace Linux
}  // namespace Supla

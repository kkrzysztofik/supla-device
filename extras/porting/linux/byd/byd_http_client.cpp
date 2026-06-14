/*
 Copyright (C) Krzysztof Krzysztofik
*/

#include "byd_http_client.h"

#include <curl/curl.h>

namespace Supla {
namespace Linux {
namespace Byd {

namespace {

size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
  auto* out = static_cast<std::string*>(userdata);
  out->append(ptr, size * nmemb);
  return size * nmemb;
}

}  // namespace

BydHttpClient::BydHttpClient() {
  curl_global_init(CURL_GLOBAL_DEFAULT);
  curl_ = curl_easy_init();
}

BydHttpClient::~BydHttpClient() {
  if (curl_ != nullptr) {
    curl_easy_cleanup(static_cast<CURL*>(curl_));
    curl_ = nullptr;
  }
  curl_global_cleanup();
}

bool BydHttpClient::postJson(const std::string& url,
                             const std::string& body,
                             const std::string& userAgent,
                             int* statusCode,
                             std::string* responseBody,
                             std::string* error) {
  if (curl_ == nullptr || responseBody == nullptr) {
    if (error) {
      *error = "curl not initialized";
    }
    return false;
  }

  responseBody->clear();
  auto* handle = static_cast<CURL*>(curl_);
  curl_easy_reset(handle);
  curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
  curl_easy_setopt(handle, CURLOPT_POST, 1L);
  curl_easy_setopt(handle, CURLOPT_POSTFIELDS, body.c_str());
  curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
  curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, writeCallback);
  curl_easy_setopt(handle, CURLOPT_WRITEDATA, responseBody);
  curl_easy_setopt(handle, CURLOPT_TIMEOUT, 30L);
  curl_easy_setopt(handle, CURLOPT_ACCEPT_ENCODING, "identity");

  struct curl_slist* headers = nullptr;
  headers = curl_slist_append(headers, "content-type: application/json; charset=UTF-8");
  const std::string uaHeader = "user-agent: " + userAgent;
  headers = curl_slist_append(headers, uaHeader.c_str());
  curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);

  const CURLcode rc = curl_easy_perform(handle);
  curl_slist_free_all(headers);

  if (rc != CURLE_OK) {
    if (error) {
      *error = curl_easy_strerror(rc);
    }
    return false;
  }

  if (statusCode != nullptr) {
    long code = 0;
    curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &code);
    *statusCode = static_cast<int>(code);
  }
  return true;
}

}  // namespace Byd
}  // namespace Linux
}  // namespace Supla

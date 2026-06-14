/*
 Copyright (C) Krzysztof Krzysztofik
*/

#include "byd_hash.h"

#include <openssl/evp.h>

#include <iomanip>
#include <sstream>

namespace Supla {
namespace Linux {
namespace Byd {

namespace {

std::string digestHex(const EVP_MD* md, const std::string& value) {
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int digestLen = 0;
  EVP_MD_CTX* ctx = EVP_MD_CTX_new();
  if (ctx == nullptr) {
    return {};
  }
  if (EVP_DigestInit_ex(ctx, md, nullptr) != 1 ||
      EVP_DigestUpdate(ctx, value.data(), value.size()) != 1 ||
      EVP_DigestFinal_ex(ctx, digest, &digestLen) != 1) {
    EVP_MD_CTX_free(ctx);
    return {};
  }
  EVP_MD_CTX_free(ctx);

  std::ostringstream oss;
  for (unsigned int i = 0; i < digestLen; ++i) {
    oss << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
        << static_cast<int>(digest[i]);
  }
  return oss.str();
}

}  // namespace

std::string md5Hex(const std::string& value) {
  return digestHex(EVP_md5(), value);
}

std::string pwdLoginKey(const std::string& password) {
  return md5Hex(md5Hex(password));
}

std::string sha1Mixed(const std::string& value) {
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int digestLen = 0;
  EVP_MD_CTX* ctx = EVP_MD_CTX_new();
  if (ctx == nullptr) {
    return {};
  }
  if (EVP_DigestInit_ex(ctx, EVP_sha1(), nullptr) != 1 ||
      EVP_DigestUpdate(ctx, value.data(), value.size()) != 1 ||
      EVP_DigestFinal_ex(ctx, digest, &digestLen) != 1) {
    EVP_MD_CTX_free(ctx);
    return {};
  }
  EVP_MD_CTX_free(ctx);

  std::string mixed;
  mixed.reserve(digestLen * 2);
  for (unsigned int i = 0; i < digestLen; ++i) {
    std::ostringstream oss;
    if (i % 2 == 0) {
      oss << std::uppercase;
    }
    oss << std::hex << std::setw(2) << std::setfill('0')
        << static_cast<int>(digest[i]);
    mixed += oss.str();
  }

  std::string filtered;
  filtered.reserve(mixed.size());
  for (size_t j = 0; j < mixed.size(); ++j) {
    if (mixed[j] == '0' && (j % 2) == 0) {
      continue;
    }
    filtered.push_back(mixed[j]);
  }
  return filtered;
}

std::string computeCheckcode(const std::map<std::string, std::string>& payload) {
  std::ostringstream json;
  json << '{';
  bool first = true;
  for (const auto& entry : payload) {
    if (!first) {
      json << ',';
    }
    first = false;
    json << '"' << entry.first << "\":\"" << entry.second << '"';
  }
  json << '}';
  const std::string md5 = md5Hex(json.str());
  if (md5.size() < 32) {
    return md5;
  }
  return md5.substr(24, 8) + md5.substr(8, 8) + md5.substr(16, 8) +
         md5.substr(0, 8);
}

}  // namespace Byd
}  // namespace Linux
}  // namespace Supla

/*
 Copyright (C) Krzysztof Krzysztofik
*/

#include "byd_aes.h"

#include <openssl/evp.h>

#include <cctype>
#include <sstream>
#include <vector>

namespace Supla {
namespace Linux {
namespace Byd {

namespace {

std::vector<unsigned char> parseHexKey(const std::string& keyHex) {
  std::string text = keyHex;
  if (text.rfind("0x", 0) == 0 || text.rfind("0X", 0) == 0) {
    text = text.substr(2);
  }
  if (text.size() % 2 != 0) {
    return {};
  }
  std::vector<unsigned char> out;
  out.reserve(text.size() / 2);
  for (size_t i = 0; i < text.size(); i += 2) {
    const std::string byteStr = text.substr(i, 2);
    out.push_back(static_cast<unsigned char>(std::stoul(byteStr, nullptr, 16)));
  }
  return out;
}

std::string toUpperHex(const unsigned char* data, size_t len) {
  std::ostringstream oss;
  oss << std::uppercase << std::hex;
  for (size_t i = 0; i < len; ++i) {
    oss.width(2);
    oss.fill('0');
    oss << static_cast<int>(data[i]);
  }
  return oss.str();
}

}  // namespace

std::string aesEncryptHex(const std::string& plaintext,
                          const std::string& keyHex) {
  const auto key = parseHexKey(keyHex);
  if (key.size() != 16) {
    return {};
  }

  const unsigned char zeroIv[16] = {};
  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
  if (ctx == nullptr) {
    return {};
  }

  std::vector<unsigned char> ciphertext(plaintext.size() + 16);
  int outLen1 = 0;
  int outLen2 = 0;

  if (EVP_EncryptInit_ex(ctx, EVP_aes_128_cbc(), nullptr, key.data(), zeroIv) != 1 ||
      EVP_EncryptUpdate(ctx, ciphertext.data(), &outLen1,
                        reinterpret_cast<const unsigned char*>(plaintext.data()),
                        static_cast<int>(plaintext.size())) != 1 ||
      EVP_EncryptFinal_ex(ctx, ciphertext.data() + outLen1, &outLen2) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return {};
  }
  EVP_CIPHER_CTX_free(ctx);
  return toUpperHex(ciphertext.data(), outLen1 + outLen2);
}

std::string aesDecryptUtf8(const std::string& cipherHex,
                           const std::string& keyHex) {
  const auto key = parseHexKey(keyHex);
  if (key.size() != 16) {
    return {};
  }

  const auto cipher = parseHexKey(cipherHex);
  if (cipher.empty()) {
    return {};
  }

  const unsigned char zeroIv[16] = {};
  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
  if (ctx == nullptr) {
    return {};
  }

  std::vector<unsigned char> plaintext(cipher.size());
  int outLen1 = 0;
  int outLen2 = 0;

  if (EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), nullptr, key.data(), zeroIv) != 1 ||
      EVP_DecryptUpdate(ctx, plaintext.data(), &outLen1, cipher.data(),
                        static_cast<int>(cipher.size())) != 1 ||
      EVP_DecryptFinal_ex(ctx, plaintext.data() + outLen1, &outLen2) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return {};
  }
  EVP_CIPHER_CTX_free(ctx);
  return std::string(reinterpret_cast<char*>(plaintext.data()), outLen1 + outLen2);
}

}  // namespace Byd
}  // namespace Linux
}  // namespace Supla

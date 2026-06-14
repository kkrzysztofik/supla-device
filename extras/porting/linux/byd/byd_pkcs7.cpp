/*
 Copyright (C) Krzysztof Krzysztofik
*/

#include "byd_pkcs7.h"

namespace Supla {
namespace Linux {
namespace Byd {

std::vector<unsigned char> addPkcs7(const unsigned char* data, size_t len,
                                    size_t blockSize) {
  const size_t remainder = len % blockSize;
  const unsigned char padLen =
      static_cast<unsigned char>(remainder == 0 ? blockSize : blockSize - remainder);
  std::vector<unsigned char> out;
  out.reserve(len + padLen);
  out.insert(out.end(), data, data + len);
  out.insert(out.end(), padLen, padLen);
  return out;
}

std::vector<unsigned char> stripPkcs7(const std::vector<unsigned char>& data) {
  if (data.empty()) {
    return data;
  }
  const unsigned char pad = data.back();
  if (pad == 0 || pad > 16 || data.size() < pad) {
    return data;
  }
  for (size_t i = data.size() - pad; i < data.size(); ++i) {
    if (data[i] != pad) {
      return data;
    }
  }
  return std::vector<unsigned char>(data.begin(), data.end() - pad);
}

}  // namespace Byd
}  // namespace Linux
}  // namespace Supla

/*
 Copyright (C) Krzysztof Krzysztofik
*/

#include "byd_sign.h"

#include "byd_hash.h"

#include <map>

namespace Supla {
namespace Linux {
namespace Byd {

std::string buildSignString(const std::map<std::string, std::string>& fields,
                            const std::string& password) {
  std::string joined;
  bool first = true;
  for (const auto& entry : fields) {
    if (!first) {
      joined += '&';
    }
    first = false;
    joined += entry.first;
    joined += '=';
    joined += entry.second;
  }
  joined += "&password=";
  joined += password;
  return joined;
}

std::string computeCheckcodeJson(const std::string& compactJson) {
  const std::string md5 = md5Hex(compactJson);
  if (md5.size() < 32) {
    return md5;
  }
  return md5.substr(24, 8) + md5.substr(8, 8) + md5.substr(16, 8) +
         md5.substr(0, 8);
}

}  // namespace Byd
}  // namespace Linux
}  // namespace Supla

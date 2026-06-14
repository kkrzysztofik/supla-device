/*
 Copyright (C) Krzysztof Krzysztofik
*/

#include "byd_types.h"

#include <algorithm>
#include <cctype>

namespace Supla {
namespace Linux {
namespace Byd {

std::string regionToBaseUrl(const std::string& region) {
  std::string r = region;
  std::transform(r.begin(), r.end(), r.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  if (r == "au") return "https://dilinkappoversea-au.byd.auto";
  if (r == "eu") return "https://dilinkappoversea-eu.byd.auto";
  if (r == "sg") return "https://dilinkappoversea-sg.byd.auto";
  if (r == "br") return "https://dilinkappoversea-br.byd.auto";
  if (r == "mx") return "https://dilinkappoversea-mx.byd.auto";
  if (r == "jp") return "https://dilinkappoversea-jp.byd.auto";
  if (r == "kr") return "https://dilinkappoversea-kr.byd.auto";
  if (r == "in") return "https://dilinkappoversea-in.byd.auto";
  if (r.empty()) return "https://dilinkappoversea-eu.byd.auto";
  if (r.rfind("http://", 0) == 0 || r.rfind("https://", 0) == 0) {
    return r;
  }
  return "https://dilinkappoversea-" + r + ".byd.auto";
}

std::string accountKey(const BydAccountConfig& account) {
  return account.baseUrl + "|" + account.username;
}

}  // namespace Byd
}  // namespace Linux
}  // namespace Supla

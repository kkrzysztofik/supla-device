/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#ifndef EXTRAS_PORTING_LINUX_BYD_BYD_HTTP_CLIENT_H_
#define EXTRAS_PORTING_LINUX_BYD_BYD_HTTP_CLIENT_H_

#include <string>

namespace Supla {
namespace Linux {
namespace Byd {

class BydHttpClient {
 public:
  BydHttpClient();
  ~BydHttpClient();

  BydHttpClient(const BydHttpClient&) = delete;
  BydHttpClient& operator=(const BydHttpClient&) = delete;

  bool postJson(const std::string& url,
                const std::string& body,
                const std::string& userAgent,
                int* statusCode,
                std::string* responseBody,
                std::string* error);

 private:
  void* curl_ = nullptr;
};

}  // namespace Byd
}  // namespace Linux
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_BYD_BYD_HTTP_CLIENT_H_

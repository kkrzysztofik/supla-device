/*
 Copyright (C) Krzysztof Krzysztofik
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

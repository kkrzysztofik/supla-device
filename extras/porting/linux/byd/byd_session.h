/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#ifndef EXTRAS_PORTING_LINUX_BYD_BYD_SESSION_H_
#define EXTRAS_PORTING_LINUX_BYD_BYD_SESSION_H_

#include <chrono>
#include <string>

namespace Supla {
namespace Linux {
namespace Byd {

class BydSession {
 public:
  BydSession() = default;
  BydSession(std::string userId,
             std::string signToken,
             std::string encryToken,
             double ttlSec);

  bool isExpired() const;
  const std::string& userId() const {
    return userId_;
  }
  std::string contentKey() const;
  std::string signKey() const;

 private:
  std::string userId_;
  std::string signToken_;
  std::string encryToken_;
  std::chrono::steady_clock::time_point createdAt_ =
      std::chrono::steady_clock::now();
  double ttlSec_ = 12 * 3600;
  mutable std::string contentKeyCache_;
  mutable std::string signKeyCache_;
};

}  // namespace Byd
}  // namespace Linux
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_BYD_BYD_SESSION_H_

/*
 Copyright (C) Krzysztof Krzysztofik
*/

#include "byd_session.h"

#include "byd_hash.h"

namespace Supla {
namespace Linux {
namespace Byd {

BydSession::BydSession(std::string userId,
                       std::string signToken,
                       std::string encryToken,
                       double ttlSec)
    : userId_(std::move(userId)),
      signToken_(std::move(signToken)),
      encryToken_(std::move(encryToken)),
      createdAt_(std::chrono::steady_clock::now()),
      ttlSec_(ttlSec) {}

bool BydSession::isExpired() const {
  if (ttlSec_ <= 0) {
    return false;
  }
  const auto age = std::chrono::steady_clock::now() - createdAt_;
  return age >= std::chrono::duration<double>(ttlSec_);
}

std::string BydSession::contentKey() const {
  if (contentKeyCache_.empty()) {
    contentKeyCache_ = md5Hex(encryToken_);
  }
  return contentKeyCache_;
}

std::string BydSession::signKey() const {
  if (signKeyCache_.empty()) {
    signKeyCache_ = md5Hex(signToken_);
  }
  return signKeyCache_;
}

}  // namespace Byd
}  // namespace Linux
}  // namespace Supla

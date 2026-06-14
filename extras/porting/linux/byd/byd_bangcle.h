/*
 Copyright (C) Krzysztof Krzysztofik

 White-box AES tables and CBC helpers for the Bangcle envelope layer.
 Ported from pyBYD (MIT), which extracted tables from libencrypt.so.
*/

#ifndef EXTRAS_PORTING_LINUX_BYD_BYD_BANGCLE_H_
#define EXTRAS_PORTING_LINUX_BYD_BYD_BANGCLE_H_

#include <cstdint>
#include <string>
#include <vector>

namespace Supla {
namespace Linux {
namespace Byd {

class BangcleCodec {
 public:
  explicit BangcleCodec(const std::string& tablesPath = {});

  bool loadTables();
  bool isLoaded() const { return loaded_; }

  std::string encodeEnvelope(const std::string& plaintext) const;
  std::vector<unsigned char> decodeEnvelope(const std::string& envelope) const;

 private:
  std::string tablesPath_;
  bool loaded_ = false;
  std::vector<unsigned char> invRound_;
  std::vector<unsigned char> invXor_;
  std::vector<unsigned char> invFirst_;
  std::vector<unsigned char> round_;
  std::vector<unsigned char> xorTables_;
  std::vector<unsigned char> finalTables_;
  unsigned char permDecrypt_[8] = {};
  unsigned char permEncrypt_[8] = {};
};

}  // namespace Byd
}  // namespace Linux
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_BYD_BYD_BANGCLE_H_

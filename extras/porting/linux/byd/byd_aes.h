/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#ifndef EXTRAS_PORTING_LINUX_BYD_BYD_AES_H_
#define EXTRAS_PORTING_LINUX_BYD_BYD_AES_H_

#include <string>

namespace Supla {
namespace Linux {
namespace Byd {

std::string aesEncryptHex(const std::string& plaintext,
                          const std::string& keyHex);
std::string aesDecryptUtf8(const std::string& cipherHex,
                           const std::string& keyHex);

}  // namespace Byd
}  // namespace Linux
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_BYD_BYD_AES_H_

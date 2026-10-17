/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#ifndef EXTRAS_PORTING_LINUX_BYD_BYD_HASH_H_
#define EXTRAS_PORTING_LINUX_BYD_BYD_HASH_H_

#include <map>
#include <string>

namespace Supla {
namespace Linux {
namespace Byd {

std::string md5Hex(const std::string& value);
std::string pwdLoginKey(const std::string& password);
std::string sha1Mixed(const std::string& value);
std::string computeCheckcode(const std::map<std::string, std::string>& payload);

}  // namespace Byd
}  // namespace Linux
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_BYD_BYD_HASH_H_

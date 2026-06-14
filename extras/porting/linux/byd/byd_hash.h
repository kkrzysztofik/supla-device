/*
 Copyright (C) Krzysztof Krzysztofik
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

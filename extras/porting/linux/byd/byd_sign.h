/*
 Copyright (C) Krzysztof Krzysztofik
*/

#ifndef EXTRAS_PORTING_LINUX_BYD_BYD_SIGN_H_
#define EXTRAS_PORTING_LINUX_BYD_BYD_SIGN_H_

#include <map>
#include <string>

namespace Supla {
namespace Linux {
namespace Byd {

std::string buildSignString(const std::map<std::string, std::string>& fields,
                            const std::string& password);
std::string computeCheckcodeJson(const std::string& compactJson);

}  // namespace Byd
}  // namespace Linux
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_BYD_BYD_SIGN_H_

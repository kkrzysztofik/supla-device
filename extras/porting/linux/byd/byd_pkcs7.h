/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#ifndef EXTRAS_PORTING_LINUX_BYD_BYD_PKCS7_H_
#define EXTRAS_PORTING_LINUX_BYD_BYD_PKCS7_H_

#include <cstddef>
#include <string>
#include <vector>

namespace Supla {
namespace Linux {
namespace Byd {

std::vector<unsigned char> addPkcs7(const unsigned char* data,
                                    size_t len,
                                    size_t blockSize = 16);
std::vector<unsigned char> stripPkcs7(const std::vector<unsigned char>& data);

}  // namespace Byd
}  // namespace Linux
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_BYD_BYD_PKCS7_H_

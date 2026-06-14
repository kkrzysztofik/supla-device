/*
 Copyright (C) Krzysztof Krzysztofik
*/

#ifndef EXTRAS_PORTING_LINUX_BYD_BYD_PKCS7_H_
#define EXTRAS_PORTING_LINUX_BYD_BYD_PKCS7_H_

#include <cstddef>
#include <string>
#include <vector>

namespace Supla {
namespace Linux {
namespace Byd {

std::vector<unsigned char> addPkcs7(const unsigned char* data, size_t len,
                                  size_t blockSize = 16);
std::vector<unsigned char> stripPkcs7(const std::vector<unsigned char>& data);

}  // namespace Byd
}  // namespace Linux
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_BYD_BYD_PKCS7_H_

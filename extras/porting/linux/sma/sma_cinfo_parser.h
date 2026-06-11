/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 YASDI reference (protocol behavior only)
 ----------------------------------------
 Reimplemented from YASDI — Yet Another SMA Data Implementation,
 Copyright (C) 2001-2008 SMA Solar Technology AG, licensed under the
 GNU Lesser General Public License v2.1 or later (LGPL-2.1+). Reference:
 yasdi/sdk/master/plant.c (TPlant_ScanChanInfoBuf).

 This file is original supla-device code; no YASDI source is incorporated.
*/

#ifndef EXTRAS_PORTING_LINUX_SMA_SMA_CINFO_PARSER_H_
#define EXTRAS_PORTING_LINUX_SMA_SMA_CINFO_PARSER_H_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "sma_types.h"

namespace Supla {
namespace Linux {
namespace Sma {

std::string trimSmaChannelName(const char* name, size_t maxLen = 16);

class SmaCinfoParser {
 public:
  static std::optional<std::vector<SmaChannelInfo>> parse(const uint8_t* data,
                                                          size_t len);

  static const SmaChannelInfo* findByName(
      const std::vector<SmaChannelInfo>& channels,
      const std::string& name);
};

}  // namespace Sma
}  // namespace Linux
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SMA_SMA_CINFO_PARSER_H_

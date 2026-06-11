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
 yasdi/sdk/master/netchannel.c, yasdi/sdk/master/statereadchan.c,
 yasdi/sdk/core/tools.c, yasdi/sdk/include/chandef.h.

 This file is original supla-device code; no YASDI source is incorporated.
*/

#ifndef EXTRAS_PORTING_LINUX_SMA_SMA_CHANNEL_CODEC_H_
#define EXTRAS_PORTING_LINUX_SMA_SMA_CHANNEL_CODEC_H_

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <utility>
#include <vector>

#include "sma_types.h"

namespace Supla {
namespace Linux {
namespace Sma {

class SmaChannelCodec {
 public:
  static uint16_t le16ToHost(const uint8_t* src);
  static uint32_t le32ToHost(const uint8_t* src);
  static float le32fToHost(const uint8_t* src);

  static bool parseGetDataValue(const uint8_t* data,
                                size_t len,
                                const SmaChannelDescriptor& channel,
                                double* outValue);

  static double applyGainOffset(double raw,
                                const SmaChannelDescriptor& channel);

  static bool channelMatchesFilter(const SmaChannelDescriptor& channel,
                                   uint16_t mask,
                                   uint8_t index);

  static bool parseBulkSpotValues(
      const uint8_t* data,
      size_t len,
      const std::vector<SmaChannelInfo>& catalog,
      std::map<std::pair<uint16_t, uint8_t>, double>* outValues);

  // Parses bulk CMD_GET_DATA in catalog order (YASDI spot mask 0x090f).
  static bool parseBulkSpotValuesByName(
      const uint8_t* data,
      size_t len,
      const std::vector<SmaChannelInfo>& catalog,
      std::map<std::string, double>* outValuesByName);
};

}  // namespace Sma
}  // namespace Linux
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SMA_SMA_CHANNEL_CODEC_H_

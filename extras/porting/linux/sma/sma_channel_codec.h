/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#ifndef EXTRAS_PORTING_LINUX_SMA_SMA_CHANNEL_CODEC_H_
#define EXTRAS_PORTING_LINUX_SMA_SMA_CHANNEL_CODEC_H_

#include <cstddef>
#include <cstdint>
#include <optional>

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

  static double applyGainOffset(double raw, const SmaChannelDescriptor& channel);
};

}  // namespace Sma
}  // namespace Linux
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SMA_SMA_CHANNEL_CODEC_H_

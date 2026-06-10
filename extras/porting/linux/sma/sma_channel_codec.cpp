/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include "sma_channel_codec.h"

#include <cmath>
#include <cstring>

namespace Supla {
namespace Linux {
namespace Sma {

namespace {

constexpr double kInvalidChannelValue = -1.0e300;

size_t valueSize(uint16_t ntype) {
  switch (ntype & 0x000f) {
    case kNtypeByte:
      return 1;
    case kNtypeWord:
      return 2;
    case kNtypeDword:
    case kNtypeFloat4:
      return 4;
    default:
      return 0;
  }
}

size_t arraySize(uint16_t ntype) {
  const size_t count = (ntype & 0xff00) >> 8;
  return count == 0 ? 1 : count;
}

bool readScalar(const uint8_t*& cursor,
                size_t& remaining,
                uint16_t ntype,
                double* out) {
  const size_t elemSize = valueSize(ntype);
  const size_t count = arraySize(ntype);
  if (elemSize == 0 || remaining < elemSize * count) {
    return false;
  }

  double value = 0.0;
  switch (ntype & 0x000f) {
    case kNtypeByte:
      value = *cursor;
      break;
    case kNtypeWord:
      value = SmaChannelCodec::le16ToHost(cursor);
      break;
    case kNtypeDword:
      value = SmaChannelCodec::le32ToHost(cursor);
      break;
    case kNtypeFloat4:
      value = SmaChannelCodec::le32fToHost(cursor);
      break;
    default:
      return false;
  }

  cursor += elemSize * count;
  remaining -= elemSize * count;
  if (out != nullptr) {
    *out = value;
  }
  return true;
}

}  // namespace

uint16_t SmaChannelCodec::le16ToHost(const uint8_t* src) {
  return static_cast<uint16_t>(src[0] | (src[1] << 8));
}

uint32_t SmaChannelCodec::le32ToHost(const uint8_t* src) {
  return static_cast<uint32_t>(src[0] | (src[1] << 8) | (src[2] << 16) |
                               (src[3] << 24));
}

float SmaChannelCodec::le32fToHost(const uint8_t* src) {
  const uint32_t bits = le32ToHost(src);
  float value = 0.0f;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

double SmaChannelCodec::applyGainOffset(double raw,
                                        const SmaChannelDescriptor& channel) {
  if (!std::isfinite(raw) || raw == kInvalidChannelValue) {
    return raw;
  }
  if (channel.ctype & kChCounter) {
    return raw * static_cast<double>(channel.gain);
  }
  if (channel.ctype & kChAnalog) {
    return raw * static_cast<double>(channel.gain) +
           static_cast<double>(channel.offset);
  }
  return raw;
}

bool SmaChannelCodec::parseGetDataValue(const uint8_t* data,
                                        size_t len,
                                        const SmaChannelDescriptor& channel,
                                        double* outValue) {
  if (data == nullptr || len < 5 || outValue == nullptr) {
    return false;
  }

  const uint8_t* cursor = data;
  size_t remaining = len;

  const uint16_t mask = le16ToHost(cursor);
  cursor += 2;
  remaining -= 2;
  const uint8_t chanNr = *cursor;
  cursor += 1;
  remaining -= 1;

  if (mask != channel.ctype || chanNr != channel.cindex) {
    return false;
  }

  if (remaining < 2) {
    return false;
  }
  cursor += 2;
  remaining -= 2;

  if (mask & kChSpot) {
    if (remaining < 8) {
      return false;
    }
    cursor += 8;
    remaining -= 8;
  }

  double raw = 0.0;
  if (!readScalar(cursor, remaining, channel.ntype, &raw)) {
    return false;
  }

  *outValue = applyGainOffset(raw, channel);
  return std::isfinite(*outValue);
}

}  // namespace Sma
}  // namespace Linux
}  // namespace Supla

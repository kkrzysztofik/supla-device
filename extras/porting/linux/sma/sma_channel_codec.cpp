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

#include "sma_channel_codec.h"

#include <supla/log_wrapper.h>

#include <cmath>
#include <cstring>
#include <map>
#include <string>
#include <vector>

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

bool SmaChannelCodec::channelMatchesFilter(const SmaChannelDescriptor& channel,
                                           uint16_t mask,
                                           uint8_t index) {
  if (mask == 0xffff) {
    return true;
  }

  // YASDI TNewChanListFilter_CheckChannel (netdevice.c): category bits
  // (PARA/SPOT/MEAN) are matched separately from value-type bits.
  constexpr uint16_t kChTest = 0x2000;
  constexpr uint16_t kChMean = 0x1000;
  constexpr uint16_t kChanTypeMask1 = kChPara | kChSpot | kChMean;
  constexpr uint16_t kChanTypeMask2 =
      kChAnalog | kChDigital | kChCounter | kChStatus;

  if ((channel.ctype & kChTest) != (mask & kChTest)) {
    return false;
  }
  if (((channel.ctype & kChanTypeMask1) & (mask & kChanTypeMask1)) == 0) {
    return false;
  }
  if (((channel.ctype & kChanTypeMask2) & (mask & kChanTypeMask2)) == 0) {
    return false;
  }
  return index == 0 || channel.cindex == index;
}

bool SmaChannelCodec::parseBulkSpotValues(
    const uint8_t* data,
    size_t len,
    const std::vector<SmaChannelInfo>& catalog,
    std::map<std::pair<uint16_t, uint8_t>, double>* outValues) {
  if (data == nullptr || outValues == nullptr || len < 5) {
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

  outValues->clear();
  size_t parsedCount = 0;
  for (const auto& channelInfo : catalog) {
    if (!channelMatchesFilter(channelInfo.descriptor, mask, chanNr)) {
      continue;
    }

    double raw = 0.0;
    if (!readScalar(cursor, remaining, channelInfo.descriptor.ntype, &raw)) {
      if (parsedCount > 0) {
        break;
      }
      return false;
    }
    ++parsedCount;

    const double value = applyGainOffset(raw, channelInfo.descriptor);
    if (!std::isfinite(value)) {
      return false;
    }

    (*outValues)[{channelInfo.descriptor.ctype,
                  channelInfo.descriptor.cindex}] = value;
  }

  return !outValues->empty();
}

bool SmaChannelCodec::parseBulkSpotValuesByName(
    const uint8_t* data,
    size_t len,
    const std::vector<SmaChannelInfo>& catalog,
    std::map<std::string, double>* outValuesByName) {
  if (data == nullptr || outValuesByName == nullptr || len < 5 ||
      catalog.empty()) {
    SUPLA_LOG_WARNING(
        "SmaBus: bulk parse aborted (invalid args or empty catalog)");
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

  if (remaining < 2) {
    SUPLA_LOG_WARNING(
        "SmaBus: bulk parse header too short (mask=0x%04x chanNr=%u len=%zu)",
        mask,
        chanNr,
        len);
    return false;
  }
  cursor += 2;
  remaining -= 2;

  if (mask & kChSpot) {
    if (remaining < 8) {
      SUPLA_LOG_WARNING(
          "SmaBus: bulk parse missing spot timestamp (remaining=%zu)",
          remaining);
      return false;
    }
    cursor += 8;
    remaining -= 8;
  }

  outValuesByName->clear();
  size_t parsedCount = 0;
  for (const auto& channelInfo : catalog) {
    if (!channelMatchesFilter(channelInfo.descriptor, mask, chanNr)) {
      continue;
    }

    double raw = 0.0;
    if (!readScalar(cursor, remaining, channelInfo.descriptor.ntype, &raw)) {
      // YASDI stops when the payload is shorter than the filtered catalog
      // (TStateChanReader_ScanUpdateValue) but keeps values already decoded.
      if (parsedCount > 0) {
        SUPLA_LOG_DEBUG(
            "SmaBus: bulk parse ended after %zu channels (%zu bytes left in "
            "payload)",
            parsedCount,
            remaining);
        break;
      }
      SUPLA_LOG_WARNING(
          "SmaBus: bulk parse failed at channel \"%s\" (#%zu, ctype=0x%04x "
          "cindex=%u ntype=0x%04x, remaining=%zu)",
          channelInfo.name.c_str(),
          parsedCount + 1,
          channelInfo.descriptor.ctype,
          channelInfo.descriptor.cindex,
          channelInfo.descriptor.ntype,
          remaining);
      return false;
    }
    ++parsedCount;

    const double value = applyGainOffset(raw, channelInfo.descriptor);
    if (!std::isfinite(value)) {
      SUPLA_LOG_WARNING("SmaBus: bulk parse non-finite value for \"%s\"",
                        channelInfo.name.c_str());
      return false;
    }

    (*outValuesByName)[channelInfo.name] = value;
  }

  if (remaining != 0) {
    SUPLA_LOG_VERBOSE("SmaBus: bulk parse finished with %zu trailing bytes",
                      remaining);
  }

  if (outValuesByName->empty()) {
    SUPLA_LOG_WARNING("SmaBus: bulk parse decoded no values (mask=0x%04x)",
                      mask);
    return false;
  }

  SUPLA_LOG_DEBUG("SmaBus: bulk parse decoded %zu values (mask=0x%04x)",
                  outValuesByName->size(),
                  mask);
  return true;
}

}  // namespace Sma
}  // namespace Linux
}  // namespace Supla

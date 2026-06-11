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

#include "sma_cinfo_parser.h"

#include <algorithm>
#include <cstring>

#include "sma_channel_codec.h"

namespace Supla {
namespace Linux {
namespace Sma {

namespace {

constexpr uint16_t kChTypeMask1 = kChSpot | kChIn | kChAnalog | 0x0400 | 0x1000 |
                                  0x2000;

}  // namespace

std::string trimSmaChannelName(const char* name, size_t maxLen) {
  if (name == nullptr) {
    return {};
  }
  std::string trimmed(name, strnlen(name, maxLen));
  while (!trimmed.empty() && trimmed.back() == ' ') {
    trimmed.pop_back();
  }
  const auto start = trimmed.find_first_not_of(' ');
  if (start == std::string::npos) {
    return {};
  }
  return trimmed.substr(start);
}

std::optional<std::vector<SmaChannelInfo>> SmaCinfoParser::parse(
    const uint8_t* data,
    size_t len) {
  if (data == nullptr || len == 0) {
    return std::nullopt;
  }

  std::vector<SmaChannelInfo> channels;
  size_t pos = 0;

  while (pos < len) {
    if (pos + 23 > len) {
      return std::nullopt;
    }

    SmaChannelInfo info;
    info.descriptor.cindex = data[pos];
    info.descriptor.ctype =
        SmaChannelCodec::le16ToHost(&data[pos + 1]);
    info.descriptor.ntype =
        static_cast<uint16_t>(SmaChannelCodec::le16ToHost(&data[pos + 3]));
    info.name = trimSmaChannelName(
        reinterpret_cast<const char*>(&data[pos + 7]), 16);
    pos += 23;

    if (info.descriptor.ctype & kChAnalog) {
      if (pos + 16 > len) {
        return std::nullopt;
      }
      info.descriptor.gain =
          SmaChannelCodec::le32fToHost(&data[pos + 8]);
      info.descriptor.offset =
          SmaChannelCodec::le32fToHost(&data[pos + 12]);
      pos += 16;
    } else if (info.descriptor.ctype & kChDigital) {
      if (pos + 32 > len) {
        return std::nullopt;
      }
      pos += 32;
    } else if (info.descriptor.ctype & kChCounter) {
      if (pos + 12 > len) {
        return std::nullopt;
      }
      info.descriptor.gain =
          SmaChannelCodec::le32fToHost(&data[pos + 8]);
      pos += 12;
    } else if (info.descriptor.ctype & kChStatus) {
      if (pos + 2 > len) {
        return std::nullopt;
      }
      const uint16_t stateCount =
          SmaChannelCodec::le16ToHost(&data[pos]);
      pos += 2;
      if (pos + stateCount > len) {
        return std::nullopt;
      }
      pos += stateCount;
    } else {
      return std::nullopt;
    }

    if (!info.name.empty()) {
      channels.push_back(std::move(info));
    }
  }

  return channels;
}

const SmaChannelInfo* SmaCinfoParser::findByName(
    const std::vector<SmaChannelInfo>& channels,
    const std::string& name) {
  const std::string trimmed = trimSmaChannelName(name.c_str(), name.size());
  for (const auto& channel : channels) {
    if (channel.name == trimmed || channel.name == name) {
      return &channel;
    }
  }
  return nullptr;
}

}  // namespace Sma
}  // namespace Linux
}  // namespace Supla

/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 Built-in catalogs mirror YASDI spot bulk order (CMD_GET_DATA mask 0x090f).
 Prefer a YASDI-exported profile file when available:
   cp yasdi/build/devices/WR33-008.bin ./sma-profiles/
*/

#include "sma_device_profiles.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace Supla {
namespace Linux {
namespace Sma {

namespace {

constexpr uint16_t kSpotWord = kChSpot | kChIn | kChAnalog;
constexpr uint16_t kSpotDword = kChSpot | kChIn | kChCounter;
constexpr uint16_t kSpotByte = kChSpot | kChIn | kChStatus;

SmaChannelInfo makeChannel(const char* name,
                           uint8_t cindex,
                           uint16_t ctype,
                           uint16_t ntype,
                           float gain,
                           float offset = 0.0f) {
  SmaChannelInfo info;
  info.name = name;
  info.descriptor.cindex = cindex;
  info.descriptor.ctype = ctype;
  info.descriptor.ntype = ntype;
  info.descriptor.gain = gain;
  info.descriptor.offset = offset;
  return info;
}

std::vector<SmaChannelInfo> wr33_008Catalog() {
  // Order matches YASDI TStateChanReader bulk parse for WR33-008.
  uint8_t idx = 0;
  return {
      makeChannel("Upv-Ist", ++idx, kSpotWord, kNtypeWord, 1.0f),
      makeChannel("Upv-Soll", ++idx, kSpotWord, kNtypeWord, 1.0f),
      makeChannel("Iac-Ist", ++idx, kSpotWord, kNtypeWord, 0.001f),
      makeChannel("Iac-Soll", ++idx, kSpotWord, kNtypeWord, 0.001f),
      makeChannel("Uac", ++idx, kSpotWord, kNtypeWord, 0.1f),
      makeChannel("Fac", ++idx, kSpotWord, kNtypeWord, 0.01f),
      makeChannel("Pac", ++idx, kSpotWord, kNtypeWord, 1.0f),
      makeChannel("Zac", ++idx, kSpotWord, kNtypeWord, 0.001f),
      makeChannel("dZac", ++idx, kSpotWord, kNtypeWord, 0.001f),
      makeChannel("Riso", ++idx, kSpotWord, kNtypeWord, 1.0f),
      makeChannel("Uac-Srr", ++idx, kSpotWord, kNtypeWord, 0.1f),
      makeChannel("Fac-Srr", ++idx, kSpotWord, kNtypeWord, 0.01f),
      makeChannel("Zac-Srr", ++idx, kSpotWord, kNtypeWord, 0.001f),
      makeChannel("Izac", ++idx, kSpotWord, kNtypeWord, 0.001f),
      makeChannel("Tkk", ++idx, kSpotWord, kNtypeWord, 0.1f),
      makeChannel("Ipv", ++idx, kSpotWord, kNtypeWord, 0.001f),
      makeChannel("Tkk max", ++idx, kSpotWord, kNtypeWord, 0.1f),
      makeChannel("Upv max", ++idx, kSpotWord, kNtypeWord, 1.0f),
      makeChannel("U-Fan", ++idx, kSpotWord, kNtypeWord, 1.0f),
      makeChannel("Freq1", ++idx, kSpotWord, kNtypeWord, 1.0f),
      makeChannel("Freq2", ++idx, kSpotWord, kNtypeWord, 1.0f),
      makeChannel("Freq3", ++idx, kSpotWord, kNtypeWord, 1.0f),
      makeChannel("Freq4", ++idx, kSpotWord, kNtypeWord, 1.0f),
      makeChannel("Freq5", ++idx, kSpotWord, kNtypeWord, 1.0f),
      makeChannel("FreqRes", ++idx, kSpotWord, kNtypeWord, 1.0f),
      makeChannel("FreqCnt", ++idx, kSpotWord, kNtypeWord, 1.0f),
      makeChannel("E-Total", ++idx, kSpotDword, kNtypeDword, 0.001f),
      makeChannel("h-Total", ++idx, kSpotDword, kNtypeDword, 1.0f),
      makeChannel("h-On", ++idx, kSpotDword, kNtypeDword, 1.0f),
      makeChannel("Netz-Ein", ++idx, kSpotDword, kNtypeDword, 1.0f),
      makeChannel("Event-Cnt", ++idx, kSpotDword, kNtypeDword, 1.0f),
      makeChannel("Seriennummer", ++idx, kSpotDword, kNtypeDword, 1.0f),
      makeChannel("Status", ++idx, kSpotByte, kNtypeByte, 1.0f),
      makeChannel("Phase", ++idx, kSpotByte, kNtypeByte, 1.0f),
      makeChannel("Fehler", ++idx, kSpotByte, kNtypeByte, 1.0f),
  };
}

std::string normalizeDeviceType(std::string type) {
  while (!type.empty() && type.back() == ' ') {
    type.pop_back();
  }
  const auto start = type.find_first_not_of(' ');
  if (start == std::string::npos) {
    return {};
  }
  type = type.substr(start);
  std::replace(type.begin(), type.end(), ' ', '-');
  return type;
}

}  // namespace

std::optional<std::vector<SmaChannelInfo>> SmaDeviceProfiles::catalogForType(
    const std::string& deviceType) {
  const std::string normalized = normalizeDeviceType(deviceType);
  if (normalized == "WR33-008" || normalized == "WR33-008.bin") {
    return wr33_008Catalog();
  }
  return std::nullopt;
}

}  // namespace Sma
}  // namespace Linux
}  // namespace Supla

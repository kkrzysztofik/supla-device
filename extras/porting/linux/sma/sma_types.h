/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 YASDI reference (protocol constants only)
 -----------------------------------------
 Constants and masks follow YASDI — Yet Another SMA Data Implementation,
 Copyright (C) 2001-2008 SMA Solar Technology AG, licensed under the
 GNU Lesser General Public License v2.1 or later (LGPL-2.1+). Reference:
 yasdi/sdk/include/chandef.h, yasdi/sdk/core/smadata_cmd.h,
 yasdi/sdk/protocol/smanet.h, yasdi/sdk/core/smadata_layer.h.
*/

#ifndef EXTRAS_PORTING_LINUX_SMA_SMA_TYPES_H_
#define EXTRAS_PORTING_LINUX_SMA_SMA_TYPES_H_

#include <cstdint>
#include <string>

namespace Supla {
namespace Linux {
namespace Sma {

constexpr uint8_t kHdlcSync = 0x7e;
constexpr uint8_t kHdlcEsc = 0x7d;
constexpr uint8_t kHdlcAddrBroadcast = 0xff;
constexpr uint8_t kHdlcCtrlUi = 0x03;

constexpr uint16_t kPppInitFcs16 = 0xffff;
constexpr uint16_t kPppGoodFcs16 = 0xf0b8;

constexpr uint16_t kProtPppSmadata1 = 0x4041;

constexpr uint8_t kCmdSynOnline = 10;
constexpr uint8_t kCmdGetData = 11;
constexpr uint8_t kCmdGetCinfo = 9;

constexpr uint8_t kCtrlAck = 0x40;
constexpr uint8_t kCtrlGroup = 0x80;

constexpr uint16_t kChAnalog = 0x0001;
constexpr uint16_t kChDigital = 0x0002;
constexpr uint16_t kChCounter = 0x0004;
constexpr uint16_t kChStatus = 0x0008;
constexpr uint16_t kChIn = 0x0100;
constexpr uint16_t kChPara = 0x0400;
constexpr uint16_t kChSpot = 0x0800;

constexpr uint16_t kChSpotOnlineMask = kChSpot | kChIn | kChAnalog |
kChDigital | kChCounter | kChStatus;

constexpr uint16_t kNtypeByte = 0x0000;
constexpr uint16_t kNtypeWord = 0x0001;
constexpr uint16_t kNtypeDword = 0x0002;
constexpr uint16_t kNtypeFloat4 = 0x0004;

enum class SerialMedia { RS232, RS485 };

struct SmaChannelDescriptor {
  uint16_t ctype = 0;
  uint8_t cindex = 0;
  uint16_t ntype = kNtypeFloat4;
  float gain = 1.0f;
  float offset = 0.0f;
  const char* suplaMapping = nullptr;
};

struct SmaChannelInfo {
  std::string name;
  SmaChannelDescriptor descriptor;
};

}  // namespace Sma
}  // namespace Linux
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SMA_SMA_TYPES_H_

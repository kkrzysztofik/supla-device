/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#ifndef EXTRAS_PORTING_LINUX_SMA_SMA_LOG_H_
#define EXTRAS_PORTING_LINUX_SMA_SMA_LOG_H_

#include <cstddef>
#include <cstdint>

namespace Supla {
namespace Linux {
namespace Sma {

const char* smaCmdName(uint8_t cmd);

void smaLogHexVerbose(const char* label, const uint8_t* data, size_t len);

void smaLogSmadataTx(uint8_t cmd,
                     uint16_t dest,
                     uint16_t src,
                     uint8_t pktCnt,
                     bool broadcast,
                     const uint8_t* txData,
                     size_t txLen);

void smaLogSmadataRx(uint8_t cmd,
                     uint16_t source,
                     uint16_t dest,
                     uint8_t pktCnt,
                     const uint8_t* payload,
                     size_t payloadLen);

struct SmaReadStats {
  int rawBytes = 0;
  int hdlcFrames = 0;
  int wrongProtocol = 0;
  int shortPayload = 0;
  int notAck = 0;
  int wrongCmd = 0;
  int wrongDest = 0;
  int wrongSrc = 0;
  uint8_t lastSeenCmd = 0;
  uint16_t lastSeenSrc = 0;
  uint16_t lastSeenDest = 0;
};

void smaLogReadStats(const char* context,
                     uint8_t expectedCmd,
                     int timeoutMs,
                     const SmaReadStats& stats);

}  // namespace Sma
}  // namespace Linux
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SMA_SMA_LOG_H_

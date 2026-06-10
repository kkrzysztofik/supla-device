/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#ifndef EXTRAS_PORTING_LINUX_SMA_SMANET_FRAMER_H_
#define EXTRAS_PORTING_LINUX_SMA_SMANET_FRAMER_H_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "sma_types.h"

namespace Supla {
namespace Linux {
namespace Sma {

struct SmanetFrame {
  uint16_t protocolId = 0;
  std::vector<uint8_t> payload;
};

class SmaNetFramer {
 public:
  static uint16_t calcFcsRaw(uint16_t fcs, const uint8_t* data, size_t len);
  static uint16_t calcChecksum(const uint8_t* data, size_t len);

  std::vector<uint8_t> encapsulate(uint16_t protocolId,
                                   const uint8_t* payload,
                                   size_t payloadLen);

  void reset();
  void feed(uint8_t byte);
  std::optional<SmanetFrame> takeFrame();

 private:
  std::optional<SmanetFrame> decodeCompletedBuffer() const;

  bool escapeNext_ = false;
  uint16_t fcsIn_ = kPppInitFcs16;
  std::vector<uint8_t> pktBuffer_;
  std::optional<SmanetFrame> pendingFrame_;
};

}  // namespace Sma
}  // namespace Linux
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SMA_SMANET_FRAMER_H_

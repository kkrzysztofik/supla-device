/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include "sma_log.h"

#include <supla/log_wrapper.h>

#include "sma_types.h"

#include <cstdio>
#include <string>

namespace Supla {
namespace Linux {
namespace Sma {

namespace {

constexpr size_t kMaxHexDump = 48;

}  // namespace

const char* smaCmdName(uint8_t cmd) {
  switch (cmd) {
    case kCmdCfgNetAddr:
      return "CMD_CFG_NETADR";
    case kCmdGetNetStart:
      return "CMD_GET_NET_START";
    case kCmdGetCinfo:
      return "CMD_GET_CINFO";
    case kCmdSynOnline:
      return "CMD_SYN_ONLINE";
    case kCmdGetData:
      return "CMD_GET_DATA";
    default:
      return "CMD_UNKNOWN";
  }
}

void smaLogHexVerbose(const char* label, const uint8_t* data, size_t len) {
  if (data == nullptr || len == 0) {
    SUPLA_LOG_VERBOSE("SmaBus: %s (empty)", label);
    return;
  }

  const size_t dumpLen = len < kMaxHexDump ? len : kMaxHexDump;
  std::string hex;
  hex.reserve(dumpLen * 3);
  char byteBuf[4];
  for (size_t i = 0; i < dumpLen; ++i) {
    std::snprintf(byteBuf, sizeof(byteBuf), "%02X ", data[i]);
    hex += byteBuf;
  }

  if (len > dumpLen) {
    SUPLA_LOG_VERBOSE("SmaBus: %s (%zu bytes): %s...", label, len, hex.c_str());
  } else {
    SUPLA_LOG_VERBOSE("SmaBus: %s (%zu bytes): %s", label, len, hex.c_str());
  }
}

void smaLogSmadataTx(uint8_t cmd,
                     uint16_t dest,
                     uint16_t src,
                     uint8_t pktCnt,
                     bool broadcast,
                     const uint8_t* txData,
                     size_t txLen) {
  SUPLA_LOG_DEBUG(
      "SmaBus: TX %s dest=0x%04x src=0x%04x pktCnt=%u broadcast=%d txLen=%zu",
      smaCmdName(cmd),
      dest,
      src,
      pktCnt,
      broadcast ? 1 : 0,
      txLen);
  if (txLen > 0 && txData != nullptr) {
    smaLogHexVerbose("TX payload", txData, txLen);
  }
}

void smaLogSmadataRx(uint8_t cmd,
                     uint16_t source,
                     uint16_t dest,
                     uint8_t pktCnt,
                     const uint8_t* payload,
                     size_t payloadLen) {
  SUPLA_LOG_DEBUG(
      "SmaBus: RX %s src=0x%04x dest=0x%04x pktCnt=%u payloadLen=%zu",
      smaCmdName(cmd),
      source,
      dest,
      pktCnt,
      payloadLen);
  if (payloadLen > 0 && payload != nullptr) {
    smaLogHexVerbose("RX payload", payload, payloadLen);
  }
}

void smaLogReadStats(const char* context,
                     uint8_t expectedCmd,
                     int timeoutMs,
                     const SmaReadStats& stats) {
  SUPLA_LOG_WARNING(
      "SmaBus: %s timeout waiting for %s (%d ms): rawBytes=%d hdlcFrames=%d "
      "wrongProto=%d shortPayload=%d notAck=%d wrongCmd=%d wrongDest=%d "
      "wrongSrc=%d lastSeen=%s src=0x%04x dest=0x%04x",
      context,
      smaCmdName(expectedCmd),
      timeoutMs,
      stats.rawBytes,
      stats.hdlcFrames,
      stats.wrongProtocol,
      stats.shortPayload,
      stats.notAck,
      stats.wrongCmd,
      stats.wrongDest,
      stats.wrongSrc,
      smaCmdName(stats.lastSeenCmd),
      stats.lastSeenSrc,
      stats.lastSeenDest);
}

}  // namespace Sma
}  // namespace Linux
}  // namespace Supla

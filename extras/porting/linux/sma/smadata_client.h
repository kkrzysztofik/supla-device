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
 yasdi/sdk/core/smadata_layer.c, yasdi/sdk/core/smadata_layer.h,
 yasdi/sdk/core/smadata_cmd.h, yasdi/sdk/master/statereadchan.c.

 This file is original supla-device code; no YASDI source is incorporated.
*/

#ifndef EXTRAS_PORTING_LINUX_SMA_SMADATA_CLIENT_H_
#define EXTRAS_PORTING_LINUX_SMA_SMADATA_CLIENT_H_

#include <cstdint>
#include <map>
#include <optional>
#include <utility>
#include <vector>

#include "sma_channel_codec.h"
#include "sma_cinfo_parser.h"
#include "sma_serial_port.h"
#include "smanet_framer.h"

namespace Supla {
namespace Linux {
namespace Sma {

struct SmaDataHead {
  uint16_t sourceAddr = 0;
  uint16_t destAddr = 0;
  uint8_t ctrl = 0;
  uint8_t pktCnt = 0;
  uint8_t cmd = 0;
};

struct SmaDataResponse {
  SmaDataHead head;
  std::vector<uint8_t> payload;
};

struct SmaDetectedDevice {
  uint32_t serial = 0;
  std::string type;
  uint16_t netAddress = 0;
};

class SmaDataClient {
 public:
  SmaDataClient(SmaSerialPort& port, uint16_t masterAddr, uint16_t deviceAddr);

  bool syncOnline(int waitAfterSec = 1);
  std::optional<SmaDetectedDevice> detectDevice(int timeoutMs = 20000);
  bool readChannel(const SmaChannelDescriptor& channel, double* outValue);
  bool verifyCinfo();
  std::optional<std::vector<SmaChannelInfo>> fetchChannelList();
  bool readSpotChannelsBulk(
      const std::vector<SmaChannelInfo>& catalog,
      std::map<std::string, double>* outValuesByName);

  void resetBackoff();
  int backoffSec() const;

 private:
  bool transact(uint16_t destAddr,
                uint8_t cmd,
                const uint8_t* txData,
                size_t txLen,
                bool broadcast,
                int timeoutMs,
                SmaDataResponse* response);

  bool sendSmadata(uint16_t destAddr,
                   uint8_t cmd,
                   const uint8_t* txData,
                   size_t txLen,
                   bool broadcast,
                   std::optional<uint8_t> forcedPktCnt = std::nullopt);

  std::optional<SmaDataResponse> readOneFrame(int timeoutMs,
                                              uint8_t expectedCmd,
                                              bool acceptAnySource = false);

  std::optional<SmaDataResponse> readResponse(int timeoutMs,
                                              uint8_t expectedCmd);

  static void hostToLe16(uint16_t val, uint8_t* dst);
  static void hostToLe32(uint32_t val, uint8_t* dst);

  SmaSerialPort& port_;
  uint16_t masterAddr_;
  uint16_t deviceAddr_;
  uint8_t pktCounter_ = 0;
  SmaNetFramer framer_;
  int consecutiveErrors_ = 0;
};

}  // namespace Sma
}  // namespace Linux
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SMA_SMADATA_CLIENT_H_

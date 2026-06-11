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

#include "smadata_client.h"

#include <supla/log_wrapper.h>

#include <chrono>
#include <ctime>
#include <thread>
#include <vector>

namespace Supla {
namespace Linux {
namespace Sma {

namespace {

constexpr int kDefaultTimeoutMs = 3000;
constexpr int kCinfoTimeoutMs = 5000;
constexpr int kMaxFollowPackets = 64;
constexpr int kMaxBackoffSec = 60;

bool parseSmadataPayload(const std::vector<uint8_t>& payload,
                         SmaDataHead* head) {
  if (head == nullptr || payload.size() < 7) {
    return false;
  }
  head->sourceAddr = SmaChannelCodec::le16ToHost(&payload[0]);
  head->destAddr = SmaChannelCodec::le16ToHost(&payload[2]);
  head->ctrl = payload[4];
  head->pktCnt = payload[5];
  head->cmd = payload[6];
  return true;
}

}  // namespace

SmaDataClient::SmaDataClient(SmaSerialPort& port,
                             uint16_t masterAddr,
                             uint16_t deviceAddr)
    : port_(port), masterAddr_(masterAddr), deviceAddr_(deviceAddr) {}

void SmaDataClient::hostToLe16(uint16_t val, uint8_t* dst) {
  dst[0] = static_cast<uint8_t>(val & 0xff);
  dst[1] = static_cast<uint8_t>((val >> 8) & 0xff);
}

void SmaDataClient::hostToLe32(uint32_t val, uint8_t* dst) {
  dst[0] = static_cast<uint8_t>(val & 0xff);
  dst[1] = static_cast<uint8_t>((val >> 8) & 0xff);
  dst[2] = static_cast<uint8_t>((val >> 16) & 0xff);
  dst[3] = static_cast<uint8_t>((val >> 24) & 0xff);
}

void SmaDataClient::resetBackoff() {
  consecutiveErrors_ = 0;
}

int SmaDataClient::backoffSec() const {
  if (consecutiveErrors_ <= 0) {
    return 0;
  }
  int backoff = 1;
  for (int i = 1; i < consecutiveErrors_ && backoff < kMaxBackoffSec; ++i) {
    backoff *= 2;
  }
  return backoff < kMaxBackoffSec ? backoff : kMaxBackoffSec;
}

bool SmaDataClient::sendSmadata(uint16_t destAddr,
                                uint8_t cmd,
                                const uint8_t* txData,
                                size_t txLen,
                                bool broadcast,
                                std::optional<uint8_t> forcedPktCnt) {
  uint8_t head[7] = {};
  const uint16_t effectiveDest = broadcast ? 0 : destAddr;
  hostToLe16(effectiveDest, &head[0]);
  hostToLe16(masterAddr_, &head[2]);
  if (broadcast) {
    head[4] |= kCtrlGroup;
  }
  head[5] = forcedPktCnt.has_value() ? *forcedPktCnt : pktCounter_++;
  head[6] = cmd;

  std::vector<uint8_t> payload;
  payload.reserve(7 + txLen);
  payload.insert(payload.end(), head, head + 7);
  if (txLen > 0 && txData != nullptr) {
    payload.insert(payload.end(), txData, txData + txLen);
  }

  const auto frame =
      framer_.encapsulate(kProtPppSmadata1, payload.data(), payload.size());
  return port_.writeAll(frame.data(), frame.size());
}

std::optional<SmaDataResponse> SmaDataClient::readOneFrame(
    int timeoutMs,
    uint8_t expectedCmd) {
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
  uint8_t buffer[256];

  while (std::chrono::steady_clock::now() < deadline) {
    const auto remaining =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now());
    const int waitMs =
        remaining.count() > 0 ? static_cast<int>(remaining.count()) : 1;
    const ssize_t readBytes = port_.readSome(buffer, sizeof(buffer), waitMs);
    if (readBytes < 0) {
      return std::nullopt;
    }
    for (ssize_t i = 0; i < readBytes; ++i) {
      framer_.feed(buffer[i]);
      if (auto frame = framer_.takeFrame()) {
        if (frame->protocolId != kProtPppSmadata1) {
          continue;
        }
        SmaDataHead head{};
        if (!parseSmadataPayload(frame->payload, &head)) {
          continue;
        }
        if (!(head.ctrl & kCtrlAck)) {
          continue;
        }
        if (head.cmd != expectedCmd) {
          continue;
        }
        if (head.destAddr != masterAddr_) {
          continue;
        }
        if (head.sourceAddr != deviceAddr_) {
          continue;
        }

        SmaDataResponse response;
        response.head = head;
        if (frame->payload.size() > 7) {
          response.payload.assign(frame->payload.begin() + 7,
                                  frame->payload.end());
        }
        return response;
      }
    }
  }

  return std::nullopt;
}

std::optional<SmaDataResponse> SmaDataClient::readResponse(
    int timeoutMs,
    uint8_t expectedCmd) {
  std::vector<uint8_t> accumulated;
  int fragments = 0;

  while (fragments < kMaxFollowPackets) {
    auto fragment = readOneFrame(timeoutMs, expectedCmd);
    if (!fragment) {
      return std::nullopt;
    }

    ++fragments;
    accumulated.insert(accumulated.end(),
                       fragment->payload.begin(),
                       fragment->payload.end());

    if (fragment->head.pktCnt == 0) {
      fragment->payload = std::move(accumulated);
      if (fragments > 1) {
        SUPLA_LOG_DEBUG("SmaDataClient: reassembled cmd %u in %d fragments",
                        expectedCmd,
                        fragments);
      }
      return fragment;
    }

    SUPLA_LOG_DEBUG(
        "SmaDataClient: cmd %u fragment pktCnt=%u, requesting follow-up",
        expectedCmd,
        fragment->head.pktCnt);

    framer_.reset();
    if (!sendSmadata(deviceAddr_,
                     expectedCmd,
                     nullptr,
                     0,
                     false,
                     fragment->head.pktCnt)) {
      return std::nullopt;
    }
  }

  SUPLA_LOG_WARNING("SmaDataClient: exceeded max follow-up packets for cmd %u",
                    expectedCmd);
  return std::nullopt;
}

bool SmaDataClient::transact(uint16_t destAddr,
                             uint8_t cmd,
                             const uint8_t* txData,
                             size_t txLen,
                             bool broadcast,
                             int timeoutMs,
                             SmaDataResponse* response) {
  framer_.reset();
  if (!sendSmadata(destAddr, cmd, txData, txLen, broadcast)) {
    ++consecutiveErrors_;
    return false;
  }

  if (broadcast && cmd == kCmdSynOnline) {
    std::this_thread::sleep_for(std::chrono::seconds(timeoutMs / 1000));
    resetBackoff();
    return true;
  }

  auto reply = readResponse(timeoutMs, cmd);
  if (!reply) {
    ++consecutiveErrors_;
    return false;
  }

  if (response != nullptr) {
    *response = *reply;
  }
  resetBackoff();
  return true;
}

bool SmaDataClient::syncOnline(int waitAfterSec) {
  const uint32_t unixTime = static_cast<uint32_t>(std::time(nullptr));
  uint8_t txData[4];
  hostToLe32(unixTime, txData);
  return transact(0,
                  kCmdSynOnline,
                  txData,
                  sizeof(txData),
                  true,
                  waitAfterSec * 1000,
                  nullptr);
}

bool SmaDataClient::readChannel(const SmaChannelDescriptor& channel,
                                double* outValue) {
  if (outValue == nullptr) {
    return false;
  }

  if (!syncOnline(1)) {
    return false;
  }

  uint8_t txData[3] = {
      static_cast<uint8_t>(channel.ctype & 0xff),
      static_cast<uint8_t>((channel.ctype >> 8) & 0xff),
      channel.cindex,
  };

  SmaDataResponse response;
  if (!transact(deviceAddr_,
                kCmdGetData,
                txData,
                sizeof(txData),
                false,
                kDefaultTimeoutMs,
                &response)) {
    return false;
  }

  return SmaChannelCodec::parseGetDataValue(response.payload.data(),
                                           response.payload.size(),
                                           channel,
                                           outValue);
}

bool SmaDataClient::verifyCinfo() {
  SmaDataResponse response;
  return transact(deviceAddr_,
                  kCmdGetCinfo,
                  nullptr,
                  0,
                  false,
                  kDefaultTimeoutMs,
                  &response);
}

std::optional<std::vector<SmaChannelInfo>> SmaDataClient::fetchChannelList() {
  if (!syncOnline(1)) {
    SUPLA_LOG_DEBUG("SmaDataClient: CMD_SYN_ONLINE failed before GET_CINFO");
  }

  SmaDataResponse response;
  if (!transact(deviceAddr_,
                kCmdGetCinfo,
                nullptr,
                0,
                false,
                kCinfoTimeoutMs,
                &response)) {
    return std::nullopt;
  }

  return SmaCinfoParser::parse(response.payload.data(), response.payload.size());
}

bool SmaDataClient::readSpotChannelsBulk(
    const std::vector<SmaChannelInfo>& catalog,
    std::map<std::pair<uint16_t, uint8_t>, double>* outValues) {
  if (outValues == nullptr || catalog.empty()) {
    return false;
  }

  if (!syncOnline(1)) {
    return false;
  }

  const uint8_t txData[3] = {
      static_cast<uint8_t>(kChSpotOnlineMask & 0xff),
      static_cast<uint8_t>((kChSpotOnlineMask >> 8) & 0xff),
      0,
  };

  SmaDataResponse response;
  if (!transact(deviceAddr_,
                kCmdGetData,
                txData,
                sizeof(txData),
                false,
                kDefaultTimeoutMs,
                &response)) {
    return false;
  }

  return SmaChannelCodec::parseBulkSpotValues(response.payload.data(),
                                             response.payload.size(),
                                             catalog,
                                             outValues);
}

}  // namespace Sma
}  // namespace Linux
}  // namespace Supla

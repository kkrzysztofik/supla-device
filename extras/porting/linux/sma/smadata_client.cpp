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

constexpr int kDefaultTimeoutMs = 4000;
constexpr int kCfgNetAddrTimeoutMs = 4000;
constexpr int kCinfoTimeoutMs = 4000;
constexpr int kCinfoRepeats = 5;
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

size_t countCatalogChannelsForMask(const std::vector<SmaChannelInfo>& catalog,
                                   uint16_t mask,
                                   uint8_t chanNr) {
  size_t count = 0;
  for (const auto& channelInfo : catalog) {
    if (SmaChannelCodec::channelMatchesFilter(channelInfo.descriptor,
                                              mask,
                                              chanNr)) {
      ++count;
    }
  }
  return count;
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

void SmaDataClient::setDeviceAddr(uint16_t addr) {
  deviceAddr_ = addr;
}

bool SmaDataClient::configureNetAddress(uint32_t serial, uint16_t newAddr) {
  uint8_t txData[6];
  hostToLe32(serial, &txData[0]);
  hostToLe16(newAddr, &txData[4]);

  framer_.reset();
  if (!sendSmadata(0, kCmdCfgNetAddr, txData, sizeof(txData), true)) {
    ++consecutiveErrors_;
    return false;
  }

  SmaReadStats stats;
  auto reply = readOneFrame(kCfgNetAddrTimeoutMs,
                            kCmdCfgNetAddr,
                            true,
                            &stats);
  if (!reply) {
    SUPLA_LOG_WARNING("SmaBus: %s failed for SN=%u newAddr=0x%04x",
                      smaCmdName(kCmdCfgNetAddr),
                      serial,
                      newAddr);
    ++consecutiveErrors_;
    return false;
  }

  deviceAddr_ = reply->head.sourceAddr;
  SUPLA_LOG_INFO(
      "SmaDataClient: %s OK SN=%u net_address=0x%04x",
      smaCmdName(kCmdCfgNetAddr),
      serial,
      deviceAddr_);
  resetBackoff();
  return true;
}

std::optional<SmaDetectedDevice> SmaDataClient::bringOnline(
    uint16_t desiredAddr,
    int detectTimeoutMs) {
  auto detected = detectDevice(detectTimeoutMs);
  if (!detected) {
    SUPLA_LOG_WARNING("SmaBus: %s detection failed",
                      smaCmdName(kCmdGetNetStart));
    return std::nullopt;
  }

  if (!configureNetAddress(detected->serial, desiredAddr)) {
    return std::nullopt;
  }

  detected->netAddress = deviceAddr_;
  SUPLA_LOG_INFO(
      "SmaDataClient: device online type=%s SN=%u net_address=0x%04x",
      detected->type.c_str(),
      detected->serial,
      detected->netAddress);
  return detected;
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

  smaLogSmadataTx(cmd,
                  effectiveDest,
                  masterAddr_,
                  head[5],
                  broadcast,
                  txData,
                  txLen);

  const auto frame =
      framer_.encapsulate(kProtPppSmadata1, payload.data(), payload.size());
  if (!port_.writeAll(frame.data(), frame.size())) {
    SUPLA_LOG_WARNING("SmaBus: serial write failed for %s (%zu wire bytes)",
                      smaCmdName(cmd),
                      frame.size());
    smaLogHexVerbose("wire TX", frame.data(), frame.size());
    return false;
  }

  SUPLA_LOG_VERBOSE("SmaBus: wire TX %zu bytes for %s",
                    frame.size(),
                    smaCmdName(cmd));
  return true;
}

std::optional<SmaDataResponse> SmaDataClient::readOneFrame(
    int timeoutMs,
    uint8_t expectedCmd,
    bool acceptAnySource,
    SmaReadStats* stats) {
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
      SUPLA_LOG_WARNING("SmaBus: serial read error while waiting for %s",
                        smaCmdName(expectedCmd));
      return std::nullopt;
    }
    if (readBytes == 0) {
      continue;
    }
    if (stats != nullptr) {
      stats->rawBytes += static_cast<int>(readBytes);
    }

    for (ssize_t i = 0; i < readBytes; ++i) {
      framer_.feed(buffer[i]);
      if (auto frame = framer_.takeFrame()) {
        if (stats != nullptr) {
          ++stats->hdlcFrames;
        }
        if (frame->protocolId != kProtPppSmadata1) {
          if (stats != nullptr) {
            ++stats->wrongProtocol;
          }
          continue;
        }
        SmaDataHead head{};
        if (!parseSmadataPayload(frame->payload, &head)) {
          if (stats != nullptr) {
            ++stats->shortPayload;
          }
          continue;
        }
        if (stats != nullptr) {
          stats->lastSeenCmd = head.cmd;
          stats->lastSeenSrc = head.sourceAddr;
          stats->lastSeenDest = head.destAddr;
        }
        if (!(head.ctrl & kCtrlAck)) {
          if (stats != nullptr) {
            ++stats->notAck;
          }
          SUPLA_LOG_VERBOSE("SmaBus: ignored non-ACK frame cmd=%s ctrl=0x%02x",
                            smaCmdName(head.cmd),
                            head.ctrl);
          continue;
        }
        if (head.cmd != expectedCmd) {
          if (stats != nullptr) {
            ++stats->wrongCmd;
          }
          SUPLA_LOG_VERBOSE(
              "SmaBus: ignored cmd=%s while waiting for %s (src=0x%04x)",
              smaCmdName(head.cmd),
              smaCmdName(expectedCmd),
              head.sourceAddr);
          continue;
        }
        if (head.destAddr != masterAddr_) {
          if (stats != nullptr) {
            ++stats->wrongDest;
          }
          SUPLA_LOG_VERBOSE(
              "SmaBus: ignored %s with dest=0x%04x (expected 0x%04x)",
              smaCmdName(head.cmd),
              head.destAddr,
              masterAddr_);
          continue;
        }
        if (!acceptAnySource && head.sourceAddr != deviceAddr_) {
          if (stats != nullptr) {
            ++stats->wrongSrc;
          }
          SUPLA_LOG_VERBOSE(
              "SmaBus: ignored %s from src=0x%04x (expected 0x%04x)",
              smaCmdName(head.cmd),
              head.sourceAddr,
              deviceAddr_);
          continue;
        }
        if (acceptAnySource && head.sourceAddr == 0) {
          if (stats != nullptr) {
            ++stats->wrongSrc;
          }
          continue;
        }

        SmaDataResponse response;
        response.head = head;
        if (frame->payload.size() > 7) {
          response.payload.assign(frame->payload.begin() + 7,
                                  frame->payload.end());
        }

        smaLogSmadataRx(head.cmd,
                        head.sourceAddr,
                        head.destAddr,
                        head.pktCnt,
                        response.payload.data(),
                        response.payload.size());
        return response;
      }
    }
  }

  if (stats != nullptr) {
    smaLogReadStats("readOneFrame", expectedCmd, timeoutMs, *stats);
  }
  return std::nullopt;
}

std::optional<SmaDataResponse> SmaDataClient::readResponse(
    int timeoutMs,
    uint8_t expectedCmd,
    SmaReadStats* stats) {
  std::vector<uint8_t> accumulated;
  int fragments = 0;

  while (fragments < kMaxFollowPackets) {
    auto fragment = readOneFrame(timeoutMs, expectedCmd, false, stats);
    if (!fragment) {
      if (fragments == 0) {
        SUPLA_LOG_WARNING("SmaBus: no response for %s", smaCmdName(expectedCmd));
      } else {
        SUPLA_LOG_WARNING(
            "SmaBus: incomplete %s after %d fragment(s), %zu bytes accumulated",
            smaCmdName(expectedCmd),
            fragments,
            accumulated.size());
      }
      return std::nullopt;
    }

    ++fragments;
    accumulated.insert(accumulated.end(),
                       fragment->payload.begin(),
                       fragment->payload.end());

    if (fragment->head.pktCnt == 0) {
      fragment->payload = std::move(accumulated);
      if (fragments > 1) {
        SUPLA_LOG_DEBUG("SmaDataClient: reassembled %s in %d fragments (%zu bytes)",
                        smaCmdName(expectedCmd),
                        fragments,
                        fragment->payload.size());
      }
      return fragment;
    }

    SUPLA_LOG_DEBUG(
        "SmaBus: %s fragment %d pktCnt=%u (%zu bytes), requesting follow-up",
        smaCmdName(expectedCmd),
        fragments,
        fragment->head.pktCnt,
        fragment->payload.size());

    framer_.reset();
    if (!sendSmadata(deviceAddr_,
                     expectedCmd,
                     nullptr,
                     0,
                     false,
                     fragment->head.pktCnt)) {
      SUPLA_LOG_WARNING("SmaBus: follow-up TX failed for %s pktCnt=%u",
                        smaCmdName(expectedCmd),
                        fragment->head.pktCnt);
      return std::nullopt;
    }
  }

  SUPLA_LOG_WARNING("SmaBus: exceeded max follow-up packets for %s",
                    smaCmdName(expectedCmd));
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
    SUPLA_LOG_DEBUG("SmaBus: %s sent, waiting %d ms",
                    smaCmdName(cmd),
                    timeoutMs);
    std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs));
    resetBackoff();
    return true;
  }

  SmaReadStats stats;
  auto reply = readResponse(timeoutMs, cmd, &stats);
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
  SUPLA_LOG_DEBUG("SmaBus: sending %s (unixTime=%u, wait=%ds)",
                  smaCmdName(kCmdSynOnline),
                  unixTime,
                  waitAfterSec);
  if (!transact(0,
                kCmdSynOnline,
                txData,
                sizeof(txData),
                true,
                waitAfterSec * 1000,
                nullptr)) {
    SUPLA_LOG_WARNING("SmaBus: %s failed", smaCmdName(kCmdSynOnline));
    return false;
  }
  return true;
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
                  kCinfoTimeoutMs,
                  &response);
}

std::optional<SmaDetectedDevice> SmaDataClient::detectDevice(int timeoutMs) {
  framer_.reset();
  if (!sendSmadata(0, kCmdGetNetStart, nullptr, 0, true)) {
    return std::nullopt;
  }

  SmaReadStats stats;
  auto reply = readOneFrame(timeoutMs, kCmdGetNetStart, true, &stats);
  if (!reply || reply->payload.size() < 12) {
    SUPLA_LOG_WARNING("SmaBus: %s failed (payload=%zu)",
                      smaCmdName(kCmdGetNetStart),
                      reply ? reply->payload.size() : 0);
    return std::nullopt;
  }

  SmaDetectedDevice device;
  device.netAddress = reply->head.sourceAddr;
  device.serial = SmaChannelCodec::le32ToHost(reply->payload.data());
  device.type.assign(reinterpret_cast<const char*>(&reply->payload[4]), 8);
  while (!device.type.empty() && device.type.back() == ' ') {
    device.type.pop_back();
  }
  const auto start = device.type.find_first_not_of(' ');
  if (start != std::string::npos) {
    device.type = device.type.substr(start);
  }

  SUPLA_LOG_INFO(
      "SmaDataClient: detected SMA device type=%s SN=%u net_address=0x%04x",
      device.type.c_str(),
      device.serial,
      device.netAddress);
  return device;
}

std::optional<std::vector<SmaChannelInfo>> SmaDataClient::fetchChannelList() {
  if (!syncOnline(1)) {
    SUPLA_LOG_DEBUG("SmaDataClient: %s skipped/failed before GET_CINFO",
                    smaCmdName(kCmdSynOnline));
  }

  for (int attempt = 0; attempt < kCinfoRepeats; ++attempt) {
    SmaDataResponse response;
    if (!transact(deviceAddr_,
                  kCmdGetCinfo,
                  nullptr,
                  0,
                  false,
                  kCinfoTimeoutMs,
                  &response)) {
      SUPLA_LOG_DEBUG("SmaBus: %s attempt %d/%d failed",
                      smaCmdName(kCmdGetCinfo),
                      attempt + 1,
                      kCinfoRepeats);
      continue;
    }

    if (auto catalog =
            SmaCinfoParser::parse(response.payload.data(),
                                  response.payload.size())) {
      SUPLA_LOG_INFO("SmaDataClient: %s returned %zu channels (%zu bytes)",
                     smaCmdName(kCmdGetCinfo),
                     catalog->size(),
                     response.payload.size());
      return catalog;
    }

    SUPLA_LOG_WARNING("SmaBus: %s response parse failed (%zu bytes)",
                      smaCmdName(kCmdGetCinfo),
                      response.payload.size());
  }

  return std::nullopt;
}

bool SmaDataClient::readSpotChannelsBulk(
    const std::vector<SmaChannelInfo>& catalog,
    std::map<std::string, double>* outValuesByName) {
  if (outValuesByName == nullptr || catalog.empty()) {
    SUPLA_LOG_WARNING("SmaBus: bulk read skipped (empty catalog)");
    return false;
  }

  const size_t matchingChannels = countCatalogChannelsForMask(
      catalog, kChSpotOnlineMask, 0);
  SUPLA_LOG_DEBUG(
      "SmaBus: bulk spot read catalog=%zu channels, %zu match mask 0x%04x",
      catalog.size(),
      matchingChannels,
      kChSpotOnlineMask);

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
    SUPLA_LOG_WARNING(
        "SmaBus: %s transact failed for device 0x%04x",
        smaCmdName(kCmdGetData),
        deviceAddr_);
    return false;
  }

  SUPLA_LOG_DEBUG("SmaBus: %s payload %zu bytes, parsing %zu catalog channels",
                  smaCmdName(kCmdGetData),
                  response.payload.size(),
                  matchingChannels);

  if (!SmaChannelCodec::parseBulkSpotValuesByName(response.payload.data(),
                                                  response.payload.size(),
                                                  catalog,
                                                  outValuesByName)) {
    SUPLA_LOG_WARNING(
        "SmaBus: %s payload parse failed (payload=%zu bytes, catalog=%zu, "
        "maskMatches=%zu)",
        smaCmdName(kCmdGetData),
        response.payload.size(),
        catalog.size(),
        matchingChannels);
    smaLogHexVerbose("GET_DATA payload", response.payload.data(),
                     response.payload.size());
    return false;
  }

  SUPLA_LOG_DEBUG("SmaBus: bulk spot read OK, %zu channel values",
                  outValuesByName->size());
  for (const auto& entry : *outValuesByName) {
    SUPLA_LOG_VERBOSE("SmaBus: spot %s = %.6f",
                      entry.first.c_str(),
                      entry.second);
  }
  return true;
}

}  // namespace Sma
}  // namespace Linux
}  // namespace Supla

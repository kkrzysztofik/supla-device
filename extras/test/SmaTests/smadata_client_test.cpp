/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include "smadata_client.h"

#include <fcntl.h>
#include <gtest/gtest.h>
#include <poll.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "sma_channel_codec.h"
#include "sma_serial_port.h"
#include "smanet_framer.h"

using Supla::Linux::Sma::kCmdCfgNetAddr;
using Supla::Linux::Sma::kCmdGetNetStart;
using Supla::Linux::Sma::kCtrlAck;
using Supla::Linux::Sma::kProtPppSmadata1;
using Supla::Linux::Sma::SerialMedia;
using Supla::Linux::Sma::SmaChannelCodec;
using Supla::Linux::Sma::SmaDataClient;
using Supla::Linux::Sma::SmaDataHead;
using Supla::Linux::Sma::SmanetFrame;
using Supla::Linux::Sma::SmaNetFramer;
using Supla::Linux::Sma::SmaSerialPort;

namespace {

class PtyPair {
 public:
  PtyPair() {
    masterFd_ = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
    EXPECT_GE(masterFd_, 0);
    EXPECT_EQ(grantpt(masterFd_), 0);
    EXPECT_EQ(unlockpt(masterFd_), 0);
    const char* path = ptsname(masterFd_);
    EXPECT_NE(path, nullptr);
    if (path != nullptr) {
      slavePath_ = path;
    }
  }

  ~PtyPair() {
    if (masterFd_ >= 0) {
      close(masterFd_);
    }
  }

  PtyPair(const PtyPair&) = delete;
  PtyPair& operator=(const PtyPair&) = delete;

  int masterFd() const {
    return masterFd_;
  }

  const std::string& slavePath() const {
    return slavePath_;
  }

 private:
  int masterFd_ = -1;
  std::string slavePath_;
};

void appendLe16(std::vector<uint8_t>* out, uint16_t value) {
  out->push_back(static_cast<uint8_t>(value & 0xff));
  out->push_back(static_cast<uint8_t>((value >> 8) & 0xff));
}

void appendLe32(std::vector<uint8_t>* out, uint32_t value) {
  out->push_back(static_cast<uint8_t>(value & 0xff));
  out->push_back(static_cast<uint8_t>((value >> 8) & 0xff));
  out->push_back(static_cast<uint8_t>((value >> 16) & 0xff));
  out->push_back(static_cast<uint8_t>((value >> 24) & 0xff));
}

std::vector<uint8_t> makeSmadataPayload(uint16_t source,
                                        uint16_t dest,
                                        uint8_t ctrl,
                                        uint8_t pktCnt,
                                        uint8_t cmd,
                                        const std::vector<uint8_t>& body) {
  std::vector<uint8_t> payload;
  appendLe16(&payload, source);
  appendLe16(&payload, dest);
  payload.push_back(ctrl);
  payload.push_back(pktCnt);
  payload.push_back(cmd);
  payload.insert(payload.end(), body.begin(), body.end());
  return payload;
}

bool writeFrame(int fd,
                uint16_t source,
                uint16_t dest,
                uint8_t cmd,
                const std::vector<uint8_t>& body = {}) {
  SmaNetFramer framer;
  const auto payload = makeSmadataPayload(source, dest, kCtrlAck, 0, cmd, body);
  const auto wire =
      framer.encapsulate(kProtPppSmadata1, payload.data(), payload.size());
  return write(fd, wire.data(), wire.size()) ==
         static_cast<ssize_t>(wire.size());
}

std::optional<SmanetFrame> readFrame(int fd, int timeoutMs) {
  SmaNetFramer framer;
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
  uint8_t buffer[128];

  while (std::chrono::steady_clock::now() < deadline) {
    pollfd pfd{};
    pfd.fd = fd;
    pfd.events = POLLIN;
    const int ready = poll(&pfd, 1, 20);
    if (ready <= 0) {
      continue;
    }

    const ssize_t bytes = read(fd, buffer, sizeof(buffer));
    if (bytes <= 0) {
      continue;
    }
    for (ssize_t i = 0; i < bytes; ++i) {
      framer.feed(buffer[i]);
      if (auto frame = framer.takeFrame()) {
        return frame;
      }
    }
  }

  return std::nullopt;
}

bool parseHead(const SmanetFrame& frame, SmaDataHead* head) {
  if (frame.payload.size() < 7) {
    return false;
  }
  head->sourceAddr = SmaChannelCodec::le16ToHost(frame.payload.data());
  head->destAddr = SmaChannelCodec::le16ToHost(frame.payload.data() + 2);
  head->ctrl = frame.payload[4];
  head->pktCnt = frame.payload[5];
  head->cmd = frame.payload[6];
  return true;
}

void runResponder(int fd,
                  uint8_t expectedCmd,
                  uint16_t responseSource,
                  std::vector<uint8_t> responseBody,
                  std::atomic<uint8_t>* observedCmd) {
  auto request = readFrame(fd, 2000);
  if (!request) {
    return;
  }

  SmaDataHead head{};
  if (!parseHead(*request, &head)) {
    return;
  }
  observedCmd->store(head.cmd);
  if (head.cmd == expectedCmd) {
    writeFrame(fd, responseSource, head.sourceAddr, expectedCmd, responseBody);
  }
}

}  // namespace

TEST(SmaDataClientTest, ConfigureNetAddressSendsCommandAndAcceptsAck) {
  PtyPair pty;
  SmaSerialPort port(pty.slavePath(), 9600, SerialMedia::RS232);
  ASSERT_TRUE(port.open());

  std::atomic<uint8_t> observedCmd{0};
  std::thread responder(runResponder,
                        pty.masterFd(),
                        kCmdCfgNetAddr,
                        0x0007,
                        std::vector<uint8_t>{},
                        &observedCmd);

  SmaDataClient client(port, 0, 1);
  EXPECT_TRUE(client.configureNetAddress(0x12345678, 7));

  responder.join();
  EXPECT_EQ(observedCmd.load(), kCmdCfgNetAddr);
}

TEST(SmaDataClientTest, DetectDeviceParsesSerialAndTrimmedDeviceType) {
  PtyPair pty;
  SmaSerialPort port(pty.slavePath(), 9600, SerialMedia::RS232);
  ASSERT_TRUE(port.open());

  std::vector<uint8_t> body;
  appendLe32(&body, 0x12345678);
  const char type[] = "WR33-008";
  body.insert(body.end(), type, type + 8);

  std::atomic<uint8_t> observedCmd{0};
  std::thread responder(runResponder,
                        pty.masterFd(),
                        kCmdGetNetStart,
                        0x0003,
                        body,
                        &observedCmd);

  SmaDataClient client(port, 0, 1);
  const auto detected = client.detectDevice(2000);

  responder.join();
  ASSERT_TRUE(detected.has_value());
  EXPECT_EQ(observedCmd.load(), kCmdGetNetStart);
  EXPECT_EQ(detected->serial, 0x12345678u);
  EXPECT_EQ(detected->type, "WR33-008");
  EXPECT_EQ(detected->netAddress, 0x0003);
}

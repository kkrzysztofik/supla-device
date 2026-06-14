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
 yasdi/sdk/driver/serial_posix.c.

 This file is original supla-device code; no YASDI source is incorporated.
*/

#include "sma_serial_port.h"

#include <supla/log_wrapper.h>
#include <sys/ioctl.h>

#include <cerrno>
#include <chrono>
#include <cstring>
#include <thread>
#include <utility>

namespace Supla {
namespace Linux {
namespace Sma {

namespace {

void logDirectionControlError(int fd, const char* operation) {
  SUPLA_LOG_WARNING(
      "SmaBus: RS485 direction control failed fd=%d op=%s "
      "(errno=%d %s)",
      fd,
      operation,
      errno,
      std::strerror(errno));
}

}  // namespace

SmaSerialPort::SmaSerialPort(std::string devicePath,
                             int baud,
                             SerialMedia media)
    : port_(std::move(devicePath), baud), media_(media) {
}

SmaSerialPort::~SmaSerialPort() {
  close();
}

bool SmaSerialPort::open() {
  if (!port_.open()) {
    SUPLA_LOG_WARNING("SmaBus: open %s failed (errno=%d %s)",
                      port_.devicePath().c_str(),
                      errno,
                      std::strerror(errno));
    return false;
  }

  SUPLA_LOG_DEBUG("SmaBus: opened %s @ %d baud (%s)",
                  port_.devicePath().c_str(),
                  port_.baud(),
                  media_ == SerialMedia::RS485 ? "RS485" : "RS232");
  return true;
}

void SmaSerialPort::close() {
  port_.close();
}

bool SmaSerialPort::isOpen() const {
  return port_.isOpen();
}

void SmaSerialPort::flushRx() {
  port_.flushRx();
}

void SmaSerialPort::waitBusFree() {
  // YASDI serial_wait_bus_free() arbitrates via DCD only on powerline media,
  // not RS232/RS485 (serial_posix.c).
}

bool SmaSerialPort::prepareSend() {
  if (media_ != SerialMedia::RS485 || !port_.isOpen()) {
    return true;
  }
  if (!port_.setModemFlag(TIOCM_RTS, true)) {
    logDirectionControlError(port_.fd(), "set RTS");
    return false;
  }
  if (!port_.setModemFlag(TIOCM_DTR, false)) {
    logDirectionControlError(port_.fd(), "clear DTR");
    return false;
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  return true;
}

bool SmaSerialPort::prepareRecv(bool skipDrain) {
  if (media_ != SerialMedia::RS485 || !port_.isOpen()) {
    return true;
  }
  if (!skipDrain && !port_.drain()) {
    logDirectionControlError(port_.fd(), "tcdrain");
    return false;
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  if (!port_.setModemFlag(TIOCM_RTS, false)) {
    logDirectionControlError(port_.fd(), "clear RTS");
    return false;
  }
  if (!port_.setModemFlag(TIOCM_DTR, true)) {
    logDirectionControlError(port_.fd(), "set DTR");
    return false;
  }
  return true;
}

bool SmaSerialPort::writeAll(const uint8_t* data, size_t len) {
  if (!port_.isOpen() || data == nullptr || len == 0) {
    return false;
  }

  waitBusFree();
  if (!prepareSend()) {
    return false;
  }

  if (!port_.writeAllRaw(data, len, "SmaBus", 1, true)) {
    return false;
  }

  if (!port_.drain()) {
    logDirectionControlError(port_.fd(), "tcdrain after write");
    return false;
  }
  return prepareRecv(true);
}

ssize_t SmaSerialPort::readSome(uint8_t* buffer, size_t maxLen, int timeoutMs) {
  return port_.readSome(buffer,
                        maxLen,
                        timeoutMs,
                        "SmaBus",
                        false,
                        false,
                        false);
}

}  // namespace Sma
}  // namespace Linux
}  // namespace Supla

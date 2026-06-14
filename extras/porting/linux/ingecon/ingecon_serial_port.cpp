/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include "ingecon_serial_port.h"

#include <supla/log_wrapper.h>
#include <sys/ioctl.h>

#include <cerrno>
#include <chrono>
#include <cstring>
#include <thread>
#include <utility>

namespace Supla {
namespace Linux {
namespace Ingecon {

SerialPort::SerialPort(std::string devicePath, int baud)
    : port_(std::move(devicePath), baud) {
}

SerialPort::~SerialPort() {
  close();
}

bool SerialPort::open() {
  if (!port_.open()) {
    SUPLA_LOG_WARNING("IngeconBus: open %s failed (errno=%d %s)",
                      port_.devicePath().c_str(),
                      errno,
                      std::strerror(errno));
    return false;
  }
  setDtr(false);
  setRts(false);
  SUPLA_LOG_DEBUG("IngeconBus: opened %s @ %d baud",
                  port_.devicePath().c_str(),
                  port_.baud());
  return true;
}

void SerialPort::close() {
  if (port_.isOpen()) {
    SUPLA_LOG_DEBUG("IngeconBus: closing %s fd=%d",
                    port_.devicePath().c_str(),
                    port_.fd());
  }
  port_.close();
}

bool SerialPort::isOpen() const {
  return port_.isOpen();
}

bool SerialPort::setRts(bool enabled) {
  if (!port_.setModemFlag(TIOCM_RTS, enabled)) {
    SUPLA_LOG_VERBOSE("IngeconBus: TIOCMSET RTS=%d %s failed (errno=%d %s)",
                      enabled,
                      port_.devicePath().c_str(),
                      errno,
                      std::strerror(errno));
    return false;
  }
  SUPLA_LOG_VERBOSE("IngeconBus: RTS=%d %s",
                    enabled,
                    port_.devicePath().c_str());
  return true;
}

bool SerialPort::setDtr(bool enabled) {
  if (!port_.setModemFlag(TIOCM_DTR, enabled)) {
    SUPLA_LOG_VERBOSE("IngeconBus: TIOCMSET DTR=%d %s failed (errno=%d %s)",
                      enabled,
                      port_.devicePath().c_str(),
                      errno,
                      std::strerror(errno));
    return false;
  }
  SUPLA_LOG_VERBOSE("IngeconBus: DTR=%d %s",
                    enabled,
                    port_.devicePath().c_str());
  return true;
}

bool SerialPort::writeAll(const uint8_t* data,
                          size_t len,
                          bool rtsToggle,
                          int turnaroundDelayMs) {
  if (!port_.isOpen() || data == nullptr || len == 0) {
    SUPLA_LOG_WARNING("IngeconBus: invalid write request fd=%d len=%zu",
                      port_.fd(),
                      len);
    return false;
  }

  SUPLA_LOG_VERBOSE("IngeconBus: serial write start %s len=%zu",
                    port_.devicePath().c_str(),
                    len);
  if (rtsToggle) {
    setRts(true);
  }

  if (!port_.writeAllRaw(data, len, "IngeconBus", 5, false)) {
    if (rtsToggle) {
      setRts(false);
    }
    return false;
  }

  if (!port_.drain()) {
    SUPLA_LOG_WARNING("IngeconBus: tcdrain %s failed (errno=%d %s)",
                      port_.devicePath().c_str(),
                      errno,
                      std::strerror(errno));
    if (rtsToggle) {
      setRts(false);
    }
    return false;
  }
  if (rtsToggle && turnaroundDelayMs > 0) {
    SUPLA_LOG_VERBOSE("IngeconBus: RS485 turn-around delay %d ms on %s",
                      turnaroundDelayMs,
                      port_.devicePath().c_str());
    std::this_thread::sleep_for(std::chrono::milliseconds(turnaroundDelayMs));
  }
  if (rtsToggle) {
    setRts(false);
  }
  SUPLA_LOG_VERBOSE("IngeconBus: serial write complete %s len=%zu",
                    port_.devicePath().c_str(),
                    len);
  return true;
}

ssize_t SerialPort::readSome(uint8_t* buffer, size_t maxLen, int timeoutMs) {
  return port_.readSome(buffer,
                        maxLen,
                        timeoutMs,
                        "IngeconBus",
                        true,
                        true,
                        true);
}

void SerialPort::flushRx() {
  if (port_.isOpen()) {
    SUPLA_LOG_VERBOSE("IngeconBus: flush RX %s", port_.devicePath().c_str());
    port_.flushRx();
  }
}

void SerialPort::flushRxTx() {
  if (port_.isOpen()) {
    SUPLA_LOG_VERBOSE("IngeconBus: flush RX/TX %s", port_.devicePath().c_str());
    port_.flushRxTx();
  }
}

}  // namespace Ingecon
}  // namespace Linux
}  // namespace Supla

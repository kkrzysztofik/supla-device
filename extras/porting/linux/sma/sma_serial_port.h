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

#ifndef EXTRAS_PORTING_LINUX_SMA_SMA_SERIAL_PORT_H_
#define EXTRAS_PORTING_LINUX_SMA_SMA_SERIAL_PORT_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "sma_types.h"

namespace Supla {
namespace Linux {
namespace Sma {

class SmaSerialPort {
 public:
  SmaSerialPort(std::string devicePath, int baud, SerialMedia media);
  ~SmaSerialPort();

  SmaSerialPort(const SmaSerialPort&) = delete;
  SmaSerialPort& operator=(const SmaSerialPort&) = delete;

  bool open();
  void close();
  bool isOpen() const;

  bool writeAll(const uint8_t* data, size_t len);
  ssize_t readSome(uint8_t* buffer, size_t maxLen, int timeoutMs);
  void flushRx();

  bool prepareSend();
  // Switches the RS485 port to receive mode. Returns true on success, false
  // on failure (e.g., tcdrain or modem status control error). When skipDrain
  // is false (default), pending transmit data is drained via tcdrain() before
  // switching; pass true to skip the drain for rapid mode switches or when
  // the upstream has already flushed the bus. Only has effect for RS485 media.
  bool prepareRecv(bool skipDrain = false);
  void waitBusFree();

 private:
  bool configureTermios();

  std::string devicePath_;
  int baud_;
  SerialMedia media_;
  int fd_ = -1;
};

}  // namespace Sma
}  // namespace Linux
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SMA_SMA_SERIAL_PORT_H_

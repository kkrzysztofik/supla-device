/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
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

  void prepareSend();
  void prepareRecv();
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

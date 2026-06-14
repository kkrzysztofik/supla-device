/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 YASDI reference (protocol behavior only)
 ----------------------------------------
 Portions of the serial configuration and RS485 modem-control behavior are
 reimplemented from YASDI -- Yet Another SMA Data Implementation,
 Copyright (C) 2001-2008 SMA Solar Technology AG, licensed under the
 GNU Lesser General Public License v2.1 or later (LGPL-2.1+). Reference:
 yasdi/sdk/driver/serial_posix.c.

 This file is original supla-device code; no YASDI source is incorporated.
*/

#ifndef EXTRAS_PORTING_LINUX_LINUX_SERIAL_PORT_H_
#define EXTRAS_PORTING_LINUX_LINUX_SERIAL_PORT_H_

#include <cstddef>
#include <cstdint>
#include <sys/types.h>
#include <string>

namespace Supla {
namespace Linux {

class LinuxSerialPort {
 public:
  LinuxSerialPort(std::string devicePath, int baud);
  ~LinuxSerialPort();

  LinuxSerialPort(const LinuxSerialPort&) = delete;
  LinuxSerialPort& operator=(const LinuxSerialPort&) = delete;

  bool open();
  void close();
  bool isOpen() const;

  int fd() const;
  int baud() const;
  const std::string& devicePath() const;

  bool drain();
  void flushRx();
  void flushRxTx();
  bool setModemFlag(int flag, bool enabled);

  bool waitWritable(const char* logPrefix,
                    size_t offset,
                    int maxFailures,
                    bool logErrno) const;
  bool writeAllRaw(const uint8_t* data,
                   size_t len,
                   const char* logPrefix,
                   int waitWritableFailures,
                   bool logErrno);
  ssize_t readSome(uint8_t* buffer,
                   size_t maxLen,
                   int timeoutMs,
                   const char* logPrefix,
                   bool logTimeout,
                   bool eintrReturnsTimeout,
                   bool logReadResult);

 private:
  static unsigned int baudToFlag(int baud);
  bool configureTermios();

  std::string devicePath_;
  int baud_ = 9600;
  int fd_ = -1;
};

}  // namespace Linux
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_LINUX_SERIAL_PORT_H_

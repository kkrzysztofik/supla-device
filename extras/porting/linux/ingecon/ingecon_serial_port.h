/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#ifndef EXTRAS_PORTING_LINUX_INGECON_INGECON_SERIAL_PORT_H_
#define EXTRAS_PORTING_LINUX_INGECON_INGECON_SERIAL_PORT_H_

#include <cstddef>
#include <cstdint>
#include <string>

namespace Supla {
namespace Linux {
namespace Ingecon {

class SerialPort {
 public:
  SerialPort(std::string devicePath, int baud);
  ~SerialPort();

  SerialPort(const SerialPort&) = delete;
  SerialPort& operator=(const SerialPort&) = delete;

  bool open();
  void close();
  bool isOpen() const;
  bool writeAll(const uint8_t* data, size_t len);
  ssize_t readSome(uint8_t* buffer, size_t maxLen, int timeoutMs);
  void flushRx();

 private:
  bool configureTermios();

  std::string devicePath_;
  int baud_ = 9600;
  int fd_ = -1;
};

}  // namespace Ingecon
}  // namespace Linux
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_INGECON_INGECON_SERIAL_PORT_H_

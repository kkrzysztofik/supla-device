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

#include <fcntl.h>
#include <supla/log_wrapper.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstring>
#include <string>
#include <thread>
#include <utility>

namespace Supla {
namespace Linux {
namespace Sma {

namespace {

speed_t baudToFlag(int baud) {
  switch (baud) {
    case 57600:
      return B57600;
    case 38400:
      return B38400;
    case 19200:
      return B19200;
    case 9600:
      return B9600;
    case 4800:
      return B4800;
    case 2400:
      return B2400;
    case 1200:
      return B1200;
    case 600:
      return B600;
    case 300:
      return B300;
    case 150:
      return B150;
    case 110:
      return B110;
    default:
      return B9600;
  }
}

bool modemStatusSet(int fd, int flag) {
  int status = 0;
  if (ioctl(fd, TIOCMGET, &status) < 0) {
    return false;
  }
  status |= flag;
  return ioctl(fd, TIOCMSET, &status) >= 0;
}

bool modemStatusClr(int fd, int flag) {
  int status = 0;
  if (ioctl(fd, TIOCMGET, &status) < 0) {
    return false;
  }
  status &= ~flag;
  return ioctl(fd, TIOCMSET, &status) >= 0;
}

void logDirectionControlError(int fd, const char* operation) {
  SUPLA_LOG_WARNING(
      "SmaBus: RS485 direction control failed fd=%d op=%s "
      "(errno=%d %s)",
      fd,
      operation,
      errno,
      std::strerror(errno));
}

bool waitWritable(int fd, const std::string& devicePath, size_t offset) {
  while (true) {
    fd_set writefds;
    FD_ZERO(&writefds);
    FD_SET(fd, &writefds);

    timeval tv{};
    tv.tv_sec = 1;
    const int ready = select(fd + 1, nullptr, &writefds, nullptr, &tv);
    if (ready > 0 && FD_ISSET(fd, &writefds)) {
      return true;
    }
    if (ready < 0 && errno == EINTR) {
      continue;
    }
    if (ready < 0) {
      SUPLA_LOG_WARNING(
          "SmaBus: wait for write %s failed at offset %zu (errno=%d %s)",
          devicePath.c_str(),
          offset,
          errno,
          std::strerror(errno));
    } else {
      SUPLA_LOG_WARNING("SmaBus: wait for write %s timed out at offset %zu",
                        devicePath.c_str(),
                        offset);
    }
    return false;
  }
}

}  // namespace

SmaSerialPort::SmaSerialPort(std::string devicePath,
                             int baud,
                             SerialMedia media)
    : devicePath_(std::move(devicePath)), baud_(baud), media_(media) {
}

SmaSerialPort::~SmaSerialPort() {
  close();
}

bool SmaSerialPort::open() {
  close();

  fd_ = ::open(devicePath_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd_ < 0) {
    SUPLA_LOG_WARNING("SmaBus: open %s failed (errno=%d %s)",
                      devicePath_.c_str(),
                      errno,
                      std::strerror(errno));
    return false;
  }

  if (!configureTermios()) {
    SUPLA_LOG_WARNING("SmaBus: termios config failed for %s (errno=%d %s)",
                      devicePath_.c_str(),
                      errno,
                      std::strerror(errno));
    close();
    return false;
  }

  int bytesInBuffer = 0;
  if (ioctl(fd_, FIONREAD, &bytesInBuffer) < 0) {
    close();
    return false;
  }

  SUPLA_LOG_DEBUG("SmaBus: opened %s @ %d baud (%s)",
                  devicePath_.c_str(),
                  baud_,
                  media_ == SerialMedia::RS485 ? "RS485" : "RS232");
  return true;
}

void SmaSerialPort::close() {
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
}

bool SmaSerialPort::isOpen() const {
  return fd_ >= 0;
}

bool SmaSerialPort::configureTermios() {
  termios options{};
  if (tcgetattr(fd_, &options) != 0) {
    return false;
  }

  options.c_iflag &=
      ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
  options.c_cflag |= (CLOCAL | CREAD);
  options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
  options.c_oflag &= ~OPOST;
  options.c_cc[VMIN] = 0;
  options.c_cc[VTIME] = 5;

  const speed_t rate = baudToFlag(baud_);
  // Set baud via c_cflag so the binary does not depend on GLIBC_2.42
  // cfsetispeed/cfsetospeed (C23); matches common Linux termios usage.
  options.c_cflag &= ~static_cast<tcflag_t>(CBAUD);
  options.c_cflag |= static_cast<tcflag_t>(rate);

  options.c_cflag &= ~PARENB;
  options.c_cflag &= ~CSTOPB;
  options.c_cflag &= ~CSIZE;
  options.c_cflag |= CS8;
  options.c_cflag &= ~CRTSCTS;

  return tcsetattr(fd_, TCSANOW, &options) == 0;
}

void SmaSerialPort::flushRx() {
  if (fd_ < 0) {
    return;
  }
  tcflush(fd_, TCIFLUSH);
}

void SmaSerialPort::waitBusFree() {
  // YASDI serial_wait_bus_free() arbitrates via DCD only on powerline media,
  // not RS232/RS485 (serial_posix.c).
}

bool SmaSerialPort::prepareSend() {
  if (media_ != SerialMedia::RS485 || fd_ < 0) {
    return true;
  }
  if (!modemStatusSet(fd_, TIOCM_RTS)) {
    logDirectionControlError(fd_, "set RTS");
    return false;
  }
  if (!modemStatusClr(fd_, TIOCM_DTR)) {
    logDirectionControlError(fd_, "clear DTR");
    return false;
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  return true;
}

bool SmaSerialPort::prepareRecv(bool skipDrain) {
  if (media_ != SerialMedia::RS485 || fd_ < 0) {
    return true;
  }
  if (!skipDrain && tcdrain(fd_) != 0) {
    logDirectionControlError(fd_, "tcdrain");
    return false;
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  if (!modemStatusClr(fd_, TIOCM_RTS)) {
    logDirectionControlError(fd_, "clear RTS");
    return false;
  }
  if (!modemStatusSet(fd_, TIOCM_DTR)) {
    logDirectionControlError(fd_, "set DTR");
    return false;
  }
  return true;
}

bool SmaSerialPort::writeAll(const uint8_t* data, size_t len) {
  if (fd_ < 0 || data == nullptr || len == 0) {
    return false;
  }

  waitBusFree();
  if (!prepareSend()) {
    return false;
  }

  size_t offset = 0;
  while (offset < len) {
    const ssize_t written = ::write(fd_, data + offset, len - offset);
    if (written < 0) {
      if (errno == EINTR) {
        continue;
      }
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        if (!waitWritable(fd_, devicePath_, offset)) {
          return false;
        }
        continue;
      }
      SUPLA_LOG_WARNING("SmaBus: write %s failed at offset %zu (errno=%d %s)",
                        devicePath_.c_str(),
                        offset,
                        errno,
                        std::strerror(errno));
      return false;
    }
    if (written == 0) {
      SUPLA_LOG_WARNING("SmaBus: write %s made no progress at offset %zu",
                        devicePath_.c_str(),
                        offset);
      return false;
    }
    offset += static_cast<size_t>(written);
  }

  if (tcdrain(fd_) != 0) {
    logDirectionControlError(fd_, "tcdrain after write");
    return false;
  }
  return prepareRecv(true);
}

ssize_t SmaSerialPort::readSome(uint8_t* buffer, size_t maxLen, int timeoutMs) {
  if (fd_ < 0 || buffer == nullptr || maxLen == 0) {
    return -1;
  }

  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET(fd_, &readfds);

  timeval tv{};
  tv.tv_sec = timeoutMs / 1000;
  tv.tv_usec = (timeoutMs % 1000) * 1000;

  const int ready = select(fd_ + 1, &readfds, nullptr, nullptr, &tv);
  if (ready < 0) {
    SUPLA_LOG_VERBOSE("SmaBus: select on %s failed (errno=%d %s)",
                      devicePath_.c_str(),
                      errno,
                      std::strerror(errno));
    return -1;
  }
  if (ready == 0) {
    return 0;
  }

  return ::read(fd_, buffer, maxLen);
}

}  // namespace Sma
}  // namespace Linux
}  // namespace Supla

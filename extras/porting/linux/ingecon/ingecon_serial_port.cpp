/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include "ingecon_serial_port.h"

#include <fcntl.h>
#include <supla/log_wrapper.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <string>
#include <utility>

namespace Supla {
namespace Linux {
namespace Ingecon {

namespace {

speed_t baudToFlag(int baud) {
  switch (baud) {
    case 115200:
      return B115200;
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
    default:
      return B9600;
  }
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
    SUPLA_LOG_WARNING("IngeconBus: wait for write %s failed at offset %zu",
                      devicePath.c_str(),
                      offset);
    return false;
  }
}

}  // namespace

SerialPort::SerialPort(std::string devicePath, int baud)
    : devicePath_(std::move(devicePath)), baud_(baud) {
}

SerialPort::~SerialPort() {
  close();
}

bool SerialPort::open() {
  close();
  fd_ = ::open(devicePath_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd_ < 0) {
    SUPLA_LOG_WARNING("IngeconBus: open %s failed (errno=%d %s)",
                      devicePath_.c_str(),
                      errno,
                      std::strerror(errno));
    return false;
  }
  if (!configureTermios()) {
    SUPLA_LOG_WARNING("IngeconBus: termios config failed for %s",
                      devicePath_.c_str());
    close();
    return false;
  }
  SUPLA_LOG_DEBUG("IngeconBus: opened %s @ %d baud",
                  devicePath_.c_str(),
                  baud_);
  return true;
}

void SerialPort::close() {
  if (fd_ >= 0) {
    SUPLA_LOG_DEBUG("IngeconBus: closing %s fd=%d",
                    devicePath_.c_str(),
                    fd_);
    ::close(fd_);
    fd_ = -1;
  }
}

bool SerialPort::isOpen() const {
  return fd_ >= 0;
}

bool SerialPort::configureTermios() {
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

  options.c_cflag &= ~static_cast<tcflag_t>(CBAUD);
  options.c_cflag |= static_cast<tcflag_t>(baudToFlag(baud_));
  options.c_cflag &= ~PARENB;
  options.c_cflag &= ~CSTOPB;
  options.c_cflag &= ~CSIZE;
  options.c_cflag |= CS8;
  options.c_cflag &= ~CRTSCTS;

  return tcsetattr(fd_, TCSANOW, &options) == 0;
}

bool SerialPort::writeAll(const uint8_t* data, size_t len) {
  if (fd_ < 0 || data == nullptr || len == 0) {
    SUPLA_LOG_WARNING("IngeconBus: invalid write request fd=%d len=%zu",
                      fd_,
                      len);
    return false;
  }

  SUPLA_LOG_VERBOSE("IngeconBus: serial write start %s len=%zu",
                    devicePath_.c_str(),
                    len);
  size_t offset = 0;
  while (offset < len) {
    const ssize_t written = ::write(fd_, data + offset, len - offset);
    if (written < 0) {
      if (errno == EINTR) {
        continue;
      }
      if ((errno == EAGAIN || errno == EWOULDBLOCK) &&
          waitWritable(fd_, devicePath_, offset)) {
        continue;
      }
      SUPLA_LOG_WARNING("IngeconBus: write %s failed at offset %zu",
                        devicePath_.c_str(),
                        offset);
      return false;
    }
    if (written == 0) {
      SUPLA_LOG_WARNING("IngeconBus: write %s made no progress at offset %zu",
                        devicePath_.c_str(),
                        offset);
      return false;
    }
    offset += static_cast<size_t>(written);
    SUPLA_LOG_VERBOSE("IngeconBus: serial wrote %zd bytes total=%zu/%zu",
                      written,
                      offset,
                      len);
  }

  if (tcdrain(fd_) != 0) {
    SUPLA_LOG_WARNING("IngeconBus: tcdrain %s failed (errno=%d %s)",
                      devicePath_.c_str(),
                      errno,
                      std::strerror(errno));
    return false;
  }
  SUPLA_LOG_VERBOSE("IngeconBus: serial write complete %s len=%zu",
                    devicePath_.c_str(),
                    len);
  return true;
}

ssize_t SerialPort::readSome(uint8_t* buffer, size_t maxLen, int timeoutMs) {
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
    if (errno == EINTR) {
      return 0;
    }
    SUPLA_LOG_VERBOSE("IngeconBus: select on %s failed (errno=%d %s)",
                      devicePath_.c_str(),
                      errno,
                      std::strerror(errno));
    return -1;
  }
  if (ready == 0) {
    SUPLA_LOG_VERBOSE("IngeconBus: read timeout on %s after %d ms",
                      devicePath_.c_str(),
                      timeoutMs);
    return 0;
  }

  const ssize_t read = ::read(fd_, buffer, maxLen);
  if (read < 0) {
    SUPLA_LOG_VERBOSE("IngeconBus: read %s failed (errno=%d %s)",
                      devicePath_.c_str(),
                      errno,
                      std::strerror(errno));
  } else {
    SUPLA_LOG_VERBOSE("IngeconBus: serial read %zd bytes from %s",
                      read,
                      devicePath_.c_str());
  }
  return read;
}

void SerialPort::flushRx() {
  if (fd_ >= 0) {
    SUPLA_LOG_VERBOSE("IngeconBus: flush RX %s", devicePath_.c_str());
    tcflush(fd_, TCIFLUSH);
  }
}

}  // namespace Ingecon
}  // namespace Linux
}  // namespace Supla

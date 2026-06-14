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

#include "linux_serial_port.h"

#include <fcntl.h>
#include <supla/log_wrapper.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <utility>

namespace Supla {
namespace Linux {

LinuxSerialPort::LinuxSerialPort(std::string devicePath, int baud)
    : devicePath_(std::move(devicePath)), baud_(baud) {
}

LinuxSerialPort::~LinuxSerialPort() {
  close();
}

bool LinuxSerialPort::open() {
  close();

  fd_ = ::open(devicePath_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd_ < 0) {
    return false;
  }

  if (!configureTermios()) {
    close();
    return false;
  }

  int bytesInBuffer = 0;
  if (ioctl(fd_, FIONREAD, &bytesInBuffer) < 0) {
    close();
    return false;
  }

  return true;
}

void LinuxSerialPort::close() {
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
}

bool LinuxSerialPort::isOpen() const {
  return fd_ >= 0;
}

int LinuxSerialPort::fd() const {
  return fd_;
}

int LinuxSerialPort::baud() const {
  return baud_;
}

const std::string& LinuxSerialPort::devicePath() const {
  return devicePath_;
}

bool LinuxSerialPort::drain() {
  return fd_ >= 0 && tcdrain(fd_) == 0;
}

void LinuxSerialPort::flushRx() {
  if (fd_ >= 0) {
    tcflush(fd_, TCIFLUSH);
  }
}

void LinuxSerialPort::flushRxTx() {
  if (fd_ >= 0) {
    tcflush(fd_, TCIOFLUSH);
  }
}

bool LinuxSerialPort::setModemFlag(int flag, bool enabled) {
  if (fd_ < 0) {
    return false;
  }

  int status = 0;
  if (ioctl(fd_, TIOCMGET, &status) < 0) {
    return false;
  }

  if (enabled) {
    status |= flag;
  } else {
    status &= ~flag;
  }
  return ioctl(fd_, TIOCMSET, &status) >= 0;
}

bool LinuxSerialPort::waitWritable(const char* logPrefix,
                                   size_t offset,
                                   int maxFailures,
                                   bool logErrno) const {
  int failures = 0;
  while (true) {
    fd_set writefds;
    FD_ZERO(&writefds);
    FD_SET(fd_, &writefds);

    timeval tv{};
    tv.tv_sec = 1;
    const int ready = select(fd_ + 1, nullptr, &writefds, nullptr, &tv);
    if (ready > 0 && FD_ISSET(fd_, &writefds)) {
      return true;
    }
    if (ready < 0 && errno == EINTR) {
      continue;
    }

    ++failures;
    if (failures < maxFailures) {
      continue;
    }

    if (logErrno && ready < 0) {
      SUPLA_LOG_WARNING(
          "%s: wait for write %s failed at offset %zu (errno=%d %s)",
          logPrefix,
          devicePath_.c_str(),
          offset,
          errno,
          std::strerror(errno));
    } else if (logErrno) {
      SUPLA_LOG_WARNING("%s: wait for write %s timed out at offset %zu",
                        logPrefix,
                        devicePath_.c_str(),
                        offset);
    } else {
      SUPLA_LOG_WARNING(
          "%s: wait for write %s failed after %d retries at offset %zu",
          logPrefix,
          devicePath_.c_str(),
          failures,
          offset);
    }
    return false;
  }
}

bool LinuxSerialPort::writeAllRaw(const uint8_t* data,
                                  size_t len,
                                  const char* logPrefix,
                                  int waitWritableFailures,
                                  bool logErrno) {
  if (fd_ < 0 || data == nullptr || len == 0) {
    return false;
  }

  size_t offset = 0;
  while (offset < len) {
    const ssize_t written = ::write(fd_, data + offset, len - offset);
    if (written < 0) {
      if (errno == EINTR) {
        continue;
      }
      if ((errno == EAGAIN || errno == EWOULDBLOCK) &&
          waitWritable(logPrefix, offset, waitWritableFailures, logErrno)) {
        continue;
      }
      if (logErrno) {
        SUPLA_LOG_WARNING(
            "%s: write %s failed at offset %zu (errno=%d %s)",
            logPrefix,
            devicePath_.c_str(),
            offset,
            errno,
            std::strerror(errno));
      } else {
        SUPLA_LOG_WARNING("%s: write %s failed at offset %zu",
                          logPrefix,
                          devicePath_.c_str(),
                          offset);
      }
      return false;
    }
    if (written == 0) {
      SUPLA_LOG_WARNING("%s: write %s made no progress at offset %zu",
                        logPrefix,
                        devicePath_.c_str(),
                        offset);
      return false;
    }
    offset += static_cast<size_t>(written);
  }

  return true;
}

ssize_t LinuxSerialPort::readSome(uint8_t* buffer,
                                  size_t maxLen,
                                  int timeoutMs,
                                  const char* logPrefix,
                                  bool logTimeout,
                                  bool eintrReturnsTimeout,
                                  bool logReadResult) {
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
    if (errno == EINTR && eintrReturnsTimeout) {
      return 0;
    }
    SUPLA_LOG_VERBOSE("%s: select on %s failed (errno=%d %s)",
                      logPrefix,
                      devicePath_.c_str(),
                      errno,
                      std::strerror(errno));
    return -1;
  }
  if (ready == 0) {
    if (logTimeout) {
      SUPLA_LOG_VERBOSE("%s: read timeout on %s after %d ms",
                        logPrefix,
                        devicePath_.c_str(),
                        timeoutMs);
    }
    return 0;
  }

  const ssize_t read = ::read(fd_, buffer, maxLen);
  if (logReadResult) {
    if (read < 0) {
      SUPLA_LOG_VERBOSE("%s: read %s failed (errno=%d %s)",
                        logPrefix,
                        devicePath_.c_str(),
                        errno,
                        std::strerror(errno));
    } else {
      SUPLA_LOG_VERBOSE("%s: serial read %zd bytes from %s",
                        logPrefix,
                        read,
                        devicePath_.c_str());
    }
  }
  return read;
}

unsigned int LinuxSerialPort::baudToFlag(int baud) {
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

bool LinuxSerialPort::configureTermios() {
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

  // Set baud via c_cflag so the binary does not depend on GLIBC_2.42
  // cfsetispeed/cfsetospeed (C23); matches common Linux termios usage.
  options.c_cflag &= ~static_cast<tcflag_t>(CBAUD);
  options.c_cflag |= static_cast<tcflag_t>(baudToFlag(baud_));
  options.c_cflag &= ~PARENB;
  options.c_cflag &= ~CSTOPB;
  options.c_cflag &= ~CSIZE;
  options.c_cflag |= CS8;
  options.c_cflag &= ~CRTSCTS;

  return tcsetattr(fd_, TCSANOW, &options) == 0;
}

}  // namespace Linux
}  // namespace Supla

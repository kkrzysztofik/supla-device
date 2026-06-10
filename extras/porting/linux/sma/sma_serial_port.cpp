/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include "sma_serial_port.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

#include <chrono>
#include <random>
#include <thread>

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

bool isDcdSet(int fd) {
  int status = 0;
  if (ioctl(fd, TIOCMGET, &status) < 0) {
    return false;
  }
  return (status & TIOCM_CD) != 0;
}

}  // namespace

SmaSerialPort::SmaSerialPort(std::string devicePath, int baud, SerialMedia media)
    : devicePath_(std::move(devicePath)), baud_(baud), media_(media) {}

SmaSerialPort::~SmaSerialPort() {
  close();
}

bool SmaSerialPort::open() {
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
  cfsetispeed(&options, rate);
  cfsetospeed(&options, rate);

  options.c_cflag &= ~PARENB;
  options.c_cflag &= ~CSTOPB;
  options.c_cflag &= ~CSIZE;
  options.c_cflag |= CS8;
  options.c_cflag &= ~CRTSCTS;

  return tcsetattr(fd_, TCSANOW, &options) == 0;
}

void SmaSerialPort::waitBusFree() {
  if (media_ != SerialMedia::RS485 || fd_ < 0) {
    return;
  }

  int waitedMs = 0;
  while (isDcdSet(fd_)) {
    if (waitedMs > 1005) {
      return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    waitedMs += 5;
  }

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<int> dist(0, 7);
  std::this_thread::sleep_for(
      std::chrono::milliseconds(85 + dist(gen) * 5));
}

void SmaSerialPort::prepareSend() {
  if (media_ != SerialMedia::RS485 || fd_ < 0) {
    return;
  }
  modemStatusSet(fd_, TIOCM_RTS);
  modemStatusClr(fd_, TIOCM_DTR);
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
}

void SmaSerialPort::prepareRecv() {
  if (media_ != SerialMedia::RS485 || fd_ < 0) {
    return;
  }
  tcdrain(fd_);
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  modemStatusClr(fd_, TIOCM_RTS);
  modemStatusSet(fd_, TIOCM_DTR);
}

bool SmaSerialPort::writeAll(const uint8_t* data, size_t len) {
  if (fd_ < 0 || data == nullptr || len == 0) {
    return false;
  }

  waitBusFree();
  prepareSend();

  size_t offset = 0;
  while (offset < len) {
    const ssize_t written = ::write(fd_, data + offset, len - offset);
    if (written < 0) {
      if (errno == EINTR) {
        continue;
      }
      return false;
    }
    offset += static_cast<size_t>(written);
  }

  tcdrain(fd_);
  prepareRecv();
  return true;
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

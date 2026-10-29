/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include "socomec_countis.h"

namespace Supla {
namespace Socomec {

namespace {
bool crcOk(const uint8_t *frame, size_t len) {
  const uint16_t crc = crc16(frame, len - 2);
  return frame[len - 2] == (crc & 0xFF) && frame[len - 1] == (crc >> 8);
}
}  // namespace

uint16_t crc16(const uint8_t *data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; bit++) {
      crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
    }
  }
  return crc;
}

void buildReadHolding(uint8_t slave,
                      uint16_t address,
                      uint16_t count,
                      uint8_t out[8]) {
  out[0] = slave;
  out[1] = 0x03;
  out[2] = address >> 8;
  out[3] = address & 0xFF;
  out[4] = count >> 8;
  out[5] = count & 0xFF;
  const uint16_t crc = crc16(out, 6);
  out[6] = crc & 0xFF;
  out[7] = crc >> 8;
}

ParseResult parseReadHolding(const uint8_t *frame,
                             size_t len,
                             uint8_t slave,
                             uint16_t count,
                             uint16_t *registers) {
  if (len < 5) {
    return ParseResult::Incomplete;
  }
  if (frame[0] != slave) {
    return ParseResult::Bad;
  }
  if (frame[1] == 0x83) {
    return crcOk(frame, 5) ? ParseResult::Exception : ParseResult::Bad;
  }
  if (frame[1] != 0x03 || frame[2] != 2 * count) {
    return ParseResult::Bad;
  }
  const size_t expected = 5 + 2 * static_cast<size_t>(count);
  if (len < expected) {
    return ParseResult::Incomplete;
  }
  if (!crcOk(frame, expected)) {
    return ParseResult::Bad;
  }
  for (uint16_t i = 0; i < count; i++) {
    registers[i] = (frame[3 + 2 * i] << 8) | frame[4 + 2 * i];
  }
  return ParseResult::Ok;
}

}  // namespace Socomec
}  // namespace Supla

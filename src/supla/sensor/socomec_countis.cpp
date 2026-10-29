/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include "socomec_countis.h"

#include <math.h>
#include <string.h>

#include <supla/log_wrapper.h>

namespace Supla {
namespace Socomec {

namespace {
bool crcOk(const uint8_t *frame, size_t len) {
  const uint16_t crc = crc16(frame, len - 2);
  return frame[len - 2] == (crc & 0xFF) && frame[len - 1] == (crc >> 8);
}

uint32_t u32(const uint16_t *r, int i) {
  return (static_cast<uint32_t>(r[i]) << 16) | r[i + 1];
}

int32_t s32(const uint16_t *r, int i) {
  return static_cast<int32_t>(u32(r, i));
}

constexpr int64_t kPowerScale = 1000000;  // raw LSB 10 W -> 0.00001 W
constexpr double kDeciDegreesPerRadian = 1800.0 / 3.14159265358979323846;
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

void decodeMeasurements(Model model, const uint16_t *r, Measurements *out) {
  const uint32_t freq = u32(r, 6);
  // Hardware-verified: E03 LSB 0.01 Hz, E23 LSB 0.001 Hz.
  out->freq = static_cast<uint16_t>(model == Model::E23 ? freq / 10 : freq);
  for (int i = 0; i < 3; i++) {
    Phase &p = out->phase[i];
    p = Phase();
    const uint32_t voltage = u32(r, 2 * i);
    if ((model == Model::E03 && i > 0) || voltage == kAbsent32) {
      continue;
    }
    const int32_t active = s32(r, 24 + 2 * i);
    const int32_t reactive = s32(r, 30 + 2 * i);
    p.present = true;
    p.voltage = static_cast<uint16_t>(voltage);
    p.current = u32(r, 8 + 2 * i);
    p.powerActive = active * kPowerScale;
    p.powerReactive = reactive * kPowerScale;
    p.powerApparent = static_cast<int64_t>(u32(r, 36 + 2 * i)) * kPowerScale;
    p.powerFactor = s32(r, 42 + 2 * i);
    p.phaseAngle = static_cast<int32_t>(
        lround(atan2(reactive, active) * kDeciDegreesPerRadian));
  }
}

bool decodeLineEnergy(const uint16_t *r, int phase, Energy *out) {
  for (int kind = 0; kind < EnergyKinds; kind++) {
    const uint32_t main = u32(r, kind * 3);     // 1 kWh LSB
    const uint16_t residual = r[kind * 3 + 2];  // 0.1 Wh LSB, < 1 kWh
    if (main == kAbsent32 || residual > 9999) {
      return false;
    }
    out->value[kind][phase] = main * 100000ULL + residual * 10ULL;
  }
  return true;
}

bool decodeTotals(const uint16_t *r, uint64_t totals[EnergyKinds]) {
  // 50946 Ea+, 50948 Er+, 50950 Eap, 50952 Ea-, 50954 Er-.
  const int offset[EnergyKinds] = {0, 6, 2, 8};
  for (int kind = 0; kind < EnergyKinds; kind++) {
    const uint32_t value = u32(r, offset[kind]);  // 0.01 kWh LSB
    if (value == kAbsent32) {
      return false;
    }
    totals[kind] = value * 1000ULL;
  }
  return true;
}

bool energyMatchesTotals(const Energy &lines,
                         const uint64_t totals[EnergyKinds],
                         int phases) {
  const uint64_t gross = totals[FwdAct] + totals[RvrAct];
  const uint64_t relative = gross / kCrossCheckDivisor;
  const uint64_t tolerance =
      relative > kCrossCheckFloor ? relative : kCrossCheckFloor;
  // Only active energy: see the comment on kCrossCheckDivisor.
  const EnergyKind checked[] = {FwdAct, RvrAct};
  for (EnergyKind kind : checked) {
    uint64_t sum = 0;
    for (int p = 0; p < phases; p++) {
      sum += lines.value[kind][p];
    }
    const uint64_t diff =
        sum > totals[kind] ? sum - totals[kind] : totals[kind] - sum;
    if (diff > tolerance) {
      return false;
    }
  }
  return true;
}

bool identityMatches(Model model, const uint16_t *regs, size_t count) {
  char name[2 * kIdentityCount + 1] = {};
  for (size_t i = 0; i < count && i < kIdentityCount; i++) {
    name[2 * i] = static_cast<char>(regs[i] >> 8);
    name[2 * i + 1] = static_cast<char>(regs[i] & 0xFF);
  }
  return strstr(name, model == Model::E23 ? "E23" : "E03") != nullptr;
}


uint64_t EnergyGuard::maxDelta(uint32_t elapsedMs) const {
  // W * ms / 36000 = 0.01 Wh
  return static_cast<uint64_t>(maxPowerW) * elapsedMs / 36000 + kSlack;
}

bool EnergyGuard::accept(uint64_t value, uint32_t nowMs) {
  if (!hasBaseline || (value >= last &&
                       value - last <= maxDelta(nowMs - lastMs))) {
    hasBaseline = true;
    last = value;
    lastMs = nowMs;
    candidateCount = 0;
    return true;
  }
  // Out of range: only a value that keeps growing plausibly on its own for
  // kReanchorCycles in a row replaces the baseline (meter swap/reset).
  const bool sameCandidate =
      candidateCount > 0 && value >= candidate &&
      value - candidate <= maxDelta(nowMs - candidateMs);
  candidateCount = sameCandidate ? candidateCount + 1 : 1;
  candidate = value;
  candidateMs = nowMs;
  if (candidateCount < kReanchorCycles) {
    return false;
  }
  SUPLA_LOG_WARNING("Socomec: energy counter re-anchored %llu -> %llu",
                    static_cast<unsigned long long>(last),  // NOLINT(runtime/int)
                    static_cast<unsigned long long>(value));  // NOLINT(runtime/int)
  last = value;
  lastMs = nowMs;
  candidateCount = 0;
  return true;
}

uint32_t defaultMaxPowerW(Model model) {
  // Rated current x 230 V x phases x 1.5 margin. Calibration knob:
  // setMaxPowerKw() / max_power_kw -- set it from the installation's breaker.
  return model == Model::E23 ? 82800 : 40000;
}


}  // namespace Socomec

}  // namespace Supla

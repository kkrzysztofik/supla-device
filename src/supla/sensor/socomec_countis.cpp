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
#include <supla/time.h>

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
  // The meter derives the line-block residual from a coarser internal
  // accumulator, so a counter's lowest digits jitter in both directions by a
  // few 0.1 Wh counts (observed on E23 firmware 3.00). A backward step within
  // kSlack is noise, not a meter reset.
  const bool forward =
      hasBaseline && value >= last && value - last <= maxDelta(nowMs - lastMs);
  const bool noise = hasBaseline && value < last && last - value <= kSlack;
  if (!hasBaseline || forward || noise) {
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

namespace {
struct Request {
  uint16_t address;
  uint16_t count;
};
}  // namespace

namespace Sensor {

using Socomec::EnergyKinds;
using Socomec::Model;
using Socomec::ParseResult;

namespace {
Request requestFor(uint8_t step) {
  switch (step) {
    case 0: return {Socomec::kIdentityAddr, Socomec::kIdentityCount};
    case 1: return {Socomec::kMeasurementsAddr, Socomec::kMeasurementsCount};
    case 2:
    case 3:
    case 4:
      return {Socomec::kLineEnergyAddr[step - 2], Socomec::kLineEnergyCount};
    case 5: return {Socomec::kTotalsAddr, Socomec::kTotalsCount};
    case 6: return {Socomec::kPhaseSequenceAddr, 1};
    default: return {0, 0};
  }
}
}  // namespace

SocomecCountis::SocomecCountis(Socomec::Transport *transport,
                               Socomec::Model model,
                               uint8_t slave)
    : transport(transport), model(model), slave(slave) {
  setMaxPowerKw(Socomec::defaultMaxPowerW(model) / 1000);
}

void SocomecCountis::setMaxPowerKw(uint32_t kw) {
  for (auto &kind : guard) {
    for (auto &g : kind) {
      g.setMaxPowerW(kw * 1000);
    }
  }
}

int SocomecCountis::phaseCount() const {
  return model == Model::E23 ? 3 : 1;
}

bool SocomecCountis::logAllowed(uint32_t now) {
  if (loggedOnce && now - lastLogMs < kLogIntervalMs) {
    return false;
  }
  loggedOnce = true;
  lastLogMs = now;
  return true;
}

void SocomecCountis::iterateAlways() {
  pollBus(millis());
  ElectricityMeter::iterateAlways();
}

void SocomecCountis::pollBus(uint32_t now) {
  switch (state) {
    case State::Disabled:
      return;
    case State::Idle:
      if (cycleStarted && now - cycleMs < refreshRateSec * 1000U) {
        return;
      }
      cycleStarted = true;
      cycleMs = now;
      step = identityOk ? Step::Measurements : Step::Identity;
      linesOk = true;
      totalsOk = false;
      phaseSequence = -1;
      startRequest(now);
      return;
    case State::Gap:
      if (now - stateMs >= kGapMs) {
        startRequest(now);
      }
      return;
    case State::Wait:
      break;
  }

  const int n = transport->readAvailable(rx + rxLen, sizeof(rx) - rxLen);
  if (n < 0) {
    onFailure(now);
    return;
  }
  rxLen += n;
  const auto request = requestFor(static_cast<uint8_t>(step));
  switch (Socomec::parseReadHolding(rx, rxLen, slave, request.count, regs)) {
    case ParseResult::Ok:
      onReply(now);
      return;
    case ParseResult::Exception:
      onException(now);
      return;
    case ParseResult::Bad:
      transport->flushRx();
      onFailure(now);
      return;
    case ParseResult::Incomplete:
      if (now - stateMs > kReplyTimeoutMs) {
        onFailure(now);
      }
      return;
  }
}

void SocomecCountis::startRequest(uint32_t now) {
  const auto request = requestFor(static_cast<uint8_t>(step));
  uint8_t frame[8];
  Socomec::buildReadHolding(slave, request.address, request.count, frame);
  transport->flushRx();
  rxLen = 0;
  state = State::Wait;
  stateMs = now;
  if (!transport->write(frame, sizeof(frame))) {
    onFailure(now);
  }
}

void SocomecCountis::onReply(uint32_t now) {
  switch (step) {
    case Step::Identity:
      if (!Socomec::identityMatches(model, regs, Socomec::kIdentityCount)) {
        SUPLA_LOG_ERROR("Socomec[%d]: meter is not a COUNTIS %s, polling "
                        "disabled", getChannelNumber(),
                        model == Model::E23 ? "E23" : "E03");
        state = State::Disabled;
        return;
      }
      identityOk = true;
      break;
    case Step::Measurements:
      Socomec::decodeMeasurements(model, regs, &measurements);
      break;
    case Step::Line1:
    case Step::Line2:
    case Step::Line3: {
      const int phase =
          static_cast<int>(step) - static_cast<int>(Step::Line1);
      linesOk = Socomec::decodeLineEnergy(regs, phase, &lineEnergy) && linesOk;
      break;
    }
    case Step::Totals:
      totalsOk = Socomec::decodeTotals(regs, totals);
      break;
    case Step::PhaseSequence:
      phaseSequence = regs[0] <= 1 ? static_cast<int8_t>(regs[0]) : -1;
      break;
    case Step::Done:
      break;
  }
  nextStep(now);
}

void SocomecCountis::onException(uint32_t now) {
  if (step == Step::Line1 && model == Model::E03) {
    SUPLA_LOG_INFO("Socomec[%d]: no line-1 energy block, using totals",
                   getChannelNumber());
    lineBlocks = false;
    nextStep(now);
    return;
  }
  if (step == Step::PhaseSequence) {
    nextStep(now);
    return;
  }
  if (logAllowed(now)) {
    SUPLA_LOG_WARNING("Socomec[%d]: exception reading register %u",
                      getChannelNumber(),
                      requestFor(static_cast<uint8_t>(step)).address);
  }
  retried = true;  // an exception will not change on retry
  onFailure(now);
}

void SocomecCountis::onFailure(uint32_t now) {
  if (!retried) {
    retried = true;
    state = State::Gap;
    stateMs = now;
    return;
  }
  retried = false;
  state = State::Idle;
  if (failedCycles < kOfflineAfterFailedCycles &&
      ++failedCycles == kOfflineAfterFailedCycles) {
    SUPLA_LOG_WARNING("Socomec[%d]: meter not answering, marked offline",
                      getChannelNumber());
    offline = true;
  }
}

bool SocomecCountis::skipStep(Step s) const {
  switch (s) {
    case Step::Line1:
      return !lineBlocks;
    case Step::Line2:
    case Step::Line3:
    case Step::PhaseSequence:
      return model == Model::E03;
    default:
      return false;
  }
}

void SocomecCountis::nextStep(uint32_t now) {
  retried = false;
  do {
    step = static_cast<Step>(static_cast<uint8_t>(step) + 1);
  } while (step != Step::Done && skipStep(step));
  if (step == Step::Done) {
    finishCycle(now);
    return;
  }
  state = State::Gap;
  stateMs = now;
}

void SocomecCountis::finishCycle(uint32_t now) {
  state = State::Idle;
  failedCycles = 0;
  offline = false;
  published = measurements;
  publishedSequence = phaseSequence;
  havePublished = true;

  const int phases = phaseCount();
  Socomec::Energy energy;
  bool consistent = totalsOk;
  if (lineBlocks) {
    energy = lineEnergy;
    consistent = consistent && linesOk &&
                 Socomec::energyMatchesTotals(energy, totals, phases);
  } else {
    for (int kind = 0; kind < EnergyKinds; kind++) {
      energy.value[kind][0] = totals[kind];
    }
  }
  if (!consistent) {
    if (logAllowed(now)) {
      SUPLA_LOG_WARNING("Socomec[%d]: energy registers inconsistent, "
                        "keeping previous values", getChannelNumber());
    }
    return;
  }
  bool accepted = true;
  for (int kind = 0; kind < EnergyKinds; kind++) {
    for (int p = 0; p < phases; p++) {
      accepted = guard[kind][p].accept(energy.value[kind][p], now) && accepted;
    }
  }
  if (!accepted) {
    if (logAllowed(now)) {
      SUPLA_LOG_WARNING("Socomec[%d]: implausible energy jump rejected "
                        "(Ea+ L1 %llu)", getChannelNumber(),
                        static_cast<unsigned long long>(  // NOLINT(runtime/int)
                            energy.value[Socomec::FwdAct][0]));
    }
    return;
  }
  publishedEnergy = energy;
  haveEnergy = true;
}

void SocomecCountis::readValuesFromDevice() {
  if (offline) {
    resetReadParameters();
    return;
  }
  if (!havePublished) {
    return;
  }
  for (int i = 0; i < phaseCount(); i++) {
    const auto &p = published.phase[i];
    if (!p.present) {
      resetReadParametersForPhase(i);
      continue;
    }
    setVoltage(i, p.voltage);
    setCurrent(i, p.current);
    setPowerActive(i, p.powerActive);
    setPowerReactive(i, p.powerReactive);
    setPowerApparent(i, p.powerApparent);
    setPowerFactor(i, p.powerFactor);
    setPhaseAngle(i, p.phaseAngle);
    if (haveEnergy) {
      setFwdActEnergy(i, publishedEnergy.value[Socomec::FwdAct][i]);
      setRvrActEnergy(i, publishedEnergy.value[Socomec::RvrAct][i]);
      setFwdReactEnergy(i, publishedEnergy.value[Socomec::FwdReact][i]);
      setRvrReactEnergy(i, publishedEnergy.value[Socomec::RvrReact][i]);
    }
  }
  setFreq(published.freq);
  if (model == Model::E23) {
    if (publishedSequence < 0) {
      clearVoltagePhaseSequenceFlag();
    } else {
      setVoltagePhaseSequence(publishedSequence == 0);
    }
  }
}

}  // namespace Sensor
}  // namespace Supla

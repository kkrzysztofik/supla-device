/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#ifndef SRC_SUPLA_SENSOR_SOCOMEC_COUNTIS_H_
#define SRC_SUPLA_SENSOR_SOCOMEC_COUNTIS_H_

#include <stddef.h>
#include <stdint.h>

#include "electricity_meter.h"

namespace Supla {
namespace Socomec {

enum class Model : uint8_t { E03, E23 };

// Byte pipe to one RS-485 bus. Every call must return immediately.
class Transport {
 public:
  virtual ~Transport() = default;
  virtual bool write(const uint8_t *data, size_t len) = 0;
  // Bytes copied (0 = nothing yet), or -1 on a port error.
  virtual int readAvailable(uint8_t *buffer, size_t maxLen) = 0;
  virtual void flushRx() = 0;
};

enum class ParseResult : uint8_t { Incomplete, Ok, Exception, Bad };

uint16_t crc16(const uint8_t *data, size_t len);
void buildReadHolding(uint8_t slave,
                      uint16_t address,
                      uint16_t count,
                      uint8_t out[8]);
// Parses the bytes received so far for an FC03 request of `count` registers.
ParseResult parseReadHolding(const uint8_t *frame,
                             size_t len,
                             uint8_t slave,
                             uint16_t count,
                             uint16_t *registers);

constexpr uint32_t kAbsent32 = 0xFFFFFFFF;
constexpr uint16_t kIdentityAddr = 50058;  // extended name, e.g. "Countis E23"
constexpr uint16_t kIdentityCount = 8;
constexpr uint16_t kMeasurementsAddr = 50520;
constexpr uint16_t kMeasurementsCount = 48;  // 50520..50567
constexpr uint16_t kLineEnergyAddr[3] = {0x4D83, 0x5583, 0x5D83};
constexpr uint16_t kLineEnergyCount = 12;
constexpr uint16_t kTotalsAddr = 50946;  // 0.01 kWh totals
constexpr uint16_t kTotalsCount = 10;
constexpr uint16_t kPhaseSequenceAddr = 51315;
constexpr size_t kMaxFrame = 5 + 2 * kMeasurementsCount;

// Sum of per-line energy vs the 50946 totals. The 2026-10-29 probe showed that
// per-line active energy tracks the system totals to ~0.015 %, while the
// per-direction reactive counters do not reproduce the system reactive totals
// (the meter defines them differently). So only active energy is cross-checked,
// with a relative tolerance that absorbs the installation-history offset; the
// floor covers a nearly empty counter. See
// docs/integrations/socomec-countis-probe-2026-10-29.md.
constexpr uint64_t kCrossCheckFloor = 3000;   // 30 Wh, in 0.00001 kWh
constexpr uint64_t kCrossCheckDivisor = 100;  // 1 % of the active totals

struct Phase {
  bool present = false;
  uint16_t voltage = 0;       // 0.01 V
  uint32_t current = 0;       // 0.001 A
  int64_t powerActive = 0;    // 0.00001 W
  int64_t powerReactive = 0;  // 0.00001 var
  int64_t powerApparent = 0;  // 0.00001 VA
  int32_t powerFactor = 0;    // 0.001
  int32_t phaseAngle = 0;     // 0.1 degree
};

struct Measurements {
  uint16_t freq = 0;  // 0.01 Hz
  Phase phase[3];
};

// Order matches the per-line block: Ea+, Ea-, Er+, Er-.
enum EnergyKind : uint8_t { FwdAct = 0, RvrAct, FwdReact, RvrReact, EnergyKinds };

struct Energy {
  uint64_t value[EnergyKinds][3] = {};  // 0.00001 kWh, [kind][phase]
};

// regs: kMeasurementsCount registers from kMeasurementsAddr.
void decodeMeasurements(Model model, const uint16_t *regs, Measurements *out);
// regs: kLineEnergyCount registers from kLineEnergyAddr[phase].
bool decodeLineEnergy(const uint16_t *regs, int phase, Energy *out);
// regs: kTotalsCount registers from kTotalsAddr.
bool decodeTotals(const uint16_t *regs, uint64_t totals[EnergyKinds]);
bool energyMatchesTotals(const Energy &lines,
                         const uint64_t totals[EnergyKinds],
                         int phases);
bool identityMatches(Model model, const uint16_t *regs, size_t count);

}  // namespace Socomec
}  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_SOCOMEC_COUNTIS_H_

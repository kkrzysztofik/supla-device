# Socomec COUNTIS E03/E23 Driver Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Publish Socomec COUNTIS E03 and E23 meters as SUPLA `ElectricityMeter` channels from one portable driver, for sd4linux (RPi 5) and ESP-IDF (ESP32).

**Architecture:** One portable file pair, `src/supla/sensor/socomec_countis.{h,cpp}`, holds the Modbus RTU FC03 framing, pure register decoders, a per-counter `EnergyGuard` and the `SocomecCountis` class. The class runs a non-blocking state machine inside `iterateAlways()` over a 3-method `Transport`. Platform code is only the transport: `LinuxSerialPort` for Linux and the UART RS485 half-duplex driver for ESP-IDF, plus the Linux YAML factory.

**Tech Stack:** C++17 (no STL in `src/` — AVR compiles it), gtest/gmock (`extras/test`), yaml-cpp (Linux factory), ESP-IDF `driver/uart.h`.

**Design:** [2026-10-29-socomec-countis-design.md](2026-10-29-socomec-countis-design.md). Register facts: [socomec-countis-modbus.md](../integrations/socomec-countis-modbus.md).

**Deliberate refinements of the design (found while planning):**
* `Transport*` is a raw, non-owning pointer. `src/` uses no `std::unique_ptr` because AVR Arduino builds every file in `src/`. The Linux factory allocates the transport for the life of the process, like every SUPLA element.
* The cross-check (Σ lines vs the 10 Wh totals) gates **every** cycle, not just the baseline: a mismatch skips the energy update for that cycle. Because of that, `EnergyGuard::accept()` needs no cross-check flag, and a bad baseline heals itself through re-anchoring. The 50770/50776 kWh read is dropped; the 10 Wh block is the independent source.
* An exception on a line block is fatal for that cycle on the E23. On the E03 it switches permanently to totals-as-phase-1. An exception on the phase-sequence register is ignored, which leaves the flag cleared.
* No new systemd unit. `extras/examples/linux/supla-device-linux.service` is already generic (`-c /etc/supla-device.yaml`).
* ESP-IDF is not installed on the dev host, so the ESP32 transport is **compile-verified only where `idf.py` is available** (Task 9). Say so in the final report.

**Conventions:**
* Build and run unit tests from the worktree root:
  `cmake -S extras/test -B extras/test/build -DCMAKE_POLICY_VERSION_MINIMUM=3.5 && cmake --build extras/test/build -j"$(nproc)" --target supladevicetests sd4linuxtests`
  then `extras/test/build/supladevicetests --gtest_filter='Socomec*'`.
* Units for SUPLA: voltage 0.01 V, current 0.001 A, freq 0.01 Hz, power 0.00001 W/var/VA, PF 0.001, angle 0.1°, energy 0.00001 kWh (= 0.01 Wh).
* License header for new files: same as `extras/porting/linux/ingecon/*.h` (Copyright Krzysztof Krzysztofik, GPL-2.0-or-later).
* Commit after each task. Each commit message ends with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

---

### Task 1: Probe the meters on `pi-szn` (hardware, before any C++)

This step can invalidate the decode tables, so it runs first. It needs SSH to `root@192.168.2.120`. If the Pi is unreachable, stop and ask the user. Do not guess.

**Files:**
- Create: `docs/integrations/socomec-countis-probe-2026-10-29.md`

**Step 1: Copy the probe tool**

```bash
scp extras/tools/modbus_probe.py root@192.168.2.120:/root/
```

**Step 2: Dump the blocks (both meters at 9600 8N1, address 2)**

```bash
E03=/dev/serial/by-id/usb-FTDI_FT232R_USB_UART_BG03FMMW-if00-port0
E23=/dev/serial/by-id/usb-FTDI_FT232R_USB_UART_BG041JVI-if00-port0
ssh root@192.168.2.120 "
for P in $E03 $E23; do
  echo \"== \$P\"
  python3 /root/modbus_probe.py read \$P 9600 N 2 3 19843 12   # L1 energy (0x4D83)
  python3 /root/modbus_probe.py read \$P 9600 N 2 3 21891 12   # L2 energy (0x5583)
  python3 /root/modbus_probe.py read \$P 9600 N 2 3 23939 12   # L3 energy (0x5D83)
  python3 /root/modbus_probe.py read \$P 9600 N 2 3 50946 10   # 10 Wh totals
  python3 /root/modbus_probe.py read \$P 9600 N 2 3 50770 8    # kWh totals
  python3 /root/modbus_probe.py read \$P 9600 N 2 3 50058 8    # extended name
  python3 /root/modbus_probe.py read \$P 9600 N 2 3 51315 1    # phase sequence
  sleep 0.2
done"
```

**Step 3: Check the assumptions and write them down**

For each meter, record raw words and decoded values in the probe doc, and answer:

1. Does the E03 answer 0x4D83, or does it return an exception? (This decides whether the E03 uses lines or totals.)
2. Is every residual in `0…9999`?
3. For each counter, does `Σ lines (main·1000 + res·0.1 Wh)` equal the 10 Wh total within 30 Wh?
4. Does the 10 Wh Ea+ equal the kWh Ea+ within 1 kWh?
5. Which string does the extended name hold, and does it contain `E03` or `E23`?

**If 2, 3 or 4 fails: stop.** The decode rule in Task 4 is wrong; report the raw dumps to the user.

**Step 4: Commit**

```bash
git add docs/integrations/socomec-countis-probe-2026-10-29.md
git commit -m "docs(socomec): raw energy-block probe of both meters"
```

---

### Task 2: Wire the empty module into both builds

**Files:**
- Create: `src/supla/sensor/socomec_countis.h`, `src/supla/sensor/socomec_countis.cpp`
- Create: `extras/test/SocomecTests/socomec_protocol_tests.cpp`
- Modify: `cmake/SuplaDeviceSources.cmake:214` (after `electricity_meter.cpp`)
- Modify: `extras/test/CMakeLists.txt` (`TEST_SRC` glob, after `IngeconTests/*.cpp`)

**Step 1: Header skeleton**

```cpp
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

}  // namespace Socomec
}  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_SOCOMEC_COUNTIS_H_
```

`.cpp`: the license header plus `#include "socomec_countis.h"`.

**Step 2: Register sources**

In `cmake/SuplaDeviceSources.cmake`, after the `electricity_meter.cpp` line:

```cmake
  ${SUPLA_DEVICE_SRC_DIR}/supla/sensor/socomec_countis.cpp
```

In `extras/test/CMakeLists.txt`, add to the `TEST_SRC` glob after `IngeconTests/*.cpp`:

```cmake
  SocomecTests/*.cpp
```

**Step 3: Smoke test**

`extras/test/SocomecTests/socomec_protocol_tests.cpp`:

```cpp
#include <gtest/gtest.h>
#include <supla/sensor/socomec_countis.h>

#include <vector>

using Supla::Socomec::Model;
```

**Step 4: Build**

Run the build command from Conventions.
Expected: builds; `supladevicetests --gtest_filter='Socomec*'` reports 0 tests and exits 0.

**Step 5: Commit**

```bash
git add src/supla/sensor/socomec_countis.* cmake/SuplaDeviceSources.cmake \
  extras/test/CMakeLists.txt extras/test/SocomecTests
git commit -m "feat(socomec): add COUNTIS driver module skeleton"
```

---

### Task 3: FC03 framing — CRC, request, response parse

**Files:**
- Modify: `src/supla/sensor/socomec_countis.h`, `.cpp`
- Test: `extras/test/SocomecTests/socomec_protocol_tests.cpp`

**Step 1: Failing tests** (append)

```cpp
using Supla::Socomec::ParseResult;

namespace {
std::vector<uint8_t> withCrc(std::vector<uint8_t> frame) {
  const uint16_t crc = Supla::Socomec::crc16(frame.data(), frame.size());
  frame.push_back(crc & 0xFF);
  frame.push_back(crc >> 8);
  return frame;
}
}  // namespace

TEST(SocomecProtocolTests, BuildsReadHoldingRequest) {
  uint8_t frame[8];
  Supla::Socomec::buildReadHolding(2, 50520, 48, frame);
  // 50520 = 0xC558, 48 = 0x0030; CRC computed by modbus_probe.py frame_of()
  const auto expected = withCrc({0x02, 0x03, 0xC5, 0x58, 0x00, 0x30});
  EXPECT_EQ(std::vector<uint8_t>(frame, frame + 8), expected);
}

TEST(SocomecProtocolTests, KnownCrcVector) {
  // Standard Modbus example: 01 03 00 00 00 0A -> CRC C5 CD
  const uint8_t req[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};
  EXPECT_EQ(Supla::Socomec::crc16(req, sizeof(req)), 0xCDC5);
}

TEST(SocomecProtocolTests, ParsesOkReply) {
  const auto frame = withCrc({0x02, 0x03, 0x04, 0x00, 0x00, 0x5D, 0x53});
  uint16_t regs[2] = {};
  EXPECT_EQ(Supla::Socomec::parseReadHolding(frame.data(), frame.size(), 2, 2,
                                             regs),
            ParseResult::Ok);
  EXPECT_EQ(regs[0], 0x0000);
  EXPECT_EQ(regs[1], 0x5D53);  // 23891 = 238.91 V
}

TEST(SocomecProtocolTests, PartialReplyIsIncomplete) {
  const auto frame = withCrc({0x02, 0x03, 0x04, 0x00, 0x00, 0x5D, 0x53});
  uint16_t regs[2] = {};
  EXPECT_EQ(Supla::Socomec::parseReadHolding(frame.data(), 4, 2, 2, regs),
            ParseResult::Incomplete);
  EXPECT_EQ(Supla::Socomec::parseReadHolding(frame.data(), 6, 2, 2, regs),
            ParseResult::Incomplete);
}

TEST(SocomecProtocolTests, ParsesExceptionReply) {
  const auto frame = withCrc({0x02, 0x83, 0x02});
  uint16_t regs[2] = {};
  EXPECT_EQ(Supla::Socomec::parseReadHolding(frame.data(), frame.size(), 2, 2,
                                             regs),
            ParseResult::Exception);
}

TEST(SocomecProtocolTests, RejectsBadCrcWrongSlaveAndWrongCount) {
  auto frame = withCrc({0x02, 0x03, 0x04, 0x00, 0x00, 0x5D, 0x53});
  uint16_t regs[2] = {};
  auto bad = frame;
  bad.back() ^= 0xFF;
  EXPECT_EQ(Supla::Socomec::parseReadHolding(bad.data(), bad.size(), 2, 2,
                                             regs),
            ParseResult::Bad);
  EXPECT_EQ(Supla::Socomec::parseReadHolding(frame.data(), frame.size(), 3, 2,
                                             regs),
            ParseResult::Bad);
  EXPECT_EQ(Supla::Socomec::parseReadHolding(frame.data(), frame.size(), 2, 3,
                                             regs),
            ParseResult::Bad);
}
```

**Step 2: Run — expect a compile failure** (`crc16` not declared).

**Step 3: Implement**

Header, inside `namespace Socomec`:

```cpp
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
```

`.cpp`:

```cpp
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
```

**Step 4: Run** `supladevicetests --gtest_filter='SocomecProtocolTests.*'`. Expected: 6 PASS.

**Step 5: Commit** `feat(socomec): Modbus RTU FC03 framing`

---

### Task 4: Register decoders (measurements, energy, identity)

**Files:** same as Task 3.

**Step 1: Failing tests** (append). Fixtures are the 2026-10-29 `pi-szn` readings from the commissioning report §6. **If Task 1 produced real dumps, replace the synthetic energy words with them.**

```cpp
using Supla::Socomec::Energy;
using Supla::Socomec::Measurements;

namespace {
constexpr uint32_t kNa = 0xFFFFFFFF;

void put32(std::vector<uint16_t> *regs, int offset, uint32_t value) {
  (*regs)[offset] = value >> 16;
  (*regs)[offset + 1] = value & 0xFFFF;
}

// 50520..50567, E03 values from the commissioning report.
std::vector<uint16_t> e03Measurements() {
  std::vector<uint16_t> r(48, 0xFFFF);
  put32(&r, 0, 23891);                         // V1 238.91 V
  put32(&r, 6, 5000);                          // F 50.00 Hz (E03: 0.01 Hz LSB)
  put32(&r, 8, 2417);                          // I1 2.417 A
  put32(&r, 16, 47);                           // P 470 W
  put32(&r, 18, static_cast<uint32_t>(-33));   // Q -330 var
  put32(&r, 20, 60);                           // S 600 VA
  put32(&r, 22, static_cast<uint32_t>(-773));  // PF
  put32(&r, 24, 47);                           // P1
  put32(&r, 30, static_cast<uint32_t>(-33));   // Q1
  put32(&r, 36, 60);                           // S1
  put32(&r, 42, static_cast<uint32_t>(-773));  // PF1
  return r;
}

std::vector<uint16_t> e23Measurements() {
  std::vector<uint16_t> r(48, 0);
  put32(&r, 0, 24009);
  put32(&r, 2, 24097);
  put32(&r, 4, 23977);
  put32(&r, 6, 50030);  // E23: 0.001 Hz LSB
  put32(&r, 8, 2744);
  put32(&r, 10, 434);
  put32(&r, 12, 464);
  return r;
}

// One line block (B+3..B+14): Ea+, Ea-, Er+, Er- as main kWh + residual 0.1 Wh.
std::vector<uint16_t> lineBlock(uint32_t eaP, uint16_t rEaP, uint32_t eaN,
                                uint16_t rEaN, uint32_t erP, uint16_t rErP,
                                uint32_t erN, uint16_t rErN) {
  std::vector<uint16_t> r(12, 0);
  put32(&r, 0, eaP);
  r[2] = rEaP;
  put32(&r, 3, eaN);
  r[5] = rEaN;
  put32(&r, 6, erP);
  r[8] = rErP;
  put32(&r, 9, erN);
  r[11] = rErN;
  return r;
}

// 50946..50955 in 0.01 kWh: Ea+, Er+, Eap, Ea-, Er-.
std::vector<uint16_t> totalsBlock(uint32_t eaP, uint32_t erP, uint32_t eaN,
                                  uint32_t erN) {
  std::vector<uint16_t> r(10, 0);
  put32(&r, 0, eaP);
  put32(&r, 2, erP);
  put32(&r, 4, 0);
  put32(&r, 6, eaN);
  put32(&r, 8, erN);
  return r;
}
}  // namespace

TEST(SocomecDecodeTests, E03MeasurementsUsePhaseOneOnly) {
  Measurements m;
  Supla::Socomec::decodeMeasurements(Model::E03, e03Measurements().data(), &m);
  EXPECT_EQ(m.freq, 5000);
  ASSERT_TRUE(m.phase[0].present);
  EXPECT_EQ(m.phase[0].voltage, 23891);
  EXPECT_EQ(m.phase[0].current, 2417u);
  EXPECT_EQ(m.phase[0].powerActive, 47000000);     // 470 W in 0.00001 W
  EXPECT_EQ(m.phase[0].powerReactive, -33000000);
  EXPECT_EQ(m.phase[0].powerApparent, 60000000);
  EXPECT_EQ(m.phase[0].powerFactor, -773);
  EXPECT_EQ(m.phase[0].phaseAngle, -351);          // atan2(-330, 470) = -35.07°
  EXPECT_FALSE(m.phase[1].present);
  EXPECT_FALSE(m.phase[2].present);
}

TEST(SocomecDecodeTests, E23FrequencyHasFinerLsb) {
  Measurements m;
  Supla::Socomec::decodeMeasurements(Model::E23, e23Measurements().data(), &m);
  EXPECT_EQ(m.freq, 5003);  // 50.030 Hz -> 0.01 Hz
  EXPECT_TRUE(m.phase[2].present);
  EXPECT_EQ(m.phase[1].voltage, 24097);
  EXPECT_EQ(m.phase[2].current, 464u);
}

TEST(SocomecDecodeTests, E23AbsentPhaseIsSkipped) {
  auto r = e23Measurements();
  put32(&r, 4, kNa);  // V3
  Measurements m;
  Supla::Socomec::decodeMeasurements(Model::E23, r.data(), &m);
  EXPECT_FALSE(m.phase[2].present);
}

TEST(SocomecDecodeTests, LineEnergyCombinesMainAndResidual) {
  Energy e;
  const auto r = lineBlock(4846, 1234, 5507, 0, 10, 9999, 0, 5);
  ASSERT_TRUE(Supla::Socomec::decodeLineEnergy(r.data(), 0, &e));
  EXPECT_EQ(e.value[Supla::Socomec::FwdAct][0], 484612340ULL);  // 4846.1234 kWh
  EXPECT_EQ(e.value[Supla::Socomec::RvrAct][0], 550700000ULL);
  EXPECT_EQ(e.value[Supla::Socomec::FwdReact][0], 1099990ULL);
  EXPECT_EQ(e.value[Supla::Socomec::RvrReact][0], 50ULL);
}

TEST(SocomecDecodeTests, LineEnergyRejectsBadResidualAndAbsent) {
  Energy e;
  auto r = lineBlock(4846, 10000, 0, 0, 0, 0, 0, 0);
  EXPECT_FALSE(Supla::Socomec::decodeLineEnergy(r.data(), 0, &e));
  r = lineBlock(kNa, 0, 0, 0, 0, 0, 0, 0);
  EXPECT_FALSE(Supla::Socomec::decodeLineEnergy(r.data(), 0, &e));
}

TEST(SocomecDecodeTests, TotalsAndCrossCheck) {
  uint64_t totals[Supla::Socomec::EnergyKinds];
  const auto t = totalsBlock(484612, 1099, 550700, 0);
  ASSERT_TRUE(Supla::Socomec::decodeTotals(t.data(), totals));
  EXPECT_EQ(totals[Supla::Socomec::FwdAct], 484612000ULL);
  EXPECT_EQ(totals[Supla::Socomec::RvrAct], 550700000ULL);
  EXPECT_EQ(totals[Supla::Socomec::FwdReact], 1099000ULL);

  Energy e;
  const auto l = lineBlock(4846, 1234, 5507, 0, 10, 9999, 0, 5);
  ASSERT_TRUE(Supla::Socomec::decodeLineEnergy(l.data(), 0, &e));
  EXPECT_TRUE(Supla::Socomec::energyMatchesTotals(e, totals, 1));

  totals[Supla::Socomec::FwdAct] += 5000;  // 50 Wh off
  EXPECT_FALSE(Supla::Socomec::energyMatchesTotals(e, totals, 1));
}

TEST(SocomecDecodeTests, IdentityMatchesModel) {
  // "Countis E23" packed big-endian, NUL padded, 8 registers
  const uint16_t e23[8] = {0x436F, 0x756E, 0x7469, 0x7320,
                           0x4532, 0x3300, 0x0000, 0x0000};
  EXPECT_TRUE(Supla::Socomec::identityMatches(Model::E23, e23, 8));
  EXPECT_FALSE(Supla::Socomec::identityMatches(Model::E03, e23, 8));
}
```

**Step 2: Run — expect a compile failure.**

**Step 3: Implement**

Header, in `namespace Socomec`:

```cpp
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
// Σ lines vs 10 Wh totals: totals truncate to 10 Wh, reads are ~100 ms apart.
constexpr uint64_t kCrossCheckTolerance = 3000;  // 30 Wh

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
```

`.cpp` — add `#include <math.h>` and `#include <string.h>`. Inside `namespace Socomec`:

```cpp
namespace {
uint32_t u32(const uint16_t *r, int i) {
  return (static_cast<uint32_t>(r[i]) << 16) | r[i + 1];
}

int32_t s32(const uint16_t *r, int i) {
  return static_cast<int32_t>(u32(r, i));
}

constexpr int64_t kPowerScale = 1000000;  // raw LSB 10 W -> 0.00001 W
constexpr double kDeciDegreesPerRadian = 1800.0 / 3.14159265358979323846;
}  // namespace

void decodeMeasurements(Model model, const uint16_t *r, Measurements *out) {
  const uint32_t freq = u32(r, 6);
  // Hardware-verified: E03 LSB 0.01 Hz, E23 LSB 0.001 Hz (report §7.1).
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
    const uint32_t main = u32(r, kind * 3);  // 1 kWh LSB
    const uint16_t residual = r[kind * 3 + 2];  // 0.1 Wh LSB, < 1 kWh
    if (main == kAbsent32 || residual > 9999) {
      return false;
    }
    out->value[kind][phase] = main * 100000ULL + residual * 10ULL;
  }
  return true;
}

bool decodeTotals(const uint16_t *r, uint64_t totals[EnergyKinds]) {
  // 50946 Ea+, 50948 Er+, 50950 Eap, 50952 Ea-, 50954 Er-
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
  for (int kind = 0; kind < EnergyKinds; kind++) {
    uint64_t sum = 0;
    for (int p = 0; p < phases; p++) {
      sum += lines.value[kind][p];
    }
    const uint64_t diff =
        sum > totals[kind] ? sum - totals[kind] : totals[kind] - sum;
    if (diff > kCrossCheckTolerance) {
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
```

**Step 4: Run** `--gtest_filter='SocomecDecodeTests.*'`. Expected: 7 PASS. If `phaseAngle` comes out as −352, check the rounding; `atan2(-330, 470)` = −35.075° → −351.

**Step 5: Commit** `feat(socomec): decode COUNTIS measurement and energy registers`

---

### Task 5: `EnergyGuard`

**Files:** same.

**Step 1: Failing tests** (append)

```cpp
using Supla::Socomec::EnergyGuard;

namespace {
constexpr uint64_t kWh = 100000;  // 1 kWh in 0.00001 kWh
}

TEST(SocomecEnergyGuardTests, FirstValueBecomesBaseline) {
  EnergyGuard g;
  EXPECT_TRUE(g.accept(1 * kWh, 0));
  EXPECT_EQ(g.value(), 1 * kWh);
}

TEST(SocomecEnergyGuardTests, SpikeIsRejectedAndRealValueStillAccepted) {
  // The user's case: 1 kWh -> 2 000 000 kWh -> 1.2 kWh, 5 s polls.
  EnergyGuard g;
  g.setMaxPowerW(83000);
  ASSERT_TRUE(g.accept(1 * kWh, 0));
  EXPECT_FALSE(g.accept(2000000 * kWh, 5000));
  EXPECT_EQ(g.value(), 1 * kWh);
  EXPECT_TRUE(g.accept(120000, 10000));  // 0.2 kWh in 10 s = 72 kW < 83 kW
  EXPECT_EQ(g.value(), 120000u);
}

TEST(SocomecEnergyGuardTests, TighterMaxPowerRejectsImplausibleStep) {
  EnergyGuard g;
  g.setMaxPowerW(17000);  // 25 A breaker
  ASSERT_TRUE(g.accept(1 * kWh, 0));
  EXPECT_FALSE(g.accept(120000, 10000));
}

TEST(SocomecEnergyGuardTests, BackwardsIsRejected) {
  EnergyGuard g;
  ASSERT_TRUE(g.accept(10 * kWh, 0));
  EXPECT_FALSE(g.accept(9 * kWh, 5000));
  EXPECT_EQ(g.value(), 10 * kWh);
}

TEST(SocomecEnergyGuardTests, NormalGrowthAccepted) {
  EnergyGuard g;
  ASSERT_TRUE(g.accept(10 * kWh, 0));
  EXPECT_TRUE(g.accept(10 * kWh + 65, 5000));  // 470 W for 5 s = 0.65 Wh
}

TEST(SocomecEnergyGuardTests, ReanchorsAfterStableNewValue) {
  // Meter swapped: the counter restarts near 0 and stays consistent.
  EnergyGuard g;
  ASSERT_TRUE(g.accept(20744 * kWh, 0));
  uint32_t now = 0;
  for (int i = 1; i < EnergyGuard::kReanchorCycles; i++) {
    now += 5000;
    EXPECT_FALSE(g.accept(5 * kWh, now)) << i;
  }
  now += 5000;
  EXPECT_TRUE(g.accept(5 * kWh, now));
  EXPECT_EQ(g.value(), 5 * kWh);
}

TEST(SocomecEnergyGuardTests, UnstableGarbageNeverReanchors) {
  EnergyGuard g;
  ASSERT_TRUE(g.accept(1 * kWh, 0));
  uint32_t now = 0;
  for (int i = 0; i < 50; i++) {
    now += 5000;
    const uint64_t garbage = (i % 2) ? 0 : 3000000 * kWh;
    EXPECT_FALSE(g.accept(garbage, now)) << i;
  }
  EXPECT_EQ(g.value(), 1 * kWh);
}
```

**Step 2: Run — expect a compile failure.**

**Step 3: Implement**

Header:

```cpp
// Plausibility filter for one monotonic energy counter (0.00001 kWh units).
class EnergyGuard {
 public:
  static constexpr uint8_t kReanchorCycles = 12;  // ~1 min at 5 s polls
  static constexpr uint64_t kSlack = 1000;        // 10 Wh = one totals LSB

  void setMaxPowerW(uint32_t watts) { maxPowerW = watts; }
  bool accept(uint64_t value, uint32_t nowMs);
  uint64_t value() const { return last; }

 private:
  uint64_t maxDelta(uint32_t elapsedMs) const;

  uint32_t maxPowerW = 83000;
  bool hasBaseline = false;
  uint64_t last = 0;
  uint32_t lastMs = 0;
  uint64_t candidate = 0;
  uint32_t candidateMs = 0;
  uint8_t candidateCount = 0;
};

uint32_t defaultMaxPowerW(Model model);
```

`.cpp`:

```cpp
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
  // setMaxPowerKw() / max_power_kw — set it from the installation's breaker.
  return model == Model::E23 ? 82800 : 40000;
}
```

Add `#include <supla/log_wrapper.h>` to the `.cpp`.

**Step 4: Run** `--gtest_filter='SocomecEnergyGuardTests.*'`. Expected: 7 PASS. In `ReanchorsAfterStableNewValue`, a constant 5 kWh counts as "same candidate" (Δ = 0).

**Step 5: Commit** `feat(socomec): energy plausibility guard`

---

### Task 6: `SocomecCountis` — the non-blocking state machine

**Files:**
- Modify: `src/supla/sensor/socomec_countis.h`, `.cpp`
- Create: `extras/test/SocomecTests/socomec_countis_tests.cpp`

**Step 1: Failing tests**

```cpp
#include <gtest/gtest.h>
#include <simple_time.h>
#include <supla/sensor/socomec_countis.h>

#include <algorithm>
#include <map>
#include <set>
#include <vector>

using Supla::Sensor::SocomecCountis;
using Supla::Socomec::Model;

namespace {

void put32(std::vector<uint16_t> *regs, int offset, uint32_t value) {
  (*regs)[offset] = value >> 16;
  (*regs)[offset + 1] = value & 0xFFFF;
}

// Answers FC03 requests from a register map, like a meter on the bus.
class FakeMeter : public Supla::Socomec::Transport {
 public:
  bool write(const uint8_t *data, size_t len) override {
    EXPECT_EQ(len, 8u);
    const uint16_t address = (data[2] << 8) | data[3];
    const uint16_t count = (data[4] << 8) | data[5];
    requests.push_back(address);
    if (dead || dropNext > 0) {
      if (dropNext > 0) {
        dropNext--;
      }
      return true;
    }
    std::vector<uint8_t> f = {data[0]};
    if (exceptions.count(address)) {
      f.insert(f.end(), {0x83, 0x02});
    } else {
      const auto &regs = blocks.at(address);
      f.insert(f.end(), {0x03, static_cast<uint8_t>(count * 2)});
      for (uint16_t i = 0; i < count; i++) {
        f.push_back(regs[i] >> 8);
        f.push_back(regs[i] & 0xFF);
      }
    }
    const uint16_t crc = Supla::Socomec::crc16(f.data(), f.size());
    f.push_back(crc & 0xFF);
    f.push_back(crc >> 8);
    rx.insert(rx.end(), f.begin(), f.end());
    return true;
  }

  int readAvailable(uint8_t *buffer, size_t maxLen) override {
    // Deliver at most 7 bytes per call: exercises partial frames.
    const size_t n = std::min({maxLen, rx.size(), static_cast<size_t>(7)});
    std::copy(rx.begin(), rx.begin() + n, buffer);
    rx.erase(rx.begin(), rx.begin() + n);
    return static_cast<int>(n);
  }

  void flushRx() override {
    rx.clear();
  }

  std::map<uint16_t, std::vector<uint16_t>> blocks;
  std::set<uint16_t> exceptions;
  std::vector<uint16_t> requests;
  std::vector<uint8_t> rx;
  int dropNext = 0;
  bool dead = false;
};

std::vector<uint16_t> name(const char *text) {
  std::vector<uint16_t> r(8, 0);
  for (int i = 0; text[i] && i < 16; i++) {
    r[i / 2] |= (i % 2) ? static_cast<uint8_t>(text[i])
                        : static_cast<uint8_t>(text[i]) << 8;
  }
  return r;
}

std::vector<uint16_t> lineBlock(uint32_t eaP, uint16_t rEaP) {
  std::vector<uint16_t> r(12, 0);
  put32(&r, 0, eaP);
  r[2] = rEaP;
  return r;
}

std::vector<uint16_t> totals(uint32_t eaP) {
  std::vector<uint16_t> r(10, 0);
  put32(&r, 0, eaP);
  return r;
}

void loadE03(FakeMeter *m) {
  std::vector<uint16_t> meas(48, 0xFFFF);
  put32(&meas, 0, 23891);
  put32(&meas, 6, 5000);
  put32(&meas, 8, 2417);
  put32(&meas, 24, 47);
  put32(&meas, 30, 0);
  put32(&meas, 36, 60);
  put32(&meas, 42, static_cast<uint32_t>(-773));
  m->blocks[50058] = name("Countis E03");
  m->blocks[50520] = meas;
  m->blocks[0x4D83] = lineBlock(4846, 1234);
  m->blocks[50946] = totals(484612);
}

void loadE23(FakeMeter *m) {
  std::vector<uint16_t> meas(48, 0);
  for (int i = 0; i < 3; i++) {
    put32(&meas, 2 * i, 24000 + i);
    put32(&meas, 8 + 2 * i, 1000);
  }
  put32(&meas, 6, 50030);
  m->blocks[50058] = name("Countis E23");
  m->blocks[50520] = meas;
  m->blocks[0x4D83] = lineBlock(7000, 0);
  m->blocks[0x5583] = lineBlock(7000, 0);
  m->blocks[0x5D83] = lineBlock(6744, 0);
  m->blocks[50946] = totals(2074400);  // 20744 kWh
  m->blocks[51315] = {0};
}

class SocomecCountisTests : public ::testing::Test {
 protected:
  void SetUp() override {
    time.advance(1);
  }

  // Calls iterateAlways() every 10 ms of simulated time.
  void run(SocomecCountis *em, int ms) {
    for (int t = 0; t < ms; t += 10) {
      em->iterateAlways();
      time.advance(10);
    }
  }

  SimpleTime time;
  FakeMeter meter;
};

}  // namespace

TEST_F(SocomecCountisTests, E03FullCyclePublishesPhaseOne) {
  loadE03(&meter);
  SocomecCountis em(&meter, Model::E03, 2);
  run(&em, 6000);  // first cycle + one base publish at 5 s
  EXPECT_EQ(em.getVoltage(0), 23891);
  EXPECT_EQ(em.getFreq(), 5000);
  EXPECT_EQ(em.getCurrent(0), 2417u);
  EXPECT_EQ(em.getPowerActive(0), 47000000);
  EXPECT_EQ(em.getPowerFactor(0), -773);
  EXPECT_EQ(em.getFwdActEnergy(0), 484612340ULL);
  EXPECT_EQ(em.getVoltage(1), 0);
  // E03 never asks for lines 2/3 or the phase sequence.
  EXPECT_EQ(std::count(meter.requests.begin(), meter.requests.end(), 0x5583),
            0);
  EXPECT_EQ(std::count(meter.requests.begin(), meter.requests.end(), 51315),
            0);
}

TEST_F(SocomecCountisTests, E23PublishesThreePhasesAndSequence) {
  loadE23(&meter);
  SocomecCountis em(&meter, Model::E23, 2);
  run(&em, 6000);
  EXPECT_EQ(em.getFreq(), 5003);
  EXPECT_EQ(em.getVoltage(2), 24002);
  EXPECT_EQ(em.getFwdActEnergy(0), 700000000ULL);
  EXPECT_EQ(em.getFwdActEnergy(2), 674400000ULL);
}

TEST_F(SocomecCountisTests, E03WithoutLineBlockFallsBackToTotals) {
  loadE03(&meter);
  meter.exceptions.insert(0x4D83);
  SocomecCountis em(&meter, Model::E03, 2);
  run(&em, 12000);  // two cycles
  EXPECT_EQ(em.getFwdActEnergy(0), 484612000ULL);
  // Asked once, then never again.
  EXPECT_EQ(std::count(meter.requests.begin(), meter.requests.end(), 0x4D83),
            1);
}

TEST_F(SocomecCountisTests, SingleTimeoutIsRetried) {
  loadE03(&meter);
  meter.dropNext = 1;  // first request (identity) times out once
  SocomecCountis em(&meter, Model::E03, 2);
  run(&em, 6000);
  EXPECT_EQ(em.getVoltage(0), 23891);
}

TEST_F(SocomecCountisTests, IdentityMismatchDisablesPolling) {
  loadE23(&meter);
  SocomecCountis em(&meter, Model::E03, 2);  // configured wrong
  run(&em, 12000);
  EXPECT_EQ(em.getVoltage(0), 0);
  EXPECT_EQ(meter.requests.size(), 1u);
}

TEST_F(SocomecCountisTests, DeadMeterGoesOfflineAfterThreeCycles) {
  loadE03(&meter);
  SocomecCountis em(&meter, Model::E03, 2);
  run(&em, 6000);
  ASSERT_EQ(em.getVoltage(0), 23891);
  meter.dead = true;
  run(&em, 25000);  // >= 3 failed cycles + a base publish
  EXPECT_EQ(em.getVoltage(0), 0);
  meter.dead = false;
  run(&em, 11000);  // recovers without restart
  EXPECT_EQ(em.getVoltage(0), 23891);
}

TEST_F(SocomecCountisTests, CrossCheckMismatchKeepsPreviousEnergy) {
  loadE23(&meter);
  SocomecCountis em(&meter, Model::E23, 2);
  run(&em, 6000);
  ASSERT_EQ(em.getFwdActEnergy(0), 700000000ULL);
  meter.blocks[0x4D83] = lineBlock(9000, 0);  // L1 garbage, totals unchanged
  run(&em, 10000);
  EXPECT_EQ(em.getFwdActEnergy(0), 700000000ULL);
}

TEST_F(SocomecCountisTests, IterateNeverWaitsOnTheBus) {
  // A blocking implementation would spin on millis(), which never advances
  // here, and hang this test.
  meter.dead = true;
  SocomecCountis em(&meter, Model::E23, 2);
  for (int i = 0; i < 1000; i++) {
    em.iterateAlways();
  }
  EXPECT_EQ(meter.requests.size(), 1u);
}
```

**Step 2: Run — expect a compile failure** (`SocomecCountis` not declared).

**Step 3: Implement**

Header, after `namespace Socomec`:

```cpp
namespace Sensor {

class SocomecCountis : public ElectricityMeter {
 public:
  // transport is not owned and must outlive this object.
  SocomecCountis(Socomec::Transport *transport,
                 Socomec::Model model,
                 uint8_t slave);

  void setMaxPowerKw(uint32_t kw);
  void iterateAlways() override;
  void readValuesFromDevice() override;

 private:
  enum class Step : uint8_t {
    Identity,
    Measurements,
    Line1,
    Line2,
    Line3,
    Totals,
    PhaseSequence,
    Done
  };
  enum class State : uint8_t { Idle, Gap, Wait, Disabled };

  static constexpr uint32_t kReplyTimeoutMs = 300;
  static constexpr uint32_t kGapMs = 50;  // report finding #4
  static constexpr uint8_t kOfflineAfterFailedCycles = 3;
  static constexpr uint32_t kLogIntervalMs = 60000;

  void pollBus(uint32_t now);
  void startRequest(uint32_t now);
  void onReply(uint32_t now);
  void onException(uint32_t now);
  void onFailure(uint32_t now);
  void nextStep(uint32_t now);
  void finishCycle(uint32_t now);
  bool skipStep(Step s) const;
  bool logAllowed(uint32_t now);
  int phaseCount() const;

  Socomec::Transport *transport = nullptr;
  Socomec::Model model;
  uint8_t slave = 0;

  State state = State::Idle;
  Step step = Step::Identity;
  bool identityOk = false;
  bool lineBlocks = true;  // false after an E03 exception on line 1
  bool retried = false;
  bool cycleStarted = false;
  uint8_t failedCycles = 0;
  uint32_t stateMs = 0;
  uint32_t cycleMs = 0;
  uint32_t lastLogMs = 0;
  bool loggedOnce = false;
  size_t rxLen = 0;
  uint8_t rx[Socomec::kMaxFrame] = {};
  uint16_t regs[Socomec::kMeasurementsCount] = {};

  // Current cycle
  Socomec::Measurements measurements;
  Socomec::Energy lineEnergy;
  uint64_t totals[Socomec::EnergyKinds] = {};
  bool linesOk = false;
  bool totalsOk = false;
  int8_t phaseSequence = -1;  // -1 unknown, 0 clockwise, 1 counter-clockwise

  // Last complete cycle
  bool offline = false;
  bool havePublished = false;
  bool haveEnergy = false;
  Socomec::Measurements published;
  int8_t publishedSequence = -1;
  Socomec::Energy publishedEnergy;
  Socomec::EnergyGuard guard[Socomec::EnergyKinds][3];
};

}  // namespace Sensor
```

`.cpp` — add `#include <supla/time.h>`. After `namespace Socomec`:

```cpp
namespace {
struct Request {
  uint16_t address;
  uint16_t count;
};
}  // namespace

namespace Supla {
namespace Sensor {

using Socomec::EnergyKinds;
using Socomec::Model;
using Socomec::ParseResult;

namespace {
Request requestFor(uint8_t step) {
  switch (step) {
    case 0: return {Socomec::kIdentityAddr, Socomec::kIdentityCount};
    case 1: return {Socomec::kMeasurementsAddr, Socomec::kMeasurementsCount};
    case 2: case 3: case 4:
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
```

Note: `requestFor` takes the `Step` value as `uint8_t`, so the enum order **must** stay Identity, Measurements, Line1..3, Totals, PhaseSequence.

**Step 4: Run** `--gtest_filter='Socomec*'`. Expected: all PASS (6 + 7 + 7 + 8).
If `DeadMeterGoesOfflineAfterThreeCycles` fails: one failed cycle costs about 0.7 s (2 × 300 ms timeout + gap) plus the 5 s refresh gap, so 25 s covers 3 cycles plus a publish. Check the timing before you change any thresholds.

**Step 5: Commit** `feat(socomec): non-blocking COUNTIS ElectricityMeter`

---

### Task 7: Linux extension (transport + YAML factory)

**Files:**
- Create: `extras/porting/linux/extensions/socomec/socomec_extension.cpp`
- Create: `extras/porting/linux/extensions/socomec/supla_linux_extension.cmake`
- Create: `extras/test/LinuxPortTests/sd4linux_socomec_extension_tests.cpp`
- Modify: `extras/test/CMakeLists.txt` (`sd4linuxtests` sources: add the extension `.cpp` after `byd_extension.cpp`)

**Step 1: Failing test**

```cpp
#include <gtest/gtest.h>

#include "linux_channel_factory.h"

namespace Supla {
namespace Linux {
void initSocomecExtension();
}  // namespace Linux
}  // namespace Supla

TEST(Sd4linuxSocomecExtensionTests, RegistersSocomecCountisFactory) {
  auto &registry = Supla::Linux::ChannelFactoryRegistry::instance();
  registry.clear();
  Supla::Linux::initSocomecExtension();
  const auto *factory = registry.findByType("SocomecCountis");
  ASSERT_NE(factory, nullptr);
  EXPECT_EQ(factory->pluginName, "socomec");
  registry.clear();
}
```

**Step 2: Run** `sd4linuxtests --gtest_filter='Sd4linuxSocomec*'`. Expect a link failure.

**Step 3: Implement** `socomec_extension.cpp`:

```cpp
/* license header */

#include <supla/log_wrapper.h>
#include <supla/sensor/socomec_countis.h>

#include <memory>
#include <string>
#include <utility>

#include "linux_channel_factory.h"
#include "linux_extension_config_helpers.h"
#include "linux_serial_port.h"
#include "linux_yaml_config.h"

namespace {

class LinuxSocomecTransport : public Supla::Socomec::Transport {
 public:
  LinuxSocomecTransport(std::string devicePath, int baud)
      : port(std::move(devicePath), baud) {
  }

  bool write(const uint8_t *data, size_t len) override {
    if (!port.isOpen() && !port.open()) {
      return false;
    }
    if (!port.writeAllRaw(data, len, "Socomec", 3, false)) {
      port.close();  // reopened on the next write, e.g. after a USB re-plug
      return false;
    }
    return true;
  }

  int readAvailable(uint8_t *buffer, size_t maxLen) override {
    if (!port.isOpen()) {
      return -1;
    }
    const ssize_t n =
        port.readSome(buffer, maxLen, 0, "Socomec", false, true, false);
    if (n < 0) {
      port.close();
    }
    return static_cast<int>(n);
  }

  void flushRx() override {
    if (port.isOpen()) {
      port.flushRx();
    }
  }

 private:
  Supla::Linux::LinuxSerialPort port;
};

bool addSocomecCountis(const Supla::Linux::ChannelFactoryContext &context) {
  const auto &ch = context.channel;
  auto &config = context.config;

  Supla::Linux::ExtensionSerialConfig serial;
  if (!Supla::Linux::parseExtensionSerialConfig(context, &serial)) {
    return false;
  }
  if (!ch["device"] || !ch["device"]["model"] ||
      !ch["device"]["modbus_address"]) {
    SUPLA_LOG_ERROR("Channel[%d] config: SocomecCountis needs device.model "
                    "and device.modbus_address", context.channelNumber);
    return false;
  }

  config.markChannelParameterUsed();
  const auto modelName = ch["device"]["model"].as<std::string>();
  Supla::Socomec::Model model;
  if (modelName == "e03") {
    model = Supla::Socomec::Model::E03;
  } else if (modelName == "e23") {
    model = Supla::Socomec::Model::E23;
  } else {
    SUPLA_LOG_ERROR("Channel[%d] config: invalid Socomec model \"%s\" "
                    "(e03 | e23)", context.channelNumber, modelName.c_str());
    return false;
  }

  config.markChannelParameterUsed();
  const int address = ch["device"]["modbus_address"].as<int>();
  if (address < 1 || address > 247) {
    SUPLA_LOG_ERROR("Channel[%d] config: modbus_address out of range",
                    context.channelNumber);
    return false;
  }

  int pollIntervalSec = 5;
  if (ch["poll_interval_sec"]) {
    Supla::Linux::parseExtensionPollIntervalSec(context, &pollIntervalSec);
  }

  SUPLA_LOG_INFO("Channel[%d] config: adding SocomecCountis %s on %s, "
                 "address=%d", context.channelNumber, modelName.c_str(),
                 serial.devicePath.c_str(), address);

  // ponytail: transport lives for the process, like every SUPLA element.
  auto *transport = new LinuxSocomecTransport(serial.devicePath, serial.baud);
  std::unique_ptr<Supla::Sensor::SocomecCountis> meter(
      new Supla::Sensor::SocomecCountis(
          transport, model, static_cast<uint8_t>(address)));
  meter->setRefreshRate(pollIntervalSec);
  if (ch["max_power_kw"]) {
    config.markChannelParameterUsed();
    meter->setMaxPowerKw(ch["max_power_kw"].as<unsigned>());
  }
  return Supla::Linux::addConfiguredChannel(context, std::move(meter));
}

}  // namespace

namespace Supla {
namespace Linux {

void initSocomecExtension() {
  ChannelFactoryRegistry::instance().registerFactory(
      "socomec", "SocomecCountis", addSocomecCountis);
}

}  // namespace Linux
}  // namespace Supla
```

`supla_linux_extension.cmake`:

```cmake
supla_linux_register_extension(
  NAME socomec
  INIT_FUNCTION initSocomecExtension
  SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/socomec_extension.cpp
  INCLUDE_DIRS
    ${SUPLA_LINUX_PORT_DIR}
    ${_SUPLA_ROOT_FROM_LINUX}/src
)
```

Add `../porting/linux/extensions/socomec/socomec_extension.cpp` to `add_executable(sd4linuxtests ...)` in `extras/test/CMakeLists.txt`.

**Step 4: Run** `sd4linuxtests --gtest_filter='Sd4linuxSocomec*'`. Expected: PASS. Then run the full `ctest --test-dir extras/test/build`. Expected: 3/3 pass.

**Step 5: Commit** `feat(socomec): sd4linux SocomecCountis extension`

---

### Task 8: Linux example config + build script

**Files:**
- Create: `extras/examples/linux/supla-device-socomec.yaml`
- Create: `extras/examples/linux/build-socomec.sh` (mode 755)

**Step 1:** `build-socomec.sh` (same shape as `build-byd.sh`):

```bash
#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
BUILD_DIR="${ROOT}/build-linux-socomec"

cmake -S "${ROOT}/extras/examples/linux" -B "${BUILD_DIR}" \
  -DSUPLA_LINUX_EXTENSION_DIRS="${ROOT}/extras/porting/linux/extensions/socomec"

cmake --build "${BUILD_DIR}" -j"$(nproc)"

echo "Built supla-device-linux with Socomec extension in ${BUILD_DIR}"
```

**Step 2:** `supla-device-socomec.yaml`. Use the YAML from the design doc ("Platform configuration"), with both meters, their `/dev/serial/by-id` paths, `model`, `modbus_address: 2`, `poll_interval_sec: 5`, and a commented-out `max_power_kw`. The header comment tells the reader to install it as `/etc/supla-device.yaml` for `supla-device-linux.service`.

**Step 3: Verify the build**

```bash
extras/examples/linux/build-socomec.sh
build-linux-socomec/supla-device-linux -c extras/examples/linux/supla-device-socomec.yaml --verbose
```

Expected: the log shows two `adding SocomecCountis` lines, no config errors, and timeouts or "offline" (there's no bus on the dev host). Stop with Ctrl-C.

**Step 4: Commit** `feat(socomec): sd4linux example config and build script`

---

### Task 9: ESP-IDF transport

**Files:**
- Create: `extras/porting/esp-idf/esp_idf_socomec_transport.h`, `.cpp`
- Modify: `extras/esp-idf/supla-device/CMakeLists.txt` (add the `.cpp` to `SUPLA_DEVICE_PORTING_SRCS`, add `esp_driver_uart` to `REQUIRES`)

**Step 1: Implement**

```cpp
// esp_idf_socomec_transport.h
#ifndef EXTRAS_PORTING_ESP_IDF_ESP_IDF_SOCOMEC_TRANSPORT_H_
#define EXTRAS_PORTING_ESP_IDF_ESP_IDF_SOCOMEC_TRANSPORT_H_

#include <driver/uart.h>
#include <supla/sensor/socomec_countis.h>

namespace Supla {

// RS-485 half duplex; DE/RE driven by the UART on the RTS pin.
class EspIdfSocomecTransport : public Socomec::Transport {
 public:
  EspIdfSocomecTransport(uart_port_t uart, int txPin, int rxPin, int dePin,
                         int baud);
  bool write(const uint8_t *data, size_t len) override;
  int readAvailable(uint8_t *buffer, size_t maxLen) override;
  void flushRx() override;

 private:
  uart_port_t uart;
  bool ready = false;
};

}  // namespace Supla

#endif  // EXTRAS_PORTING_ESP_IDF_ESP_IDF_SOCOMEC_TRANSPORT_H_
```

```cpp
// esp_idf_socomec_transport.cpp
#include "esp_idf_socomec_transport.h"

#include <supla/log_wrapper.h>

namespace Supla {

EspIdfSocomecTransport::EspIdfSocomecTransport(uart_port_t uart, int txPin,
                                               int rxPin, int dePin, int baud)
    : uart(uart) {
  uart_config_t config = {};
  config.baud_rate = baud;
  config.data_bits = UART_DATA_8_BITS;
  config.parity = UART_PARITY_DISABLE;
  config.stop_bits = UART_STOP_BITS_1;
  config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
  config.source_clk = UART_SCLK_DEFAULT;
  ready = uart_driver_install(uart, 256, 0, 0, nullptr, 0) == ESP_OK &&
          uart_param_config(uart, &config) == ESP_OK &&
          uart_set_pin(uart, txPin, rxPin, dePin, UART_PIN_NO_CHANGE) ==
              ESP_OK &&
          uart_set_mode(uart, UART_MODE_RS485_HALF_DUPLEX) == ESP_OK;
  if (!ready) {
    SUPLA_LOG_ERROR("Socomec: UART%d setup failed", uart);
  }
}

bool EspIdfSocomecTransport::write(const uint8_t *data, size_t len) {
  return ready && uart_write_bytes(uart, data, len) == static_cast<int>(len);
}

int EspIdfSocomecTransport::readAvailable(uint8_t *buffer, size_t maxLen) {
  if (!ready) {
    return -1;
  }
  return uart_read_bytes(uart, buffer, maxLen, 0);  // 0 ticks: never blocks
}

void EspIdfSocomecTransport::flushRx() {
  if (ready) {
    uart_flush_input(uart);
  }
}

}  // namespace Supla
```

**Step 2: Verify** — only if ESP-IDF is installed (`. $IDF_PATH/export.sh`). Temporarily add the design-doc `main.cpp` snippet to `extras/examples/esp_idf/main/main.cpp`, run `idf.py -C extras/examples/esp_idf build` (expected: builds), then revert the snippet. **If `idf.py` is missing (it is on the dev host), don't claim it builds.** Record "ESP32 transport not compile-verified" for the final report.

**Step 3: Commit** `feat(socomec): ESP-IDF RS-485 transport`

---

### Task 10: User documentation

**Files:**
- Create: `docs/integrations/socomec-countis-supla.md`
- Modify: `docs/index.md` (Socomec line: add a "SUPLA driver setup" link)

Contents (short):
1. What gets published, using the SUPLA field coverage table from the design.
2. **Wiring trap first**: the mirrored E03/E23 terminal table and the `+`=A=D+ note (report §4).
3. RPi: `build-socomec.sh`, install the binary + `/etc/supla-device.yaml` + `supla-device-linux.service`, `journalctl -u supla-device -f`.
4. ESP32: the `main.cpp` snippet, a 3.3 V transceiver, DE on RTS, and a bring-up checklist (scope DE during TX; first expected log is the identity; then values match the LCD).
5. Log messages and what to do about each (identity mismatch, offline, inconsistent energy, implausible jump, re-anchored).
6. `max_power_kw`: set it from the breaker rating.

Commit: `docs(socomec): SUPLA driver setup guide`

---

### Task 11: Hardware verification on `pi-szn`

Needs SSH and a build on the Pi (aarch64). Stop and ask the user before installing the systemd unit. It changes a production box.

1. On the Pi: `git clone` this branch (or `rsync` the worktree), `apt install cmake g++ libssl-dev libyaml-cpp-dev`, then `extras/examples/linux/build-socomec.sh`.
2. Foreground run: `build-linux-socomec/supla-device-linux -c supla-device-socomec.yaml --verbose`. Compare V, I, P, PF, F and Ea± with both LCDs. Record them in the probe doc from Task 1.
3. **Ask the user**, then install as a systemd service (README Task 10 §3) and leave it running for 24 h. Pass: `journalctl -u supla-device | grep -c "Socomec.*\(implausible\|inconsistent\)"` = 0, LCD energy Δ = SUPLA Δ, offline count = 0.
4. Fault injection (with the user present): unplug the E03 adapter → its channel goes offline within ~15 s and the E23 keeps publishing; re-plug → it recovers without a restart.
5. Record results in the probe doc. Commit: `docs(socomec): hardware verification results`.

---

### Done when

* `ctest --test-dir extras/test/build`: 3/3 pass, including all `Socomec*` tests.
* Both meters publish on `pi-szn` and match the LCDs; the 24 h soak is clean.
* Final report states plainly whether the ESP32 transport was compile-verified.

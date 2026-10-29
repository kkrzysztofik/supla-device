/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include <gtest/gtest.h>
#include <supla/sensor/socomec_countis.h>

#include <vector>

using Supla::Socomec::Model;

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
  // 50520 = 0xC558, 48 = 0x0030
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

using Supla::Socomec::Energy;
using Supla::Socomec::Measurements;

namespace {
constexpr uint32_t kNa = 0xFFFFFFFF;

void put32(std::vector<uint16_t> *regs, int offset, uint32_t value) {
  (*regs)[offset] = value >> 16;
  (*regs)[offset + 1] = value & 0xFFFF;
}

// 50520..50567, E03 live values from the 2026-10-29 probe.
std::vector<uint16_t> e03Measurements() {
  std::vector<uint16_t> r(48, 0xFFFF);
  put32(&r, 0, 23875);                         // V1 238.75 V
  put32(&r, 6, 5000);                          // F 50.00 Hz (E03: 0.01 Hz LSB)
  put32(&r, 8, 2810);                          // I1 2.810 A
  put32(&r, 16, 55);                           // P 550 W
  put32(&r, 18, static_cast<uint32_t>(-34));   // Q -340 var
  put32(&r, 20, 66);                           // S 660 VA
  put32(&r, 22, static_cast<uint32_t>(-813));  // PF
  put32(&r, 24, 55);                           // P1
  put32(&r, 30, static_cast<uint32_t>(-34));   // Q1
  put32(&r, 36, 66);                           // S1
  put32(&r, 42, static_cast<uint32_t>(-813));  // PF1
  return r;
}

// 50520..50567, E23 live values from the 2026-10-29 probe.
std::vector<uint16_t> e23Measurements() {
  std::vector<uint16_t> r(48, 0);
  put32(&r, 0, 24004);
  put32(&r, 2, 24040);
  put32(&r, 4, 24006);
  put32(&r, 6, 50070);  // E23: 0.001 Hz LSB
  put32(&r, 8, 3006);
  put32(&r, 10, 105);
  put32(&r, 12, 466);
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
std::vector<uint16_t> totalsBlock(uint32_t eaP, uint32_t erP, uint32_t eap,
                                  uint32_t eaN, uint32_t erN) {
  std::vector<uint16_t> r(10, 0);
  put32(&r, 0, eaP);
  put32(&r, 2, erP);
  put32(&r, 4, eap);
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
  EXPECT_EQ(m.phase[0].voltage, 23875);
  EXPECT_EQ(m.phase[0].current, 2810u);
  EXPECT_EQ(m.phase[0].powerActive, 55000000);   // 550 W in 0.00001 W
  EXPECT_EQ(m.phase[0].powerReactive, -34000000);
  EXPECT_EQ(m.phase[0].powerApparent, 66000000);
  EXPECT_EQ(m.phase[0].powerFactor, -813);
  EXPECT_EQ(m.phase[0].phaseAngle, -317);  // atan2(-34, 55) = -31.7 deg
  EXPECT_FALSE(m.phase[1].present);
  EXPECT_FALSE(m.phase[2].present);
}

TEST(SocomecDecodeTests, E23FrequencyHasFinerLsb) {
  Measurements m;
  Supla::Socomec::decodeMeasurements(Model::E23, e23Measurements().data(), &m);
  EXPECT_EQ(m.freq, 5007);  // 50.070 Hz -> 0.01 Hz
  EXPECT_TRUE(m.phase[2].present);
  EXPECT_EQ(m.phase[1].voltage, 24040);
  EXPECT_EQ(m.phase[2].current, 466u);
}

TEST(SocomecDecodeTests, E23AbsentPhaseIsSkipped) {
  auto r = e23Measurements();
  put32(&r, 4, kNa);  // V3
  Measurements m;
  Supla::Socomec::decodeMeasurements(Model::E23, r.data(), &m);
  EXPECT_TRUE(m.phase[1].present);
  EXPECT_FALSE(m.phase[2].present);
}

TEST(SocomecDecodeTests, LineEnergyCombinesMainAndResidual) {
  Energy e;
  // Real E23 line #1 dump from the 2026-10-29 probe.
  const auto r = lineBlock(5909, 4269, 0, 0, 176, 6735, 2084, 6893);
  ASSERT_TRUE(Supla::Socomec::decodeLineEnergy(r.data(), 0, &e));
  EXPECT_EQ(e.value[Supla::Socomec::FwdAct][0], 590942690ULL);
  EXPECT_EQ(e.value[Supla::Socomec::RvrAct][0], 0ULL);
  EXPECT_EQ(e.value[Supla::Socomec::FwdReact][0], 17667350ULL);
  EXPECT_EQ(e.value[Supla::Socomec::RvrReact][0], 208468930ULL);
}

TEST(SocomecDecodeTests, LineEnergyRejectsBadResidualAndAbsent) {
  Energy e;
  auto r = lineBlock(4846, 10000, 0, 0, 0, 0, 0, 0);
  EXPECT_FALSE(Supla::Socomec::decodeLineEnergy(r.data(), 0, &e));
  r = lineBlock(kNa, 0, 0, 0, 0, 0, 0, 0);
  EXPECT_FALSE(Supla::Socomec::decodeLineEnergy(r.data(), 0, &e));
}

TEST(SocomecDecodeTests, TotalsDecodeAndRealCrossCheck) {
  uint64_t totals[Supla::Socomec::EnergyKinds];
  const auto t = totalsBlock(2075161, 39437, 2411590, 0, 545214);
  ASSERT_TRUE(Supla::Socomec::decodeTotals(t.data(), totals));
  EXPECT_EQ(totals[Supla::Socomec::FwdAct], 2075161000ULL);
  EXPECT_EQ(totals[Supla::Socomec::RvrAct], 0ULL);
  EXPECT_EQ(totals[Supla::Socomec::FwdReact], 39437000ULL);
  EXPECT_EQ(totals[Supla::Socomec::RvrReact], 545214000ULL);

  // Real line dumps; their per-direction reactive sums deliberately disagree
  // with the system reactive totals (probe doc), so only active energy gates.
  Energy e;
  ASSERT_TRUE(Supla::Socomec::decodeLineEnergy(
      lineBlock(5909, 4269, 0, 0, 176, 6735, 2084, 6893).data(), 0, &e));
  ASSERT_TRUE(Supla::Socomec::decodeLineEnergy(
      lineBlock(12239, 2756, 0, 0, 274, 3766, 2964, 8611).data(), 1, &e));
  ASSERT_TRUE(Supla::Socomec::decodeLineEnergy(
      lineBlock(2599, 9099, 0, 30, 55, 6186, 502, 3799).data(), 2, &e));
  EXPECT_TRUE(Supla::Socomec::energyMatchesTotals(e, totals, 3));

  // A gross active-energy decode error still fails the gate.
  const uint64_t good = totals[Supla::Socomec::FwdAct];
  totals[Supla::Socomec::FwdAct] += 300000000ULL;  // 3000 kWh
  EXPECT_FALSE(Supla::Socomec::energyMatchesTotals(e, totals, 3));
  totals[Supla::Socomec::FwdAct] = good;

  // Reactive totals are not part of the gate.
  totals[Supla::Socomec::FwdReact] += 200000000ULL;
  EXPECT_TRUE(Supla::Socomec::energyMatchesTotals(e, totals, 3));
}

TEST(SocomecDecodeTests, IdentityMatchesModel) {
  // "Countis E23" packed big-endian, NUL padded, 8 registers
  const uint16_t e23[8] = {0x436F, 0x756E, 0x7469, 0x7320,
                           0x4532, 0x3300, 0x0000, 0x0000};
  EXPECT_TRUE(Supla::Socomec::identityMatches(Model::E23, e23, 8));
  EXPECT_FALSE(Supla::Socomec::identityMatches(Model::E03, e23, 8));
}

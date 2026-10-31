/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

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

TEST_F(SocomecCountisTests, E23PhaseSequenceDirections) {
  loadE23(&meter);
  SocomecCountis em(&meter, Model::E23, 2);
  run(&em, 6000);  // 51315 = 0 -> clockwise
  ASSERT_TRUE(em.isVoltagePhaseSequenceSet());
  EXPECT_TRUE(em.isVoltagePhaseSequenceClockwise());
  meter.blocks[51315] = {1};  // counter-clockwise
  run(&em, 10000);
  EXPECT_TRUE(em.isVoltagePhaseSequenceSet());
  EXPECT_FALSE(em.isVoltagePhaseSequenceClockwise());
}

TEST_F(SocomecCountisTests, E23UnknownPhaseSequenceClearsFlag) {
  // E23 firmware 3.00 answers 132 at 51315, not the documented 0/1/3 enum.
  loadE23(&meter);
  SocomecCountis em(&meter, Model::E23, 2);
  run(&em, 6000);
  ASSERT_TRUE(em.isVoltagePhaseSequenceSet());
  meter.blocks[51315] = {132};
  run(&em, 10000);
  EXPECT_FALSE(em.isVoltagePhaseSequenceSet());
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

TEST_F(SocomecCountisTests, EnergyJitterDoesNotDecreasePublishedValue) {
  // E23 fw 3.00 line-block residuals jitter backwards by a few 0.1 Wh counts;
  // a lifetime SUPLA energy counter must not go down because of that.
  loadE23(&meter);
  meter.blocks[0x4D83] = lineBlock(7000, 500);
  SocomecCountis em(&meter, Model::E23, 2);
  run(&em, 6000);
  ASSERT_EQ(em.getFwdActEnergy(0), 700005000ULL);
  meter.blocks[0x4D83] = lineBlock(7000, 400);  // 10 Wh lower
  run(&em, 10000);
  EXPECT_EQ(em.getFwdActEnergy(0), 700005000ULL);
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

TEST_F(SocomecCountisTests, PerPhaseEnergyCeilingRejectsPhaseSpike) {
  // A 60 Wh step in 5 s is 43 kW on one phase: implausible for an 80 A phase
  // (per-phase ceiling ~27.6 kW) even though it is plausible for the whole
  // three-phase service (82.8 kW). The per-phase guard must reject it.
  loadE23(&meter);
  SocomecCountis em(&meter, Model::E23, 2);
  run(&em, 6000);
  ASSERT_EQ(em.getFwdActEnergy(0), 700000000ULL);
  meter.blocks[0x4D83] = lineBlock(7000, 600);  // +60 Wh on phase 1
  run(&em, 10000);
  EXPECT_EQ(em.getFwdActEnergy(0), 700000000ULL);
}

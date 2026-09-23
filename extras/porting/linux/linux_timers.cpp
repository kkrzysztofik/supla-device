// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "linux_timers.h"

#include <SuplaDevice.h>
#include <supla/log_wrapper.h>
#include <supla/time.h>

#include <atomic>
#include <cstdlib>
#include <mutex>
#include <thread>  // NOLINT(build/c++11)

namespace {

std::atomic<bool> timersRunning{false};
std::mutex timersMutex;
std::thread standardTimer;
std::thread fastTimer;
bool stopRegistered = false;

void supla10msTimer() {
  while (timersRunning.load(std::memory_order_acquire)) {
    SuplaDevice.onTimer();
    delay(10);
  }
}

void supla1msTimer() {
  while (timersRunning.load(std::memory_order_acquire)) {
    SuplaDevice.onFastTimer();
    delay(1);
  }
}

}  // namespace

void Supla::Linux::Timers::init() {
  std::lock_guard<std::mutex> lock(timersMutex);
  if (timersRunning.load(std::memory_order_acquire)) {
    return;
  }

  SUPLA_LOG_DEBUG("Starting linux timers...");
  timersRunning.store(true, std::memory_order_release);
  standardTimer = std::thread(supla10msTimer);
  fastTimer = std::thread(supla1msTimer);

  if (!stopRegistered) {
    std::atexit(&Supla::Linux::Timers::stop);
    stopRegistered = true;
  }
}

void Supla::Linux::Timers::stop() {
  std::thread standardToJoin;
  std::thread fastToJoin;

  {
    std::lock_guard<std::mutex> lock(timersMutex);
    timersRunning.store(false, std::memory_order_release);
    if (standardTimer.joinable()) {
      standardToJoin = std::move(standardTimer);
    }
    if (fastTimer.joinable()) {
      fastToJoin = std::move(fastTimer);
    }
  }

  if (standardToJoin.joinable()) {
    standardToJoin.join();
  }
  if (fastToJoin.joinable()) {
    fastToJoin.join();
  }
}

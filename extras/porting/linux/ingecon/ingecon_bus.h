/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#ifndef EXTRAS_PORTING_LINUX_INGECON_INGECON_BUS_H_
#define EXTRAS_PORTING_LINUX_INGECON_INGECON_BUS_H_

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "ingecon_types.h"

namespace Supla {
namespace Linux {
namespace Ingecon {

class Bus : public std::enable_shared_from_this<Bus> {
 public:
  struct Subscriber {
    struct State {
      void* owner = nullptr;
      mutable std::mutex mutex;
      Readings readings;
      bool cacheValid = false;
    };

    std::shared_ptr<State> state;
  };

  static std::shared_ptr<Bus> acquire(const BusConfig& config);

#ifdef SUPLA_TEST
  static void invalidateCachedReadingsForTest(
      const std::vector<Subscriber>& subscribers);
#endif

  void subscribe(Subscriber subscriber);
  void unsubscribe(void* owner);

 private:
  explicit Bus(BusConfig config);
  void startWorkerIfNeeded();
  void stopWorkerIfIdle();
  void workerLoop();
  bool poll(Readings* readings);
  bool readInputBlock(uint16_t address,
                      uint16_t count,
                      std::vector<uint16_t>* registers);

  BusConfig config_;
  std::mutex subscribersMutex_;
  std::vector<Subscriber> subscribers_;
  std::thread worker_;
  std::atomic<bool> stopWorker_{false};
  bool workerRunning_ = false;
};

}  // namespace Ingecon
}  // namespace Linux
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_INGECON_INGECON_BUS_H_

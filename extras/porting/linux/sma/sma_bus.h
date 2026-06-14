/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#ifndef EXTRAS_PORTING_LINUX_SMA_SMA_BUS_H_
#define EXTRAS_PORTING_LINUX_SMA_SMA_BUS_H_

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "sma_meter_channel.h"
#include "sma_types.h"

namespace Supla {
namespace Linux {
namespace Sma {

constexpr int kDefaultPollIntervalSec = 5;

struct SmaBusConfig {
  std::string serialDevice;
  int baud = 9600;
  SerialMedia media = SerialMedia::RS485;
  uint16_t netAddress = 1;
  int pollIntervalSec = kDefaultPollIntervalSec;
  // Optional built-in type (e.g. WR33-008) used when CMD_GET_CINFO fails.
  std::string deviceProfile;
};

class SmaBus : public std::enable_shared_from_this<SmaBus> {
 public:
  struct Subscriber {
    struct State {
      void* owner = nullptr;
      std::vector<Supla::PV::SmaMappedChannel> channels;
      mutable std::mutex mutex;
      std::map<std::string, double> valuesByKey;
      bool cacheValid = false;
    };

    std::shared_ptr<State> state;
  };

  static std::shared_ptr<SmaBus> acquire(const SmaBusConfig& config);

  void subscribe(Subscriber subscriber);
  void unsubscribe(void* owner);

 private:
  explicit SmaBus(SmaBusConfig config);
  void startWorkerIfNeeded();
  void stopWorkerIfIdle();
  void workerLoop();

  SmaBusConfig config_;
  std::mutex subscribersMutex_;
  std::vector<Subscriber> subscribers_;

 std::thread worker_;
   std::atomic<bool> stopWorker_{false};
   std::atomic<bool> workerRunning_{false};

  std::vector<SmaChannelInfo> channelCatalog_;
  bool cinfoChecked_ = false;
};

}  // namespace Sma
}  // namespace Linux
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SMA_SMA_BUS_H_

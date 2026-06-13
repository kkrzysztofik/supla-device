/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#ifndef EXTRAS_PORTING_LINUX_INGECON_INGECON_BUS_CLIENT_H_
#define EXTRAS_PORTING_LINUX_INGECON_INGECON_BUS_CLIENT_H_

#include <memory>

#include "ingecon_bus.h"
#include "ingecon_types.h"

namespace Supla {
namespace Linux {
namespace Ingecon {

class BusClient {
 public:
  BusClient(void* owner, BusConfig config);
  BusClient(const BusClient&) = delete;
  BusClient& operator=(const BusClient&) = delete;
  BusClient(BusClient&&) noexcept = default;
  BusClient& operator=(BusClient&&) noexcept = default;
  ~BusClient();

  void attach();
  void detach();
  // Returns false if readings or valid is null, or if the underlying bus has
  // no valid cached readings. Callers must pass non-null pointers.
  bool copyReadings(Readings* readings, bool* valid) const;

#ifdef SUPLA_TEST
  void setReadingsForTest(const Readings& readings, bool valid);
#endif

 private:
  BusConfig config_;
  std::shared_ptr<Bus::Subscriber::State> state_;
  std::shared_ptr<Bus> bus_;
};

void shutdownAllClients();

}  // namespace Ingecon
}  // namespace Linux
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_INGECON_INGECON_BUS_CLIENT_H_

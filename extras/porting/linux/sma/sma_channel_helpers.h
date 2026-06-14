/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#ifndef EXTRAS_PORTING_LINUX_SMA_SMA_CHANNEL_HELPERS_H_
#define EXTRAS_PORTING_LINUX_SMA_SMA_CHANNEL_HELPERS_H_

#include <cstring>
#include <string>
#include <utility>

#include "sma_bus.h"

namespace Supla {
namespace PV {

inline bool smaMappingIs(const char* mapping, const char* expected) {
  return mapping != nullptr && expected != nullptr &&
         std::strcmp(mapping, expected) == 0;
}

inline int normalizeSmaPollIntervalSec(int pollIntervalSec) {
  return pollIntervalSec > 0 ? pollIntervalSec
                             : Supla::Linux::Sma::kDefaultPollIntervalSec;
}

inline Supla::Linux::Sma::SmaBusConfig makeSmaBusConfig(
    std::string serialDevice,
    int baud,
    Supla::Linux::Sma::SerialMedia media,
    uint16_t netAddress,
    int pollIntervalSec,
    std::string deviceProfile) {
  return Supla::Linux::Sma::SmaBusConfig{
      std::move(serialDevice),
      baud,
      media,
      netAddress,
      normalizeSmaPollIntervalSec(pollIntervalSec),
      std::move(deviceProfile)};
}

}  // namespace PV
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SMA_SMA_CHANNEL_HELPERS_H_

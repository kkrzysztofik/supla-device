/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#ifndef EXTRAS_PORTING_LINUX_SMA_SMA_METER_CHANNEL_H_
#define EXTRAS_PORTING_LINUX_SMA_SMA_METER_CHANNEL_H_

#include <string>
#include <vector>

#include "sma_types.h"

namespace Supla {
namespace PV {

struct SmaMappedChannel {
  Linux::Sma::SmaChannelDescriptor descriptor;
  std::string key;
  std::string smaName;
  bool resolveByName = false;
};

}  // namespace PV
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SMA_SMA_METER_CHANNEL_H_

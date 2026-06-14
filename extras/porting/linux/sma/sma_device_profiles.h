/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#ifndef EXTRAS_PORTING_LINUX_SMA_SMA_DEVICE_PROFILES_H_
#define EXTRAS_PORTING_LINUX_SMA_SMA_DEVICE_PROFILES_H_

#include <optional>
#include <string>
#include <vector>

#include "sma_types.h"

namespace Supla {
namespace Linux {
namespace Sma {

class SmaDeviceProfiles {
 public:
  static std::optional<std::vector<SmaChannelInfo>> catalogForType(
      const std::string& deviceType);
};

}  // namespace Sma
}  // namespace Linux
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SMA_SMA_DEVICE_PROFILES_H_

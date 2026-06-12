/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 Built-in catalog resolver for SMA profile names.
*/

#include "sma_profile_loader.h"

#include <string>
#include <vector>

#include "sma_device_profiles.h"

namespace Supla {
namespace Linux {
namespace Sma {

std::optional<std::vector<SmaChannelInfo>> SmaProfileLoader::resolveProfile(
    const std::string& profileRef) {
  if (profileRef.empty()) {
    return std::nullopt;
  }
  return builtinCatalog(profileRef);
}

std::optional<std::vector<SmaChannelInfo>> SmaProfileLoader::builtinCatalog(
    const std::string& deviceType) {
  return SmaDeviceProfiles::catalogForType(deviceType);
}

}  // namespace Sma
}  // namespace Linux
}  // namespace Supla

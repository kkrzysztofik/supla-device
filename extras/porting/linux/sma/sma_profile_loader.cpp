/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 YASDI reference (protocol behavior only)
 ----------------------------------------
 Reimplemented from YASDI — Yet Another SMA Data Implementation,
 Copyright (C) 2001-2008 SMA Solar Technology AG, licensed under the
 GNU Lesser General Public License v2.1 or later (LGPL-2.1+). Reference:
 yasdi/sdk/core/repository.c (TRepository_LoadChannelList).
*/

#include "sma_profile_loader.h"

#include <supla/log_wrapper.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "sma_cinfo_parser.h"
#include "sma_device_profiles.h"

namespace Supla {
namespace Linux {
namespace Sma {

namespace {

constexpr uint8_t kYasdiCacheVersion = 10;

bool fileExists(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  return file.good();
}

std::string withBinExtension(const std::string& name) {
  if (name.size() >= 4 && name.compare(name.size() - 4, 4, ".bin") == 0) {
    return name;
  }
  return name + ".bin";
}

}  // namespace

std::optional<std::vector<SmaChannelInfo>> SmaProfileLoader::loadYasdiBinFile(
    const std::string& path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file) {
    return std::nullopt;
  }

  const auto fileSize = file.tellg();
  if (fileSize <= 1) {
    return std::nullopt;
  }
  file.seekg(0, std::ios::beg);

  std::vector<uint8_t> buffer(static_cast<size_t>(fileSize));
  if (!file.read(reinterpret_cast<char*>(buffer.data()), fileSize)) {
    return std::nullopt;
  }

  if (buffer[0] != kYasdiCacheVersion) {
    SUPLA_LOG_WARNING("SmaProfileLoader: unsupported cache version %u in %s",
                      buffer[0],
                      path.c_str());
    return std::nullopt;
  }

  return SmaCinfoParser::parse(buffer.data() + 1, buffer.size() - 1);
}

std::optional<std::vector<SmaChannelInfo>> SmaProfileLoader::resolveProfile(
    const std::string& profileRef) {
  if (profileRef.empty()) {
    return std::nullopt;
  }

  const std::string binName = withBinExtension(profileRef);

  if (profileRef.find('/') != std::string::npos ||
      profileRef.find('\\') != std::string::npos) {
    return loadYasdiBinFile(binName);
  }

  const char* envDir = std::getenv("SMA_PROFILES_DIR");
  if (envDir != nullptr && envDir[0] != '\0') {
    const std::string envPath = std::string(envDir) + "/" + binName;
    if (auto catalog = loadYasdiBinFile(envPath)) {
      SUPLA_LOG_INFO("SmaProfileLoader: loaded profile %s", envPath.c_str());
      return catalog;
    }
  }

  const std::vector<std::string> searchPaths = {
      "./sma-profiles/" + binName,
      "./profiles/" + binName,
      "../profiles/" + binName,
      "profiles/" + binName,
  };

  for (const auto& path : searchPaths) {
    if (!fileExists(path)) {
      continue;
    }
    if (auto catalog = loadYasdiBinFile(path)) {
      SUPLA_LOG_INFO("SmaProfileLoader: loaded profile %s", path.c_str());
      return catalog;
    }
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

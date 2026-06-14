/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include "ingecon_types.h"

namespace Supla {
namespace Linux {
namespace Ingecon {

int normalizePollIntervalSec(int pollIntervalSec) {
  if (pollIntervalSec < 1) {
    return kDefaultPollIntervalSec;
  }
  return pollIntervalSec;
}

int normalizeTimeoutMs(int timeoutMs) {
  if (timeoutMs < 100) {
    return kDefaultTimeoutMs;
  }
  return timeoutMs;
}

int normalizeRetries(int retries) {
  if (retries < 0) {
    return kDefaultRetries;
  }
  if (retries > 10) {
    return 10;
  }
  return retries;
}

const char* profileToString(Profile profile) {
  switch (profile) {
    case Profile::Auto:
      return "auto";
    case Profile::Lite27:
      return "lite_27";
    case Profile::MonofAayV1:
      return "monof_aay_v1";
    case Profile::MonofAapV1:
      return "monof_aap_v1";
    case Profile::TrifAasV1:
      return "trif_aas_v1";
  }
  return "unknown";
}

bool parseProfileName(const std::string& value, Profile* profile) {
  if (profile == nullptr) {
    return false;
  }
  if (value == "auto") {
    *profile = Profile::Auto;
    return true;
  }
  if (value == "lite_27") {
    *profile = Profile::Lite27;
    return true;
  }
  if (value == "monof_aay_v1") {
    *profile = Profile::MonofAayV1;
    return true;
  }
  if (value == "monof_aap_v1") {
    *profile = Profile::MonofAapV1;
    return true;
  }
  if (value == "trif_aas_v1") {
    *profile = Profile::TrifAasV1;
    return true;
  }
  return false;
}

}  // namespace Ingecon
}  // namespace Linux
}  // namespace Supla

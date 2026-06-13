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

}  // namespace Ingecon
}  // namespace Linux
}  // namespace Supla

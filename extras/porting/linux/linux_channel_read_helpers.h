/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#ifndef EXTRAS_PORTING_LINUX_LINUX_CHANNEL_READ_HELPERS_H_
#define EXTRAS_PORTING_LINUX_LINUX_CHANNEL_READ_HELPERS_H_

#include <cmath>

namespace Supla {
namespace Linux {

constexpr int kMaxStaleReadCount = 3;

inline bool markInvalidRead(int* staleReadCounter,
                            int maxStaleReads = kMaxStaleReadCount) {
  if (staleReadCounter == nullptr) {
    return true;
  }
  ++(*staleReadCounter);
  return *staleReadCounter > maxStaleReads;
}

inline void markValidRead(int* staleReadCounter) {
  if (staleReadCounter != nullptr) {
    *staleReadCounter = 0;
  }
}

inline double staleOrUnavailableValue(int* staleReadCounter,
                                      double currentValue,
                                      double unavailableValue,
                                      int maxStaleReads =
                                          kMaxStaleReadCount) {
  if (markInvalidRead(staleReadCounter, maxStaleReads)) {
    return unavailableValue;
  }
  return currentValue;
}

}  // namespace Linux
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_LINUX_CHANNEL_READ_HELPERS_H_

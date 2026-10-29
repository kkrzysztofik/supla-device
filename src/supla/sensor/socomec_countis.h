/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#ifndef SRC_SUPLA_SENSOR_SOCOMEC_COUNTIS_H_
#define SRC_SUPLA_SENSOR_SOCOMEC_COUNTIS_H_

#include <stddef.h>
#include <stdint.h>

#include "electricity_meter.h"

namespace Supla {
namespace Socomec {

enum class Model : uint8_t { E03, E23 };

// Byte pipe to one RS-485 bus. Every call must return immediately.
class Transport {
 public:
  virtual ~Transport() = default;
  virtual bool write(const uint8_t *data, size_t len) = 0;
  // Bytes copied (0 = nothing yet), or -1 on a port error.
  virtual int readAvailable(uint8_t *buffer, size_t maxLen) = 0;
  virtual void flushRx() = 0;
};

}  // namespace Socomec
}  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_SOCOMEC_COUNTIS_H_

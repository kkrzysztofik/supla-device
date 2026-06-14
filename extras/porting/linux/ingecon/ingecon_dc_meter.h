/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#ifndef EXTRAS_PORTING_LINUX_INGECON_INGECON_DC_METER_H_
#define EXTRAS_PORTING_LINUX_INGECON_INGECON_DC_METER_H_

#include <supla/sensor/electricity_meter.h>

#include "ingecon_bus_client.h"
#include "ingecon_types.h"

namespace Supla {
namespace PV {

class IngeconDcMeter : public Supla::Sensor::ElectricityMeter {
 public:
  explicit IngeconDcMeter(Supla::Linux::Ingecon::BusConfig config);
  ~IngeconDcMeter() override;

  void onInit() override;
  void iterateAlways() override;

#ifdef SUPLA_TEST
  void setReadingsForTest(const Supla::Linux::Ingecon::Readings& readings,
                          bool valid);
  void applyReadingsForTest();
#endif

 private:
  void applyReadingsToChannel();
  void applyValidReadings(const Supla::Linux::Ingecon::Readings& readings);
  void setZeroInstantaneousValues();

  Supla::Linux::Ingecon::BusClient busClient_;
  int pollIntervalSec_ = Supla::Linux::Ingecon::kDefaultPollIntervalSec;
  int staleReadCounter_ = 0;
  uint64_t lastReadTime_ = 0;
};

}  // namespace PV
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_INGECON_INGECON_DC_METER_H_

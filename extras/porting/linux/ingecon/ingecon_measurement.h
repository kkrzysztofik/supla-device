/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#ifndef EXTRAS_PORTING_LINUX_INGECON_INGECON_MEASUREMENT_H_
#define EXTRAS_PORTING_LINUX_INGECON_INGECON_MEASUREMENT_H_

#include <supla/sensor/general_purpose_measurement.h>

#include <string>

#include "ingecon_bus_client.h"
#include "ingecon_types.h"

namespace Supla {
namespace PV {

class IngeconMeasurement : public Supla::Sensor::GeneralPurposeMeasurement {
 public:
  IngeconMeasurement(Supla::Linux::Ingecon::BusConfig config,
                     std::string valueKey);
  ~IngeconMeasurement() override;

  void onInit() override;
  double getValue() override;

#ifdef SUPLA_TEST
  void setReadingsForTest(const Supla::Linux::Ingecon::Readings& readings,
                          bool valid);
#endif

 private:
  double readValue(const Supla::Linux::Ingecon::Readings& readings) const;

  std::string valueKey_;
  Supla::Linux::Ingecon::BusClient busClient_;
  int staleReadCounter_ = 0;
};

}  // namespace PV
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_INGECON_INGECON_MEASUREMENT_H_

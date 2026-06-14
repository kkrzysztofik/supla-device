/*
 Copyright (C) Krzysztof Krzysztofik
*/

#ifndef EXTRAS_PORTING_LINUX_BYD_BYD_MEASUREMENT_H_
#define EXTRAS_PORTING_LINUX_BYD_BYD_MEASUREMENT_H_

#include <supla/sensor/general_purpose_measurement.h>

#include <string>

#include "byd_poller_client.h"
#include "byd_types.h"

namespace Supla {
namespace PV {

class BydMeasurement : public Supla::Sensor::GeneralPurposeMeasurement {
 public:
  BydMeasurement(Supla::Linux::Byd::BydAccountConfig account,
                   Supla::Linux::Byd::BydVehicleConfig vehicle,
                   std::string fieldKey);
  ~BydMeasurement() override;

  void onInit() override;
  double getValue() override;

#ifdef SUPLA_TEST
  void setReadingsForTest(const Supla::Linux::Byd::BydReadings& readings);
#endif

 private:
  std::string fieldKey_;
  Supla::Linux::Byd::BydPollerClient pollerClient_;
  int staleReadCounter_ = 0;
};

}  // namespace PV
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_BYD_BYD_MEASUREMENT_H_

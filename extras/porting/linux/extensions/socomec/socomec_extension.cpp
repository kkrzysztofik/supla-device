/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include <supla/log_wrapper.h>
#include <supla/sensor/socomec_countis.h>

#include <memory>
#include <string>
#include <utility>

#include "linux_channel_factory.h"
#include "linux_extension_config_helpers.h"
#include "linux_serial_port.h"
#include "linux_yaml_config.h"

namespace {

class LinuxSocomecTransport : public Supla::Socomec::Transport {
 public:
  LinuxSocomecTransport(std::string devicePath, int baud)
      : port(std::move(devicePath), baud) {
  }

  bool write(const uint8_t *data, size_t len) override {
    if (!port.isOpen() && !port.open()) {
      return false;
    }
    if (!port.writeAllRaw(data, len, "Socomec", 3, false)) {
      port.close();  // reopened on the next write, e.g. after a USB re-plug
      return false;
    }
    return true;
  }

  int readAvailable(uint8_t *buffer, size_t maxLen) override {
    if (!port.isOpen()) {
      return -1;
    }
    const ssize_t n =
        port.readSome(buffer, maxLen, 0, "Socomec", false, true, false);
    if (n < 0) {
      port.close();
    }
    return static_cast<int>(n);
  }

  void flushRx() override {
    if (port.isOpen()) {
      port.flushRx();
    }
  }

 private:
  Supla::Linux::LinuxSerialPort port;
};

bool addSocomecCountis(const Supla::Linux::ChannelFactoryContext &context) {
  const auto &ch = context.channel;
  auto &config = context.config;

  Supla::Linux::ExtensionSerialConfig serial;
  if (!Supla::Linux::parseExtensionSerialConfig(context, &serial)) {
    return false;
  }
  if (!ch["device"] || !ch["device"]["model"] ||
      !ch["device"]["modbus_address"]) {
    SUPLA_LOG_ERROR("Channel[%d] config: SocomecCountis needs device.model "
                    "and device.modbus_address", context.channelNumber);
    return false;
  }

  config.markChannelParameterUsed();
  const auto modelName = ch["device"]["model"].as<std::string>();
  Supla::Socomec::Model model;
  if (modelName == "e03") {
    model = Supla::Socomec::Model::E03;
  } else if (modelName == "e23") {
    model = Supla::Socomec::Model::E23;
  } else {
    SUPLA_LOG_ERROR("Channel[%d] config: invalid Socomec model \"%s\" "
                    "(e03 | e23)", context.channelNumber, modelName.c_str());
    return false;
  }

  config.markChannelParameterUsed();
  const int address = ch["device"]["modbus_address"].as<int>();
  if (address < 1 || address > 247) {
    SUPLA_LOG_ERROR("Channel[%d] config: modbus_address out of range",
                    context.channelNumber);
    return false;
  }

  int pollIntervalSec = 5;
  if (ch["poll_interval_sec"]) {
    Supla::Linux::parseExtensionPollIntervalSec(context, &pollIntervalSec);
  }

  SUPLA_LOG_INFO("Channel[%d] config: adding SocomecCountis %s on %s, "
                 "address=%d", context.channelNumber, modelName.c_str(),
                 serial.devicePath.c_str(), address);

  // The transport lives for the process, like every SUPLA element.
  auto *transport = new LinuxSocomecTransport(serial.devicePath, serial.baud);
  std::unique_ptr<Supla::Sensor::SocomecCountis> meter(
      new Supla::Sensor::SocomecCountis(
          transport, model, static_cast<uint8_t>(address)));
  meter->setRefreshRate(pollIntervalSec);
  if (ch["max_power_kw"]) {
    config.markChannelParameterUsed();
    meter->setMaxPowerKw(ch["max_power_kw"].as<unsigned>());
  }
  return Supla::Linux::addConfiguredChannel(context, std::move(meter));
}

}  // namespace

namespace Supla {
namespace Linux {

void initSocomecExtension() {
  ChannelFactoryRegistry::instance().registerFactory(
      "socomec", "SocomecCountis", addSocomecCountis);
}

}  // namespace Linux
}  // namespace Supla

/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include <gtest/gtest.h>
#include <linux_channel_factory.h>
#include <linux_yaml_config.h>

namespace Supla {

void LinuxYamlConfig::markChannelParameterUsed() {
}

bool LinuxYamlConfig::addCommonChannelParameters(const YAML::Node&,
                                                 Supla::Element*) {
  return false;
}

}  // namespace Supla

namespace Supla {
namespace Linux {
void initSmaExtension();
}  // namespace Linux
}  // namespace Supla

namespace {

class Sd4linuxSmaExtensionTests : public ::testing::Test {
 protected:
  void SetUp() override {
    Supla::Linux::ChannelFactoryRegistry::instance().clear();
  }

  void TearDown() override {
    Supla::Linux::ChannelFactoryRegistry::instance().clear();
  }
};

void expectSmaFactoryRegistered(const char* typeName) {
  const auto* factory =
      Supla::Linux::ChannelFactoryRegistry::instance().findByType(typeName);
  ASSERT_NE(factory, nullptr);
  EXPECT_EQ(factory->pluginName, "sma");
  EXPECT_EQ(factory->typeName, typeName);
}

}  // namespace

TEST_F(Sd4linuxSmaExtensionTests, RegistersAllSmaChannelFactories) {
  Supla::Linux::initSmaExtension();

  EXPECT_TRUE(
      Supla::Linux::ChannelFactoryRegistry::instance().hasPlugin("sma"));
  expectSmaFactoryRegistered("SmaInverter");
  expectSmaFactoryRegistered("SmaDcMeter");
  expectSmaFactoryRegistered("SmaThermometer");
  expectSmaFactoryRegistered("SmaMeasurement");
  EXPECT_EQ(Supla::Linux::ChannelFactoryRegistry::instance().findByType(
                "MissingSmaType"),
            nullptr);
}

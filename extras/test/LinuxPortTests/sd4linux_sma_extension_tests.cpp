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
#include <yaml-cpp/yaml.h>

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
void initIngeconExtension();
void initSmaExtension();
void initBydExtension();
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

void expectIngeconFactoryRegistered(const char* typeName) {
  const auto* factory =
      Supla::Linux::ChannelFactoryRegistry::instance().findByType(typeName);
  ASSERT_NE(factory, nullptr);
  EXPECT_EQ(factory->pluginName, "ingecon");
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

TEST_F(Sd4linuxSmaExtensionTests, RegistersAllIngeconChannelFactories) {
  Supla::Linux::initIngeconExtension();

  EXPECT_TRUE(
      Supla::Linux::ChannelFactoryRegistry::instance().hasPlugin("ingecon"));
  expectIngeconFactoryRegistered("IngeconInverter");
  expectIngeconFactoryRegistered("IngeconDcMeter");
  expectIngeconFactoryRegistered("IngeconMeasurement");
  EXPECT_EQ(Supla::Linux::ChannelFactoryRegistry::instance().findByType(
                "MissingIngeconType"),
            nullptr);
}

void expectBydFactoryRegistered(const char* typeName) {
  const auto* factory =
      Supla::Linux::ChannelFactoryRegistry::instance().findByType(typeName);
  ASSERT_NE(factory, nullptr);
  EXPECT_EQ(factory->pluginName, "byd");
  EXPECT_EQ(factory->typeName, typeName);
}

TEST_F(Sd4linuxSmaExtensionTests, RegistersAllBydChannelFactories) {
  Supla::Linux::initBydExtension();

  EXPECT_TRUE(
      Supla::Linux::ChannelFactoryRegistry::instance().hasPlugin("byd"));
  expectBydFactoryRegistered("BydMeasurement");
  expectBydFactoryRegistered("BydMeter");
  expectBydFactoryRegistered("BydThermometer");
  expectBydFactoryRegistered("BydBinary");
  EXPECT_EQ(Supla::Linux::ChannelFactoryRegistry::instance().findByType(
                "MissingBydType"),
            nullptr);
}

TEST_F(Sd4linuxSmaExtensionTests, ExampleBydYamlUsesRegisteredChannelTypes) {
  Supla::Linux::initBydExtension();

  const YAML::Node root = YAML::LoadFile(
      "../../examples/linux/supla-device-byd.yaml");
  ASSERT_TRUE(root["channels"].IsSequence());

  for (const auto& channel : root["channels"]) {
    ASSERT_TRUE(channel["type"].IsScalar());
    const std::string type = channel["type"].as<std::string>();
    EXPECT_NE(Supla::Linux::ChannelFactoryRegistry::instance().findByType(type),
              nullptr)
        << "Channel type " << type << " from supla-device-byd.yaml";
  }

  EXPECT_TRUE(root["byd"].IsMap());
  EXPECT_TRUE(root["byd"]["email"].IsScalar());
  EXPECT_TRUE(root["byd"]["region"].IsScalar());
}

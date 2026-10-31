/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include <gtest/gtest.h>

#include "linux_channel_factory.h"

namespace Supla {
namespace Linux {
void initSocomecExtension();
}  // namespace Linux
}  // namespace Supla

TEST(Sd4linuxSocomecExtensionTests, RegistersSocomecCountisFactory) {
  auto &registry = Supla::Linux::ChannelFactoryRegistry::instance();
  registry.clear();
  Supla::Linux::initSocomecExtension();
  const auto *factory = registry.findByType("SocomecCountis");
  ASSERT_NE(factory, nullptr);
  EXPECT_EQ(factory->pluginName, "socomec");
  registry.clear();
}

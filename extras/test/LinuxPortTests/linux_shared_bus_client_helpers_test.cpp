/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include <gtest/gtest.h>
#include <linux_shared_bus_client_helpers.h>

#include <memory>
#include <vector>

namespace {

struct FakeConfig {
  int id = 0;
};

struct FakeState {
  void* owner = nullptr;
};

class FakeBus {
 public:
  struct Subscriber {
    std::shared_ptr<FakeState> state;
  };

  static std::shared_ptr<FakeBus> acquire(const FakeConfig& config) {
    ++acquireCount;
    lastConfig = config;
    return std::make_shared<FakeBus>();
  }

  static void reset() {
    acquireCount = 0;
    lastConfig = FakeConfig();
  }

  void subscribe(Subscriber subscriber) {
    ++subscribeCount;
    lastSubscriberOwner = subscriber.state ? subscriber.state->owner : nullptr;
  }

  void unsubscribe(void* owner) {
    ++unsubscribeCount;
    lastUnsubscribedOwner = owner;
  }

  static int acquireCount;
  static FakeConfig lastConfig;

  int subscribeCount = 0;
  int unsubscribeCount = 0;
  void* lastSubscriberOwner = nullptr;
  void* lastUnsubscribedOwner = nullptr;
};

int FakeBus::acquireCount = 0;
FakeConfig FakeBus::lastConfig;

class FakeClient {
 public:
  void detach() {
    ++detachCount;
  }

  int detachCount = 0;
};

}  // namespace

TEST(SharedBusClientHelpersTest, AttachSubscribesOnceAndKeepsBus) {
  FakeBus::reset();
  FakeConfig config;
  config.id = 42;
  int owner = 0;
  auto state = std::make_shared<FakeState>();
  state->owner = &owner;
  std::shared_ptr<FakeBus> bus;

  Supla::Linux::attachSharedBusClient(config, state, &bus);
  Supla::Linux::attachSharedBusClient(config, state, &bus);

  ASSERT_NE(bus, nullptr);
  EXPECT_EQ(FakeBus::acquireCount, 1);
  EXPECT_EQ(FakeBus::lastConfig.id, 42);
  EXPECT_EQ(bus->subscribeCount, 1);
  EXPECT_EQ(bus->lastSubscriberOwner, &owner);
}

TEST(SharedBusClientHelpersTest, DetachUnsubscribesOwnerAndResetsBus) {
  FakeBus::reset();
  int owner = 0;
  auto state = std::make_shared<FakeState>();
  state->owner = &owner;
  std::shared_ptr<FakeBus> bus;
  Supla::Linux::attachSharedBusClient(FakeConfig(), state, &bus);
  std::shared_ptr<FakeBus> attachedBus = bus;

  Supla::Linux::detachSharedBusClient(state, &bus);

  EXPECT_EQ(bus, nullptr);
  ASSERT_NE(attachedBus, nullptr);
  EXPECT_EQ(attachedBus->unsubscribeCount, 1);
  EXPECT_EQ(attachedBus->lastUnsubscribedOwner, &owner);
}

TEST(SharedBusClientHelpersTest, RegistrySnapshotsRemovesAndShutsDownClients) {
  Supla::Linux::SharedBusClientRegistry<FakeClient> registry;
  FakeClient first;
  FakeClient second;

  registry.add(&first);
  registry.add(&second);
  std::vector<FakeClient*> snapshot = registry.snapshot();
  ASSERT_EQ(snapshot.size(), 2u);
  EXPECT_EQ(snapshot[0], &first);
  EXPECT_EQ(snapshot[1], &second);

  registry.remove(&first);
  snapshot = registry.snapshot();
  ASSERT_EQ(snapshot.size(), 1u);
  EXPECT_EQ(snapshot[0], &second);

  Supla::Linux::shutdownSharedBusClients(registry);

  EXPECT_EQ(first.detachCount, 0);
  EXPECT_EQ(second.detachCount, 1);
}

/*
 Copyright (C) Krzysztof Krzysztofik

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include <SuplaDevice.h>
#include <supla-common/proto.h>
#include <supla/action_handler.h>
#include <supla/actions.h>
#include <supla/channels/channel.h>
#include <supla/control/action_trigger.h>
#include <supla/control/button.h>
#include <supla/element.h>
#include <supla/events.h>
#include <supla/log_wrapper.h>
#include <supla/sensor/electricity_meter.h>
#include <supla/sensor/thermometer_driver.h>
#include <supla/time.h>

#include <cmath>
#include <cstdlib>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "hat_display.h"
#include "hat_gpio.h"
#include "hat_model.h"
#include "linux_channel_factory.h"
#include "linux_yaml_config.h"

namespace dhm = Supla::Linux::DisplayHatMini;

namespace {

constexpr int kWakeAction = 100;
constexpr uint32_t kRefreshMs = 250;

std::optional<double> readValue(int channelNumber, dhm::Field field) {
  auto element = Supla::Element::getElementByChannelNumber(channelNumber);
  auto channel = element ? element->getChannel() : nullptr;
  if (!channel || !channel->isStateOnline()) {
    return std::nullopt;
  }
  if (field == dhm::Field::Primary) {
    if (channel->getChannelType() == SUPLA_CHANNELTYPE_RELAY) {
      return channel->getValueBool() ? 1.0 : 0.0;
    }
    double value = channel->getValueDouble();
    if (std::isnan(value) || value == TEMPERATURE_NOT_AVAILABLE) {
      return std::nullopt;
    }
    return value;
  }
  auto meter = dynamic_cast<Supla::Sensor::ElectricityMeter *>(element);
  if (!meter) {
    return std::nullopt;
  }
  double sum = 0;
  for (int phase = 0; phase < 3; phase++) {
    switch (field) {
      case dhm::Field::PowerActive:
        sum += meter->getPowerActive(phase) / 100000.0;
        break;
      case dhm::Field::FwdActEnergy:
        sum += meter->getFwdActEnergy(phase) / 100000.0;
        break;
      case dhm::Field::Current:
        sum += meter->getCurrent(phase) / 1000.0;
        break;
      case dhm::Field::Voltage:
        return meter->getVoltage(0) / 100.0;
      default:
        return std::nullopt;
    }
  }
  return sum;
}

bool isRelay(int channelNumber) {
  auto element = Supla::Element::getElementByChannelNumber(channelNumber);
  return element && element->getChannel() &&
         element->getChannel()->getChannelType() == SUPLA_CHANNELTYPE_RELAY;
}

class HatController : public Supla::Element, public Supla::ActionHandler {
 public:
  explicit HatController(dhm::Config cfg)
      : config(std::move(cfg)),
        nav(config.pages),
        history(config.pages.size()),
        lastSampleMs(config.pages.size(), 0) {}

  bool openGpio() {
    gpioOk = gpio.open("/dev/" + config.gpioChip);
    if (!gpioOk) {
      SUPLA_LOG_ERROR("DisplayHatMini: cannot open GPIO lines on /dev/%s, "
                      "HAT disabled", config.gpioChip.c_str());
    }
    return gpioOk;
  }

  void createButtons() {
    for (int i = 0; i < dhm::kButtonCount; i++) {
      auto button = new Supla::Control::Button(&gpio, dhm::kButtonPins[i]);
      button->setHoldTime(config.holdTimeMs);
      button->setMulticlickTime(config.multiclickTimeMs);
      button->addAction(i, this, Supla::ON_CLICK_1);
      button->addAction(kWakeAction, this, Supla::ON_PRESS);
      if (config.buttonCaptions[i]) {
        auto trigger = new Supla::Control::ActionTrigger();
        trigger->attach(button);
        if (!config.buttonCaptions[i]->empty()) {
          trigger->setInitialCaption(config.buttonCaptions[i]->c_str());
        }
      }
    }
  }

  void onInit() override {
    if (!gpioOk) {
      return;
    }
    auto check = [](int channel) {
      if (!Supla::Element::getElementByChannelNumber(channel)) {
        SUPLA_LOG_ERROR("DisplayHatMini: channel %d not found, shown as --",
                        channel);
      }
    };
    for (const auto &page : config.pages) {
      for (const auto &item : page.items) {
        check(item.channel);
      }
    }
    for (const auto &alert : config.alerts) {
      check(alert.channel);
    }

    displayOk = display.begin(config.framebuffer, config.backlight);
    if (!displayOk) {
      SUPLA_LOG_ERROR("DisplayHatMini: cannot open %s, display disabled. "
                      "Add the fbtft overlay to /boot/firmware/config.txt "
                      "(docs/integrations/display-hat-mini.md)",
                      config.framebuffer.c_str());
    }
    lastActivityMs = millis();
    instance = this;
    std::atexit([] { instance->shutdown(); });
  }

  // Timer thread: only enqueue.
  void handleAction(int, int action) override {
    std::lock_guard<std::mutex> lock(mutex);
    pending.push_back(action);
  }

  void iterateAlways() override {
    if (!gpioOk) {
      return;
    }
    uint32_t now = millis();
    bool changed = processButtons(now);

    if (config.backlightTimeoutSec && backlightOn &&
        now - lastActivityMs > config.backlightTimeoutSec * 1000) {
      backlightOn = false;
      display.setBacklight(false);
    }

    for (size_t i = 0; i < config.pages.size(); i++) {
      const auto &page = config.pages[i];
      if (page.type == dhm::PageType::Chart &&
          (history[i].size() == 0 ||
           now - lastSampleMs[i] >= page.sampleSec * 1000)) {
        lastSampleMs[i] = now;
        if (auto value = readValue(page.items[0].channel,
                                   page.items[0].field)) {
          history[i].push(*value);
        }
      }
    }

    if (changed || now - lastRefreshMs >= kRefreshMs) {
      lastRefreshMs = now;
      auto led = dhm::evaluateLed(config.alerts, readValue,
                                  SuplaDevice.getCurrentStatus());
      gpio.setLed(dhm::ledOutput(led, now));
      if (displayOk && backlightOn) {
        display.render(config, nav, readValue, isRelay, history);
      }
    }
    if (displayOk) {
      display.tick();
    }
  }

 private:
  bool processButtons(uint32_t now) {
    std::vector<int> actions;
    {
      std::lock_guard<std::mutex> lock(mutex);
      actions.swap(pending);
    }
    bool changed = false;
    for (int action : actions) {
      if (action == kWakeAction) {
        if (!backlightOn) {
          // A wake press also produces a click; swallow the next clicks for
          // one hold+multiclick window instead of tracking each press.
          ignoreClicksUntil = now + config.holdTimeMs +
                              config.multiclickTimeMs;
          backlightOn = true;
          display.setBacklight(true);
          changed = true;
        }
        lastActivityMs = now;
        continue;
      }
      if (static_cast<int32_t>(ignoreClicksUntil - now) > 0) {
        continue;  // the click that woke the screen does nothing else
      }
      int toggle = nav.onClick(static_cast<dhm::Button>(action), isRelay);
      if (toggle >= 0) {
        auto handler = dynamic_cast<Supla::ActionHandler *>(
            Supla::Element::getElementByChannelNumber(toggle));
        if (handler) {
          handler->handleAction(0, Supla::TOGGLE);
        }
      }
      changed = true;
    }
    return changed;
  }

  void shutdown() {
    gpio.setLed(dhm::kOff);
    if (displayOk) {
      display.shutdown();
    }
  }

  static HatController *instance;

  dhm::Config config;
  dhm::Navigator nav;
  std::vector<dhm::History> history;
  std::vector<uint32_t> lastSampleMs;
  dhm::HatGpio gpio;
  dhm::HatDisplay display;
  std::mutex mutex;
  std::vector<int> pending;
  bool gpioOk = false;
  bool displayOk = false;
  bool backlightOn = true;
  uint32_t lastActivityMs = 0;
  uint32_t lastRefreshMs = 0;
  uint32_t ignoreClicksUntil = 0;
};

HatController *HatController::instance = nullptr;

bool AddDisplayHatMini(const Supla::Linux::ChannelFactoryContext &context) {
  const auto &ch = context.channel;
  auto &yaml = context.config;
  for (auto key : {"gpio_chip", "framebuffer", "backlight",
                   "backlight_timeout_sec", "hold_time_ms",
                   "multiclick_time_ms", "buttons", "pages", "led"}) {
    yaml.getAndMarkChannelParameter(ch, key);
  }

  dhm::Config config;
  std::string error;
  if (!dhm::parseConfig(ch, &config, &error)) {
    SUPLA_LOG_ERROR("Channel[%d] config: %s", context.channelNumber,
                    error.c_str());
    return false;
  }

  auto controller = new HatController(std::move(config));
  if (controller->openGpio()) {
    controller->createButtons();
  }
  // GPIO failure is logged; the rest of the gateway keeps running.
  return true;
}

}  // namespace

namespace Supla {
namespace Linux {

void initDisplayHatMiniExtension() {
  ChannelFactoryRegistry::instance().registerFactory(
      "displayhatmini", "DisplayHatMini", AddDisplayHatMini);
}

}  // namespace Linux
}  // namespace Supla

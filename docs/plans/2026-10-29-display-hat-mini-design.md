# Pimoroni Display HAT Mini for sd4linux — design

Date: 2026-10-29
Status: approved, ready for implementation plan

## Goal

Support the [Pimoroni Display HAT Mini](https://shop.pimoroni.com/en-eu/products/display-hat-mini)
on a Raspberry Pi running sd4linux:

- **LCD** — local dashboard of this device's own channels, with button-driven
  pages, relay toggles and history charts.
- **Buttons** — short click drives the menu; hold / multi-click are sent to
  SUPLA as action triggers.
- **RGB LED** — device connection status by default, overridden by YAML alert
  rules on channel values.

## Hardware

| Part | Pins | Notes |
|---|---|---|
| LCD ST7789 320×240 | SPI0 CE1, DC GPIO9, backlight GPIO13 | driven by kernel `mipi-dbi-spi` overlay → `/dev/fb1` |
| Buttons A/B/X/Y | GPIO5 / 6 / 16 / 24 | active-low, internal pull-up |
| RGB LED | R GPIO17, G GPIO27, B GPIO22 | active-low; no hardware PWM → on/off, 7 colours |

## Approach

Native C++ sd4linux extension `displayhatmini`, same process as the channels.
LVGL (git submodule, Linux fbdev driver) renders the UI from day one so charts,
gauges and anti-aliased fonts are available without a later rewrite. GPIO via
libgpiod v2. Rejected: Python sidecar (needs new IPC for channel values and
action triggers, two processes fighting over GPIO) and a hybrid of both.

## Components

```
extras/porting/linux/extensions/displayhatmini/
  supla_linux_extension.cmake   # links libgpiod + LVGL
  displayhatmini_extension.cpp  # YAML factory "DisplayHatMini", wires everything
  hat_gpio.{h,cpp}              # libgpiod v2: 4 inputs (pull-up, edge events), 3 LED outputs, backlight
  hat_model.{h,cpp}             # pure logic: pages, cursor, alert rules, history ring buffers
  hat_buttons.cpp               # 4 × Supla::Control::Button fed from hat_gpio + ActionTrigger per button
  hat_led.cpp                   # Element: status colour from SuplaDevice state, alert override
  hat_display.cpp               # Element: LVGL on /dev/fb1, renders hat_model, lv_timer_handler()
```

Everything runs on the sd4linux main loop (`iterateAlways`), no extra threads,
so LVGL (single-threaded) needs no locking.

`hat_model` knows nothing about pixels or pins. It is the unit-test target and
the seam that keeps rendering replaceable.

### Data flow

- `hat_gpio` polls edge events non-blocking; `Button` instances see a normal pin.
- `ON_CLICK_1` → `hat_model` menu handler. `ON_HOLD` / `ON_CLICK_2+` → the
  button's `ActionTrigger` channel → cloud.
- `hat_led` every ~200 ms: status colour from device state; first matching alert
  rule overrides.
- `hat_display` reads channel values from `Supla::Element`, updates `hat_model`
  history on a fixed `sample_sec` timer, pushes values into LVGL labels/charts.

### Kernel setup

One documented line in `/boot/firmware/config.txt` (`dtoverlay=mipi-dbi-spi,...`)
creates `/dev/fb1`. Exact overlay parameters to be verified on hardware.

## YAML config

```yaml
channels:
  - type: DisplayHatMini
    gpio_chip: gpiochip0
    framebuffer: /dev/fb1
    backlight_timeout_sec: 60    # 0 = always on; any button wakes it
    hold_time_ms: 700
    multiclick_time_ms: 300

    buttons:                     # each listed button creates one ActionTrigger channel
      a: { caption: "HAT A" }
      b: { caption: "HAT B" }
      x: { caption: "HAT X" }
      y: { caption: "HAT Y" }

    pages:
      - title: "PV"
        type: values
        items:
          - { channel: 0, field: power_active, label: "Power", unit: "W" }
          - { channel: 0, field: fwd_act_energy, label: "Total", unit: "kWh" }
          - { channel: 1, label: "Pump" }          # relay: ON/OFF, togglable
      - title: "PV today"
        type: chart
        channel: 0
        field: power_active
        sample_sec: 30           # 320 points × 30 s ≈ 2.7 h
        min: 0                   # optional fixed axis, else auto-scale

    led:
      alerts:                    # first match wins
        - { channel: 2, field: soc, below: 20, color: red, blink: true }
        - { channel: 0, field: power_active, above: 3000, color: yellow }
```

- Short-click mapping (fixed): **A/B** prev/next page, **X** move cursor,
  **Y** toggle selected relay item (no-op on non-relay items).
- `field` defaults to the channel's primary value; electricity meters need it
  explicitly. Initial set: `power_active`, `fwd_act_energy`, `voltage`,
  `current`, `soc`.
- Status colours (fixed): green = registered, blue blinking = connecting,
  red = error/offline, magenta = config mode.
- Action trigger channels take the next channel numbers after the
  `DisplayHatMini` entry; omitted buttons create no channel.
- Unknown keys are warned about via `getAndMarkChannelParameter`.

## Error handling

| Failure | Behaviour |
|---|---|
| `/dev/fb1` missing | Log error with the `dtoverlay` line; display disabled, buttons/LED/ATs keep working |
| GPIO chip/line busy or missing | Log error, skip the HAT; sd4linux and other channels keep running |
| Unknown channel/field in a page or alert | Config error at startup naming page/item |
| Channel offline / no value | Show `--`, chart skips sample, alerts do not fire |
| Relay toggle while cloud offline | Toggle locally; state syncs on reconnect |
| Shutdown | LED off, backlight off, release GPIO lines, clear screen |

## Testing

- gtest in `extras/test`, no hardware, against `hat_model`:
  alert evaluation (below/above, first-match, missing value), page/cursor
  navigation and wrap, Y on non-relay item, history ring buffer wrap and
  auto-scale, config parsing (good sample, unknown channel, bad field).
- On-hardware smoke checklist in the integration doc.

## Docs and examples

- `docs/integrations/display-hat-mini.md` — overlay, build flag
  (`-DSUPLA_LINUX_EXTENSION_DIRS=.../displayhatmini`), YAML reference, smoke checklist.
- `extras/examples/linux/supla-device-displayhatmini.yaml`.

## Out of scope

LED dimming (no hardware PWM on LED pins), persisting chart history across
restarts, a cloud-facing display channel, multiple HATs.

## Open questions for the plan

- How sd4linux assigns channel numbers when one YAML entry creates several
  channels (4 action triggers) — check `linux_yaml_config.cpp` around the
  extension factory call.
- Accessing typed channel values (EM fields, BYD `soc`) from outside the
  owning element.
- LVGL version and fbdev driver config; whether `/dev/fb1` is fbdev or DRM-only
  on current Raspberry Pi OS (fallback: LVGL DRM driver).

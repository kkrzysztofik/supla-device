# Pimoroni Display HAT Mini — sd4linux integration guide

Native sd4linux extension that drives the
[Pimoroni Display HAT Mini](https://shop.pimoroni.com/en-eu/products/display-hat-mini)
on a Raspberry Pi from the same process as the device channels:

- **LCD** — local dashboard of this device's own channels: pages of values and
  history charts, driven by the four buttons.
- **Buttons** — short clicks drive the local menu; hold and multi-click are sent
  to SUPLA as action triggers (one channel per configured button).
- **RGB LED** — SUPLA device status by default, overridden by YAML alert rules
  on channel values.

The whole YAML surface is described in [§4](#4-yaml-reference); a complete
runnable example is
[`extras/examples/linux/supla-device-displayhatmini.yaml`](../../extras/examples/linux/supla-device-displayhatmini.yaml).

**Verification status.**

- **Verified in software on the development machine:** the configuration model
  is covered by unit tests (`sd4linuxtests --gtest_filter='HatModel*'`: parser,
  navigation, LED/alert rules, chart history/range, value formatting); the
  example config shipped with this repository is accepted by that parser; and
  the extension binary built from this tree starts with that config (only
  `state_files_path` pointed at a writable directory), logs the expected GPIO
  error without a HAT and keeps running.
- **Brought up on hardware (2026-11-03):** Raspberry Pi 5, Debian 13 (trixie),
  kernel 6.18.50+rpt-rpi-2712, no HDMI attached, running as a systemd service
  next to two Socomec COUNTIS meters. The overlay lines, framebuffer
  (`/dev/fb0`), backlight name (`fb_st7789v`), GPIO chip (`gpiochip0`) and
  service permissions in [§2](#2-raspberry-pi-setup) are the values measured
  there; the LCD rendered a values page with live meter readings.
- **Not yet verified on hardware:** the rest of the smoke test
  ([§6](#6-hardware-smoke-checklist)) — buttons, LED, page navigation, charts,
  backlight timeout. Other Pi models and kernels may differ; use the check
  steps in §2.

---

## 1. Hardware & pins

BCM numbering; libgpiod line offsets are the same numbers.

| Part | Pins (BCM) | Notes |
| --- | --- | --- |
| LCD ST7789 320×240 | SPI0 CE1, DC GPIO9, backlight GPIO13 | kernel display overlay exposes it as a framebuffer (`/dev/fb0` when headless, usually `/dev/fb1` with HDMI; resolve by name, §2.1). GPIO9 is also SPI0 MISO, so SPI must be enabled without MISO (§2.1) |
| Buttons A / B / X / Y | GPIO5 / 6 / 16 / 24 | active-low, internal pull-up |
| RGB LED | R GPIO17, G GPIO27, B GPIO22 | active-low; no hardware PWM, so on/off only, 7 colours |

The HAT sits on the 40-pin header, so the GPIO lines live on the Pi's GPIO
header chip — `gpiochip0` on most kernels (on the Pi 5 with kernel 6.18 it is
`gpiochip0 [pinctrl-rp1]`), but on some older Raspberry Pi 5 kernels it is
`gpiochip4`. Use the chip that `gpioinfo` reports for these lines (§2.3).

---

## 2. Raspberry Pi setup

> The values below were measured on a Raspberry Pi 5 (Debian 13, kernel 6.18).
> The framebuffer number, overlay line and backlight name can differ on other
> kernels and HAT revisions, so run the check commands.

### 2.1 SPI and the display overlay

The HAT uses GPIO9 as the display's DC (data/command) line, but GPIO9 is also
SPI0 MISO. Do **not** enable SPI with `dtparam=spi=on` (or `raspi-config`):
that claims GPIO9 for SPI and the display driver fails to probe with

```text
pinctrl-rp1 ...: pin gpio9 already requested by 1f00050000.spi; cannot claim for pinctrl-rp1:...
fb_st7789v spi0.1: error -EINVAL: Failed to request dc GPIO
```

Enable SPI0 without MISO instead and add the display overlay to
`/boot/firmware/config.txt` (under `[all]`):

```text
dtoverlay=spi0-2cs,no_miso
dtoverlay=fbtft,spi0-1,st7789v,width=240,height=320,rotate=90,dc_pin=9,led_pin=13,speed=40000000
```

Reboot and check that a framebuffer appeared for the HAT:

```bash
dmesg | grep -E "fb_st7789v|graphics fb"  # "graphics fbN: fb_st7789v frame buffer, 320x240 ..."
ls -l /dev/fb*
cat /sys/class/graphics/fb*/name          # find the fb_st7789v one
```

Framebuffer numbers follow kernel registration order: headless the HAT is
usually `/dev/fb0`, with HDMI usually `/dev/fb1`, and the `dtparam=drm_fb*_vc4`
options can shift them. Resolve the HAT index by name (the command below) and
use it for `framebuffer:`; the extension defaults to `/dev/fb1`.

If the panel stays white or colours are inverted, the generic `st7789v` init
does not match your panel: try `rotate=270`, then add `bgr` to the overlay
line; as a last resort use the `mipi-dbi-spi` overlay with an ST7789 init
firmware (Raspberry Pi `overlays/README`, section `mipi-dbi-spi`).

To see what the panel shows without being next to it, read the HAT framebuffer
(RGB565, 320×240 — width × height after `rotate=90`) and convert it, e.g. with
Pillow. Pick the framebuffer by name rather than by number; `/dev/fbN` is
`root:video` mode 0660, so read it as root or as a user in the `video` group
(§2.4):

```bash
# replace user@host with your login; index of the fb named fb_st7789v
FB=$(ssh user@host 'grep -l "^fb_st7789v$" /sys/class/graphics/fb*/name | head -n1 | cut -d/ -f5')
test -n "$FB" || { echo "no fb_st7789v framebuffer - is the display overlay loaded?" >&2; exit 1; }
ssh user@host "cat /dev/$FB" > fb.raw
python3 -c 'from PIL import Image; Image.frombytes("RGB", (320, 240), open("fb.raw", "rb").read(), "raw", "BGR;16").save("fb.png")'
```

`BGR;16` is Pillow's raw mode for 16-bit little-endian pixels with 5 red,
6 green and 5 blue bits — exactly the framebuffer's RGB565 layout. The name
refers to Pillow's byte-order convention, not to BGR channel order.

### 2.2 Backlight path

The display overlay creates a sysfs backlight device; the directory name is
kernel-specific:

```bash
ls /sys/class/backlight/              # note the name, e.g. fb_st7789v
echo 4 | sudo tee /sys/class/backlight/*/bl_power   # backlight off
echo 0 | sudo tee /sys/class/backlight/*/bl_power   # backlight on
```

Put the directory in `backlight:` in the YAML (trailing slash not needed). The
extension writes `bl_power` (`0` = on, `4` = off).

- If the directory does not exist, or the path is wrong, writes fail silently:
  the LCD still works but `backlight_timeout_sec` has no visible effect.
- `bl_power` is owned by root, so a service running as a non-root user cannot
  change it. Either grant the `video` group write access with a udev rule, or
  set `backlight_timeout_sec: 0` and accept an always-on backlight. Rule used
  on the Pi 5, e.g. `/etc/udev/rules.d/99-supla-backlight.rules` (replace
  `fb_st7789v` with the name found above):

  ```text
  SUBSYSTEM=="backlight", KERNEL=="fb_st7789v", ACTION=="add", RUN+="/bin/chgrp video /sys%p/bl_power", RUN+="/bin/chmod g+w /sys%p/bl_power"
  ```

  The `KERNEL` match keeps the rule off other backlights (e.g. a DSI panel).
  After the next boot `ls -l /sys/class/backlight/fb_st7789v/bl_power` shows
  `-rw-rw-r-- root video`.

### 2.3 GPIO chip

```bash
sudo apt install gpiod libgpiod-dev pkg-config
gpioinfo | grep -E "GPIO(5|6|16|24|17|27|22)\b"   # note the chip name
```

Use that chip name in `gpio_chip:` (the extension opens `/dev/<gpio_chip>`).
Press a button while watching the lines to confirm which chip carries them.

### 2.4 Permissions

The user running sd4linux needs access to the GPIO chip, the framebuffer and
the backlight:

```bash
sudo usermod -aG gpio,video supla
# log out and back in (or restart the service) for the new groups to apply
```

For a systemd service, also add the groups to the unit. The example unit in
this repository is
[`extras/examples/linux/supla-device-linux.service`](../../extras/examples/linux/supla-device-linux.service),
installed as `supla-device.service` (see the
[Linux README](../../extras/examples/linux/README.md)); that is the unit name
used below. That unit sets `ProtectKernelTunables=true`, which makes `/sys`
read-only, so backlight writes also need the real (symlink-resolved) sysfs
directory in `ReadWritePaths=`. A drop-in,
`/etc/systemd/system/supla-device.service.d/override.conf`:

```ini
[Service]
SupplementaryGroups=dialout gpio video
# Output of: readlink -f /sys/class/backlight/fb_st7789v
ReadWritePaths=-/sys/devices/platform/axi/1000120000.pcie/1f00050000.spi/spi_master/spi0/spi0.1/backlight/fb_st7789v
```

Keep the `-` prefix. systemd builds the mount namespace before `ExecStart`, and
without `-` a path that does not exist at that moment makes the unit fail with
`226/NAMESPACE` — overlay removed, HAT not fitted, or a different kernel path.
That takes down buttons, LED and every other channel too, so the
[§6](#6-hardware-smoke-checklist) step that removes the overlay would fail at
the unit level instead of logging the framebuffer error. With `-` a missing path
is ignored and the service still starts; if the backlight device shows up later,
restart the service to include it.

The path above is the one on a Raspberry Pi 5; take yours from
`readlink -f`. Then `systemctl daemon-reload && systemctl restart supla-device`.

### 2.5 libgpiod v2

The extension uses the libgpiod **v2** API:

```bash
sudo apt install libgpiod-dev pkg-config
pkg-config --modversion libgpiod        # must be >= 2.0
```

Ubuntu ≥ 24.04 and Raspberry Pi OS Bookworm (and newer) ship v2. If the version
is 1.x, the extension will not configure — upgrade the OS/package or build
libgpiod v2 from source.

---

## 3. Build

The extension has no extra source dependencies beyond libgpiod v2; LVGL
v9.2.2 is fetched by CMake `FetchContent` at configure time, so the first build
needs network access.

```bash
sudo apt install build-essential cmake git libssl-dev libyaml-cpp-dev \
                 libcurl4-openssl-dev \
                 libgpiod-dev pkg-config

cd extras/examples/linux
./build-displayhatmini.sh
```

The script configures with `-DSUPLA_LINUX_EXTENSION_DIRS=<repo>/extras/porting/linux/extensions/displayhatmini`
and produces `<repo>/build-linux-displayhatmini/supla-device-linux`.

Manual equivalent (from the repository root):

```bash
cmake -S extras/examples/linux -B build-linux-displayhatmini \
  -DSUPLA_LINUX_EXTENSION_DIRS="$PWD/extras/porting/linux/extensions/displayhatmini"
cmake --build build-linux-displayhatmini -j"$(nproc)"
```

Run with the example config:

```bash
./build-linux-displayhatmini/supla-device-linux \
  -c extras/examples/linux/supla-device-displayhatmini.yaml
```

`-c/--config` selects the YAML file (default: `etc/supla-device.yaml`). Edit
the example first: `supla.server`, `supla.mail`, `gpio_chip`, `backlight`, and
make sure `state_files_path` exists and is writable.

Expected log lines on a machine without the HAT (or with a wrong chip/path) —
the process keeps running and other channels are unaffected:

```text
DisplayHatMini: cannot open GPIO lines on /dev/gpiochip0, HAT disabled
DisplayHatMini: cannot open /dev/fb1, display disabled. Add the fbtft overlay to /boot/firmware/config.txt (docs/integrations/display-hat-mini.md)
```

Nested configuration mistakes are reported as
`Channel[N] config: DisplayHatMini <details>` and abort startup.

---

## 4. YAML reference

The HAT is configured as one channel entry with `type: DisplayHatMini` (the
type name is case-sensitive).

### 4.1 Top-level keys

| Key | Default | Meaning |
| --- | --- | --- |
| `gpio_chip` | `gpiochip0` | libgpiod chip name; opened as `/dev/<gpio_chip>`. Must be the chip with GPIO5/6/16/24 (buttons) and GPIO17/27/22 (LED). |
| `framebuffer` | `/dev/fb1` | Framebuffer device LVGL writes to. Must exist (readable and writable) at startup. Use the index resolved by name (§2.1) — usually `/dev/fb0` on a headless Pi. |
| `backlight` | `/sys/class/backlight/fb_st7789v` | sysfs backlight directory containing `bl_power`. Wrong/missing path = no backlight control. |
| `backlight_timeout_sec` | `60` | Seconds without a button press before the backlight is switched off. `0` = always on. |
| `hold_time_ms` | `700` | Hold time used by the button elements; a longer press is reported as `ON_HOLD` to the action trigger. |
| `multiclick_time_ms` | `300` | Multi-click window; the second and further clicks within it are reported as `ON_CLICK_2+` to the action trigger. |
| `buttons` | none | Map of `a`, `b`, `x`, `y` (see §4.2). |
| `pages` | `[]` | List of display pages (see §4.3). Empty = blank screen. |
| `led` | none | Map with an `alerts` list (see §4.5). |

Unknown nested keys are rejected at startup; unknown top-level keys are
rejected by sd4linux itself (`unrecognized parameter`). Spelling matters.

### 4.2 `buttons`

Only the keys `a`, `b`, `x`, `y` are accepted, and each has a single optional
sub-key `caption`:

```yaml
buttons:                     # each listed button creates one ActionTrigger channel
  a: { caption: "HAT A" }
  b: { caption: "HAT B" }
  x: { caption: "HAT X" }
  y: { caption: "HAT Y" }
```

All four buttons always work for the local menu. Listing a button additionally
creates a SUPLA action-trigger channel for its hold and multi-click events.
`a:` with no value creates the channel with the default caption; omitting the
button entirely means no action-trigger channel for it.

### 4.3 `pages`

A page is either a **values** page (the default) or a **chart** page. `title`
is free text shown at the top of the screen; it defaults to empty.

Values page:

```yaml
pages:
  - title: Relays
    type: values             # optional; this is the default
    items:                   # required, non-empty
      - { channel: 0, label: Pump }
      - { channel: 1, label: Heater }
```

| Item key | Default | Meaning |
| --- | --- | --- |
| `channel` | required | SUPLA channel number shown in this row (≥ 0). |
| `field` | primary value | Which value to read from that channel (see §4.4). Omit the key for the primary value. |
| `label` | empty | Text on the left of the row. |
| `unit` | empty | Appended to the value after a space: `unit: W` → `1234.6 W`. Relays show `ON`/`OFF` instead. |

Chart page:

```yaml
  - title: Pump history
    type: chart
    channel: 0
    sample_sec: 5            # default 30
    min: 0                   # optional fixed Y axis
    max: 1
```

| Chart key | Default | Meaning |
| --- | --- | --- |
| `channel` | required | SUPLA channel number sampled for the chart. |
| `field` | primary value | Which value to sample (see §4.4). Omit the key for the primary value. |
| `unit` | empty | Appended to the current value shown in the top-right corner. |
| `sample_sec` | `30` | Sampling period in seconds (≥ 1). |
| `min` | auto | Fixed lower Y-axis bound; otherwise the axis auto-scales to the data. |
| `max` | auto | Fixed upper Y-axis bound. |

A chart keeps the last 320 samples, one per screen column: `sample_sec: 30` is
about 2 h 40 min of history, `sample_sec: 5` about 27 min. A chart page has
exactly one series; `items:` is not used on chart pages.

### 4.4 `field` values

`field` selects a specific value from an `ElectricityMeter` channel. When the
key is omitted, the channel's primary value is used — for a relay that is its
on/off state, shown as `ON`/`OFF` and togglable with the Y button. There is no
`field: primary` spelling: `primary` is the absence of the key.

| `field` | Unit | Meaning |
| --- | --- | --- |
| `power_active` | W | Active power. Summed over all three phases. |
| `fwd_act_energy` | kWh | Forward active energy. Summed over all three phases. |
| `voltage` | V | Voltage of phase 1. |
| `current` | A | Current. Summed over all three phases. |

The four electricity-meter fields only work on `ElectricityMeter` channels; on
any other channel the value is shown as `--`. Plain measurement channels (for
example a BYD SoC or a thermometer) need no `field` — use the default primary
value.

### 4.5 `led.alerts`

```yaml
    led:
      alerts:                # first matching alert wins, in list order
        - { channel: 1, above: 0.5, color: yellow }
        - { channel: 0, field: power_active, below: 0, color: green, blink: true }
```

| Alert key | Default | Meaning |
| --- | --- | --- |
| `channel` | required | SUPLA channel number to evaluate. |
| `field` | primary value | Value to evaluate (§4.4); omit for the primary value. |
| `below` | — | Fires when the value is strictly **less than** the threshold. |
| `above` | — | Fires when the value is strictly **greater than** the threshold. |
| `color` | required | LED colour from the list below. |
| `blink` | `false` | Blink the LED (500 ms on / 500 ms off). |

Exactly one of `below` / `above` must be set. The first alert whose condition is
true wins and overrides the status colour; alerts on missing/offline values are
skipped and the next alert is evaluated.

Colours: `red`, `green`, `yellow`, `blue`, `magenta`, `cyan`, `white`.
(Any other name is a configuration error.) The LED has no PWM, so there is no
brightness control and `off` is not an accepted YAML colour.

### 4.6 Button mapping

Short clicks (one click, `ON_CLICK_1`) are handled locally for the menu:

| Button | Short click |
| --- | --- |
| A | Previous page (wraps) |
| B | Next page (wraps) |
| X | Next item on a values page (wraps); no-op on a chart page |
| Y | Toggle the selected relay (only a `primary` item on a relay channel; no-op otherwise) |

Hold and multi-click events are reported to SUPLA by the button's action-trigger
channel. Configure the actions for those channels in the SUPLA app.

### 4.7 Channel numbering

Channel numbers are assigned by the order of the `channels:` list. The
`DisplayHatMini` entry adds no value channel of its own; its action triggers take
the next free numbers, in `a`, `b`, `x`, `y` order, for the buttons that are
listed. With the example config:

| Channel | Source |
| --- | --- |
| 0 | first `VirtualRelay` (Pump) |
| 1 | second `VirtualRelay` (Heater) |
| 2, 3, 4, 5 | Display HAT Mini action triggers A, B, X, Y |

Keep the `DisplayHatMini` entry **last**. A channel entry after it would be
assigned the same numbers the action triggers already took, and adding or
removing a listed button shifts the numbers of the triggers after it — SUPLA
then reports a channel conflict and the device must be removed from the cloud
and re-registered.

---

## 5. Behaviour

### 5.1 Status colours

Without a matching alert the LED shows the SUPLA device status:

| LED | Status |
| --- | --- |
| green, steady | registered and ready (also offline mode and SUPLA protocol disabled) |
| blue, blinking | initialized/starting, network disconnected, registration in progress |
| magenta, steady | configuration mode |
| red, steady | any other status (errors, unknown server address, …) |

The LED and display are refreshed every 250 ms.

### 5.2 Alerts

An alert overrides the status colour for as long as its condition is true; when
it stops matching, the next matching alert (or the status colour) takes over.
Conditions are strict (`below` = `<`, `above` = `>`), so an alert
`{ channel: 1, above: 0.5 }` on a relay is true exactly while the relay is on.

### 5.3 Display and buttons

- Values page: title on top, one row per item with the label on the left and the
  value on the right; the row selected with X is highlighted. Missing values
  are shown as `--`.
- Chart page: title and the current value on top, line chart below. The Y axis
  uses `min`/`max` when set and auto-scales to the data otherwise. Samples are
  taken every `sample_sec`; samples of missing values are skipped.
- The display is only rendered while the backlight is on.

### 5.4 Backlight

With `backlight_timeout_sec: 60` the backlight is switched off after 60 s
without a button press. Rendering is suspended while it is off, but chart
sampling continues, so a chart still has history when the screen wakes.

Any button press wakes the display. The press that woke the screen is swallowed
and does not also move the menu, and click events within
`hold_time_ms + multiclick_time_ms` after waking are ignored. (A press held
longer than that window can still be processed as a hold after the screen
wakes.) Set `backlight_timeout_sec: 0` to keep the backlight always on.

### 5.5 Shutdown

On SIGINT/SIGTERM (Ctrl-C, `systemctl stop`) the LED is switched off, the
backlight is switched off and the screen is cleared.

### 5.6 Failure handling

| Failure | Behaviour |
| --- | --- |
| `/dev/fb1` missing or not writable | Error logged with the overlay hint; display disabled; buttons, LED and action triggers keep working |
| GPIO chip or lines missing/busy | Error logged, HAT disabled (no display, buttons or LED); sd4linux and all other channels keep running |
| Unknown nested key, field, colour, page type or alert threshold combination | Startup configuration error naming the offending key; the process does not start |
| `channel` in a page/alert does not exist | Logged at init; the item shows `--`; the alert never fires; startup continues |
| Channel offline or no value yet | Item shows `--`, chart skips the sample, alerts do not fire |
| Relay toggled while the cloud connection is down | Toggled locally; the state syncs on reconnect |
| Backlight path wrong/missing | No backlight control (panel stays lit); set `backlight_timeout_sec: 0`. Under systemd keep the `-` prefix in `ReadWritePaths=` (§2.4), otherwise a missing path stops the whole unit |

---

## 6. Hardware smoke checklist

> **Not executed yet.** Bring-up (overlay, framebuffer, LCD rendering) was
> confirmed on a Raspberry Pi 5 (see the verification status at the top); the
> interactive steps below have not been run. Run this on the Pi with the
> example config, adjusted for your `gpio_chip`, `backlight` and `framebuffer`:

- [ ] Startup: LED blinks blue while connecting, turns green after registration.
- [ ] LCD shows the "Relays" page; B → "Pump history" chart; A → back.
- [ ] X moves the highlight; Y on Pump toggles it and the SUPLA app shows the change within ~1 s.
- [ ] Toggle Heater from the app → LED turns yellow (alert) while on, green when off.
- [ ] Hold A ~1 s and double-click B → action triggers visible in the SUPLA app (configure an action on them first).
- [ ] Wait `backlight_timeout_sec` → backlight off; press X → backlight on, page unchanged.
- [ ] Unplug network → LED goes blue blinking / red; replug → green.
- [ ] `systemctl stop` / Ctrl-C → LED off, screen black, backlight off.
- [ ] Remove the overlay line, reboot → log shows the framebuffer error, buttons and LED still work.

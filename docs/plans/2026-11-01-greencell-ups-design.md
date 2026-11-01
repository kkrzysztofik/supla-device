# Green Cell PowerProof 1000VA UPS in SUPLA — design

## Hardware (probed 2026-11-01 on `supla-sma`, Debian 13 KVM guest)

- USB `0665:5161` "INNO TECH USB to Serial" (Cypress HID), `/dev/hidraw1`.
- Megatec/Q1 protocol, answered raw over hidraw:
  - `Q1` → `(231.5 174.7 226.7 012 50.0 27.5 --.- 00001001` — in 231.5 V,
    out 226.7 V, load 12 %, 50 Hz, battery 27.5 V, no temperature,
    status bits: mains OK, battery OK, line-interactive, beeper on.
  - `I` → `1000VA` (no vendor/model strings).
  - `F` → `230.0 004 24.00 50.0` — 24 V pack (2×12 V).
- No battery % or runtime from the UPS; any charge % is a voltage estimate.

## Approach

NUT does the UPS side, and sd4linux's existing `Cmd` source and `Simple`
parser do the SUPLA side. No C++.

A native Megatec extension was rejected. It would mean owning the protocol
code, and NUT also brings instant commands and optional `upsmon` shutdown.

## Components

### NUT (standalone, localhost)

`/etc/nut/ups.conf`:

```ini
[greencell]
  driver = nutdrv_qx
  port = auto
  vendorid = 0665
  productid = 5161
  subdriver = cypress
  # calibration knobs for the voltage→% estimate, tune after a real discharge:
  # default.battery.voltage.low = 21.4
  # default.battery.voltage.high = 26.0
```

- `nut.conf`: `MODE=standalone`.
- `upsd.conf`: `LISTEN 127.0.0.1 3493`.
- `upsd.users`: user `supla`, `instcmds = beeper.enable beeper.disable`.

### Separate SUPLA device

The UPS gets its own device, following the `supla-device-ingecon` pattern.
The WR33 device's channel list stays unchanged.

- `/etc/supla-device-ups.yaml`, `state_files_path: /var/lib/supla-device-ups`
- `supla-device-ups.service` (copy of `supla-device.service` with the new `-c`)

One `Cmd` source and one shared `Simple` parser. awk fixes the line order:

| line | value | channel |
|---|---|---|
| 0 | `ups.status` contains `OB` → 1/0 | `BinaryParsed` "On battery" |
| 1 | `ups.status` contains `LB` → 1/0 | `BinaryParsed` "Battery low" |
| 2 | `battery.charge` | `GeneralPurposeMeasurementParsed` "Battery" % |
| 3 | `ups.load` | `GeneralPurposeMeasurementParsed` "Load" % |
| 4 | `input.voltage` | `ElectricityMeterParsed` phase 1 `voltage` |
| 3 | `ups.load` × 7 | same EM, `power_active` W (700 W at 100 %; multiplier is the calibration knob) |
| 5 | `ups.beeper.status == enabled` | `CmdRelay` "Beeper" via `upscmd` |

Refresh: 5000 ms. Each refresh is one `upsc` call that feeds every channel.

## Error handling

- `upsd` down or data stale → `upsc` prints nothing to stdout. The awk
  `END{if(!NR) exit 1}` emits no lines, and the `Simple` parser treats empty
  content as invalid (`parser/simple.cpp:72`). Channels go invalid instead
  of showing "on mains, 0 %".
- A single missing variable → a blank line → stored as a string → that
  channel reads invalid.

## Testing

- `upsc greencell` values match the raw `Q1` reading.
- Pull the mains plug for a few seconds: "On battery" flips within about 5 s,
  and a SUPLA notification arrives.
- Toggle the "Beeper" relay; `ups.beeper.status` follows.
- Low battery is not tested deliberately (it needs a deep discharge).

## Out of scope

- `upsmon` auto-shutdown: about 3 lines of config, add later if wanted.
- `battery.runtime`: needs `runtimecal` calibration first.
- Output voltage and frequency: `ElectricityMeterParsed` has no frequency key.

# Socomec COUNTIS E03/E23 — SUPLA driver setup

The portable COUNTIS driver publishes one or two Socomec COUNTIS electricity meters
as SUPLA `ElectricityMeter` channels. It builds into `sd4linux` (Raspberry Pi, Debian)
and into ESP-IDF firmware from one shared source file, so the register handling is
identical on both platforms:

* `src/supla/sensor/socomec_countis.{h,cpp}` — Modbus RTU FC03 framing, decoders,
  energy plausibility guard, non-blocking poll state machine.
* `extras/porting/linux/extensions/socomec/` — sd4linux `LinuxSerialPort` transport
  and YAML channel factory.
* `extras/porting/esp-idf/esp_idf_socomec_transport.{h,cpp}` — ESP-IDF UART
  RS-485 half-duplex transport.

Register evidence and hardware measurements: [commissioning report](socomec-countis-commissioning-report.md),
[register reference](socomec-countis-modbus.md), [raw probe of both meters](socomec-countis-probe-2026-10-29.md).

## ⚠ Wiring first — the polarity trap

Both meters are silent (no reply, no exception) when the RS-485 pair is reversed, so
check the wiring before blaming the software.

1. **The terminal numbering is mirrored between the models:**

   | Terminal | COUNTIS E03 | COUNTIS E23 |
   | --- | --- | --- |
   | pin 1 | **+** | NC / shield |
   | pin 2 | **−** | **−** |
   | pin 3 | NC / shield | **+** |

   Wiring both meters "by terminal number" from one drawing reverses the pair on one
   of them.

2. **`+` means `A` means `D+`.** Socomec prints `+`/`−`, never `A`/`B`. Many USB-RS485
   adapters follow the TIA/EIA-485 convention where the *inverting* line is called A,
   so "A to A, B to B" can be exactly wrong. Follow the `+`/`−` markings.

Both meters in this installation are self-powered from their voltage inputs: with no
mains they are completely dead on the bus, and the driver reports them offline.

## What gets published

| SUPLA field | E23 (three-phase) | E03 (single-phase) |
| --- | --- | --- |
| Frequency (0.01 Hz) | 50526 ÷ 10 | 50526 |
| Voltage per phase (0.01 V) | V1/V2/V3 | V1 |
| Current per phase (0.001 A) | I1/I2/I3 | I1 |
| Active/reactive/apparent power per phase | P/Q/S 1–3 | P/Q/S 1 |
| Power factor per phase (0.001) | PF1–PF3 | PF1 |
| Phase angle per phase (0.1°) | derived `atan2(Qᵢ, Pᵢ)` | phase 1 |
| Forward/reverse active energy per phase | per-line blocks, 1 kWh + 0.1 Wh | lifetime totals, phase 1 |
| Forward/reverse reactive energy per phase | per-line blocks, 1 kvarh + 0.1 Wh | lifetime totals, phase 1 |
| Voltage phase sequence | 51315 (see note) | — |

Absent registers are skipped, never published as `655.35 V` or similar. Not published
because SUPLA has no field for them: phase-to-phase voltages, neutral current, sum
powers, system averages, tariffs.

**Why the energies do not equal the meter's summed totals.** Per-phase energy comes from
the meter's `Line #1/#2/#3` blocks (the only per-phase source). On this hardware those
blocks do not add up exactly to the `50946` system totals:

* per-line **active** energy tracks the system total to ~0.015 % — at 20.7 MWh that is a
  static ~3 kWh installation-history offset, which stays constant;
* per-direction per-line **reactive** counters differ from the system reactive totals by
  up to ~2 % (≈507 vs ≈394 kvarh here). Socomec defines those two objects differently
  (per-phase counters versus system reactive totals), so this is not a decoding error.

That is why the driver's sanity cross-check gates **active** energy only, and why a
comparison against the LCD's summed reactive total is not a valid health check. The raw
evidence and the decoded tables are in the
[2026-10-29 probe](socomec-countis-probe-2026-10-29.md).

Notes:

* The E03 is single-phase and has **no per-line energy block**; the driver reads the
  `50946` fine totals and publishes them as phase 1. The `no line-1 energy block,
  using totals` INFO log at startup is expected on an E03.
* On E23 firmware 3.00 the phase-sequence register returns `132`, not the documented
  `0`/`1`/`3`. The driver treats anything outside 0…1 as unknown and simply clears the
  SUPLA phase-sequence flag.
* One meter per RS-485 bus, one transport per bus. Both meters ship at address 2, so
  they must stay on separate buses (as wired here).

## Raspberry Pi (sd4linux)

```bash
cd extras/examples/linux
./build-socomec.sh                     # -> build-linux-socomec/supla-device-linux
```

Or from the repository root:

```bash
extras/examples/linux/build-socomec.sh
```

Install the binary, config, and the generic service unit (the following commands assume
the repository root as the working directory):

```bash
sudo install -m 0755 build-linux-socomec/supla-device-linux /usr/local/bin/
sudo install -m 0644 extras/examples/linux/supla-device-socomec.yaml \
    /etc/supla-device.yaml
sudo cp extras/examples/linux/supla-device-linux.service \
    /etc/systemd/system/supla-device.service
sudo systemctl daemon-reload
sudo systemctl enable --now supla-device.service
journalctl -u supla-device.service -f
```

Before the first run set `supla.server` and `supla.mail` in `/etc/supla-device.yaml`,
and keep its `state_files_path` aligned with the unit's `StateDirectory`
(`/var/lib/supla-device`). GUID and AuthKey are generated on first start under
`state_files_path/guid_auth.yaml`. The service user needs access to the serial
adapters (the example unit adds `SupplementaryGroups=dialout`).

Use the stable `/dev/serial/by-id/...` paths, not `/dev/ttyUSB*`: enumeration order is
not guaranteed across re-plugs. The example config already does this.

Foreground test without the service:

```bash
build-linux-socomec/supla-device-linux -c /etc/supla-device.yaml --verbose
```

## ESP32 (ESP-IDF)

Wire a 3.3 V RS-485 transceiver (MAX3485/SP3485), with DE/RE on the UART's RTS pin.
**Tie the transceiver's `/RE` to the same RTS-driven `DE` line** (or otherwise keep the
receiver disabled while transmitting). If `/RE` stays enabled, the transceiver echoes the
8-byte request back, every reply parses as a bad frame, and the meter is reported offline
after ~15 s even though the wiring is otherwise correct. The transport enables the ESP-IDF
hardware half-duplex mode (`UART_MODE_RS485_HALF_DUPLEX`), so the driver never toggles
DE/RE itself.

```cpp
#include "esp_idf_socomec_transport.h"
#include <supla/sensor/socomec_countis.h>

auto bus = new Supla::EspIdfSocomecTransport(
    UART_NUM_1, /*tx*/ 17, /*rx*/ 16, /*de*/ 4, 9600);
auto em = new Supla::Sensor::SocomecCountis(
    bus, Supla::Socomec::Model::E23, 2);
em->setMaxPowerKw(25);
```

Create the transport and meter before `SuplaDevice.begin()`; the transport is not
owned by the meter and must outlive it (allocate it for the process lifetime, as
above). Pins and model are fixed at build time — there is no web-UI page.

Bring-up checklist:

1. Scope DE/RE during a request: DE must be asserted for the whole 8-byte request,
   then released before the meter replies.
2. First expected log is the identity check; a wrong `model` disables polling with an
   ERROR instead of publishing garbage.
3. Values should match the meter LCD (V, I, P, PF, F). For energy, compare trend and
   resolution rather than the summed LCD total: per-phase energy comes from the line
   blocks and carries the static offsets described above (active ≈0.015 %, reactive up to
   ~2 %).
4. An E03 logs `no line-1 energy block, using totals` once and publishes the totals.
5. Leave it running for a day and confirm no repeated `implausible`/`inconsistent`
   warnings.

## Log messages

| Log | Meaning | What to do |
| --- | --- | --- |
| `adding SocomecCountis e03 on … address=2` | YAML channel accepted (INFO) | — |
| `meter is not a COUNTIS E03/E23, polling disabled` (ERROR) | `device.model` does not match the meter's extended name | Fix `model`; check the wiring/polarity if the identity read is garbage |
| `no line-1 energy block, using totals` (INFO) | E03 has no per-line block | Expected on E03 |
| `exception reading register N` (WARNING) | Meter rejected one register block | That step aborts the cycle and counts toward the offline threshold; usually a firmware/model mismatch. Two exceptions are handled instead of aborting: the E03 line-1 block (permanent totals fallback) and the phase-sequence register (ignored) |
| `meter not answering, marked offline` (WARNING) | 3 consecutive failed cycles | Check power, RS-485 pair, adapter, `serial.device` path |
| `energy registers inconsistent, keeping previous values` (WARNING) | Σ per-line active energy no longer matches the meter totals | A decode/register problem; report with the raw probe |
| `implausible energy jump rejected` (WARNING) | One energy counter jumped faster than `max_power_kw` allows | A real meter fault or a too-tight `max_power_kw` |
| `energy counter re-anchored A -> B` (WARNING) | The counter stayed on a new value for 12 cycles (meter swap/reset) | Expected after replacing a meter |

## `max_power_kw`

The energy plausibility filter rejects any counter step that would need more power
than `max_power_kw` since the last accepted value. `max_power_kw` is the **installation-wide**
ceiling; the driver divides it by the number of phases and gives each per-phase guard its
share, so one phase cannot justify a step that is only plausible for the whole service.
Defaults are deliberately generous: 82 800 W (≈83 kW, i.e. 27 600 W per phase) for the E23
(3 × 80 A × 230 V × 1.5) and 40 000 W for the E03, a single-phase direct meter whose ceiling
is a round installation limit rather than a formula fit. Set it from the installation's
breaker rating to tighten the filter — and keep it at least 3× the largest single-phase
load you expect, since each E23 phase gets one third of it:

```yaml
    max_power_kw: 25
```

Rejections keep the previous energy values, so a badly chosen value shows up as a
repeating `implausible energy jump rejected` warning, not as wrong data. If the real load
stays above the configured ceiling, the guard re-anchors to it after the 12-cycle
re-anchor window instead of freezing energy publication.

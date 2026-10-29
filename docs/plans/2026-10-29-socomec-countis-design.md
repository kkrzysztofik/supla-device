# Socomec COUNTIS E03/E23 — SUPLA driver design

**Date:** 2026-10-29 · **Status:** approved · **Targets:** sd4linux (RPi 5 `pi-szn`), ESP32 (ESP-IDF)

Inputs: [commissioning report](../integrations/socomec-countis-commissioning-report.md),
[register reference](../integrations/socomec-countis-modbus.md).

## Goal

Publish both COUNTIS meters as SUPLA `ElectricityMeter` channels, with every EM field the
meters can provide, from one portable driver that builds for Linux and ESP-IDF.

## Decisions

| Topic | Decision | Why |
| --- | --- | --- |
| ESP32 stack | ESP-IDF only | Matches `extras/examples/esp_idf`; `UART_MODE_RS485_HALF_DUPLEX` drives DE/RE in hardware |
| I/O model | Non-blocking state machine inside `iterateAlways()` | Never stalls the SUPLA loop; no threads/mutexes on either target (Ingecon worker needed ~6 deadlock fixes) |
| Energy resolution | Finest available: per-line main + residual (0.1 Wh) | Per-phase energy is SUPLA's native model; 1 kWh LSB makes charts useless |
| Model selection | Explicit `model: e03 \| e23`, cross-checked against identity | A wrong frequency scale is a silent ×10 error |
| Writes to meter | None | Commissioning is done on the front panel; `57856 = 0xB2` is a product reset |
| Reuse of Ingecon RTU code | No | FC04-only and Linux-bound; the FC03 + CRC builder is ~25 lines |

## Architecture

```
src/supla/sensor/socomec_countis.{h,cpp}        portable; compiled by both targets
  Supla::Socomec::Model { E03, E23 }
  Supla::Socomec::Transport  write / readAvailable (non-blocking) / flushRx
  buildReadHolding / parseReadHolding          FC03 frames, CRC16
  decode(model, blocks) -> Snapshot            pure, unit-tested
  EnergyGuard                                  per-counter plausibility filter
  Supla::Sensor::SocomecCountis : ElectricityMeter
    iterateAlways()        steps the bus state machine, then calls the base
    readValuesFromDevice() copies the last *complete* Snapshot into the setters

extras/porting/linux/extensions/socomec/       YAML factory + LinuxSerialPort transport
extras/porting/esp-idf/esp_idf_socomec_transport.{h,cpp}   UART RS485 half-duplex
```

The base `ElectricityMeter::iterateAlways()` keeps the publish timing (`refreshRateSec`,
default 5 s). One `SocomecCountis` per meter and one transport per bus (one meter per bus
in this installation). Sharing a bus between meters (needs a bus-busy token) is out of scope;
both meters sit at address 2 now and would collide anyway.

## SUPLA field coverage

| SUPLA field | E23 | E03 |
| --- | --- | --- |
| `FREQ` (0.01 Hz) | 50526 raw ÷ 10 | 50526 raw |
| `VOLTAGE`[3] | 50520/22/24 | 50520 |
| `CURRENT`[3] (+ `OVER_65A` automatic) | 50528/30/32 | 50528 |
| `POWER_ACTIVE/REACTIVE/APPARENT`[3] | 50544–50561 | 50544, 50550, 50556 |
| `POWER_FACTOR`[3] | 50562/64/66 | 50562 |
| `PHASE_ANGLE`[3] | derived `atan2(Qᵢ, Pᵢ)`, 0.1° | same, phase 1 |
| `FORWARD/REVERSE_ACTIVE_ENERGY`[3] | line blocks Ea+/Ea− | L1 block if present, else 50946/50952 |
| `FORWARD/REVERSE_REACTIVE_ENERGY`[3] | line blocks Er+/Er− | L1 block if present, else 50948/50954 |
| `VOLTAGE_PHASE_SEQUENCE` | 51315 (0 CW, 1 CCW, 3 → flag cleared) | – |
| `*_BALANCED`, `VOLTAGE_PHASE_ANGLE_12/13`, `CURRENT_PHASE_SEQUENCE` | not available | not available |

Not published (no EM field): U12/U23/U31, neutral current, Σ powers, system averages,
tariffs. Vector-balanced energy would need integrating ΣP in the driver, which would drift
from the MID counters; add it only if the balanced view is needed.

Unit conversions: V and A are identity (0.01 V, 0.001 A); power raw × 10 W → × 10⁶ in
0.00001 W; energy `main × 100000 + residual × 10` in 0.01 Wh — integer only, no float.
`0xFFFF` / `0xFFFFFFFF` means "absent": skip the setter; a fully absent phase →
`resetReadParametersForPhase()`.

## Poll cycle

One FC03 request in flight; ≥ 50 ms gap between requests (report finding #4).

| # | Registers | Count | E23 | E03 |
| --- | --- | --- | --- | --- |
| 1 | 50520–50567 measurements | 48 | ✅ | ✅ |
| 2 | 0x4D83–0x4D8E L1 energy | 12 | ✅ | if present; exception → permanent fallback to #5 |
| 3 | 0x5583–0x558E L2 energy | 12 | ✅ | – |
| 4 | 0x5D83–0x5D8E L3 energy | 12 | ✅ | – |
| 5 | 50946–50955 10 Wh totals | 10 | ✅ (cross-check) | ✅ |
| 6 | 51315 phase sequence | 1 | ✅ | – |

Once at start-up: identity (50050 product name) must match the configured model, otherwise
log ERROR and never publish.

```
IDLE --due--> SEND(i) --> WAIT --complete+CRC ok--> parse, i++ --> ... --> publish snapshot
                           |--timeout 300 ms--> retry once --> fail: abort cycle
```

## Error handling

* Timeout → one retry; second failure aborts the cycle and keeps the last snapshot.
* CRC / length / slave / function mismatch → `flushRx()`, handled like a timeout.
* Modbus exception → logged once with the address; that step fails, other steps go ahead.
* 3 consecutive failed cycles (~15 s) → `resetReadParameters()` (meter shows offline;
  covers "mains down, self-powered meter dead").

## Energy plausibility — `EnergyGuard`

Applied to each energy counter. If any counter in a cycle is rejected, the whole energy set
keeps last cycle's values (instantaneous values are still published), so phase sums stay consistent.

1. Sentinel (`0xFFFF…`) or residual > 9999 → reject.
2. No baseline yet → accept only if the cross-check passes:
   Σ lines ≈ 10 Wh total ≈ 50770/50776 × 1000 within 1 kWh.
3. `0 ≤ Δ ≤ Pmax · Δt + 1 LSB` (Δt since last *accepted* value) → accept.
4. Otherwise it becomes a candidate. If the same value (± Pmax · Δt drift) shows up for
   12 consecutive cycles and the cross-check passes → re-anchor with a WARN (meter
   swap or reset). Otherwise reject and keep the last value.
5. Rejections are logged with raw values, at most one line per minute.

`Pmax` defaults: E23 = 3 × 80 A × 230 V × 1.5 ≈ 83 kW, E03 ≈ 40 kW; overridable
(`max_power_kw` / `setMaxPowerKw()`) — a tighter, installation-specific value tightens the
filter. Instantaneous values get only the sentinel filter.

## Platform configuration

**Linux** — `extras/examples/linux/supla-device-socomec.yaml`, `build-socomec.sh`, systemd unit:

```yaml
channels:
  - type: SocomecCountis
    caption: Countis E23
    serial:
      device: /dev/serial/by-id/usb-FTDI_FT232R_USB_UART_BG041JVI-if00-port0
      baud: 9600
    device:
      model: e23            # required
      modbus_address: 2
    poll_interval_sec: 5
    max_power_kw: 25        # optional
```

Reuses `parseExtensionSerialConfig` / `parseExtensionPollIntervalSec`. `LinuxSerialPort`
is already `O_NONBLOCK` and fixed at 8N1, which matches both meters. A parity option is left
out until a meter needs it.

**ESP-IDF** — wired in `main.cpp`:

```cpp
auto bus = new Supla::EspIdfSocomecTransport(UART_NUM_1, /*tx*/17, /*rx*/16, /*de*/4, 9600);
auto em  = new Supla::Sensor::SocomecCountis(bus, Supla::Socomec::Model::E23, 2);
em->setMaxPowerKw(25);
```

Transport: `uart_param_config` 8N1 → `uart_set_pin` (DE on RTS) → `uart_driver_install`
→ `uart_set_mode(UART_MODE_RS485_HALF_DUPLEX)`; `readAvailable` = `uart_read_bytes(…, 0)`.
Use a 3.3 V transceiver (MAX3485/SP3485). No web-UI page; pins and model are fixed at build time.
The README carries the mirrored E03/E23 terminal table (report §4).

## Testing

`extras/test/SocomecTests/` (gtest, existing glob):

* **frame** — FC03 request bytes + CRC; parse ok / exception / bad CRC / wrong slave / short.
* **decode** — raw registers from the `pi-szn` run → expected SUPLA values (both frequency
  scales, absent phases, power, PF sign, phase angle, main + residual, phase sequence).
* **energy guard** — every row of the guard rules, including `1 → 2 000 000 → 1.2 kWh`,
  boot baseline, re-anchor, Pmax override.
* **state machine** — `FakeTransport` + fake clock: full cycle, retry, double timeout,
  3-cycle offline, E03 L1 fallback, identity mismatch, `iterateAlways()` never waits.

Hardware on `pi-szn`, in order:

1. **Probe before writing C++**: dump L1/L2/L3 blocks and 50946–50955 on both meters with
   `extras/tools/modbus_probe.py`; save the dumps as test fixtures. This confirms the E03 L1 block,
   the +0x800 stride and the main + residual rule.
2. Run sd4linux with `--verbose`; compare every value with the LCDs.
3. 24 h soak under systemd: 0 guard rejections, energy Δ matches LCD Δ, < 0.1 % failed cycles.
4. Fault injection: unplug an adapter / remove mains from one meter → that channel offline
   within ~15 s, other unaffected, recovers without a restart.

ESP32: compile-verified only (`idf.py build` of the example) until a board is on the bus;
manual bring-up checklist in the README.

## Open items

* Lines #2/#3 layout and the E03 L1 block — settled by probe task 1.
* Word order (big-endian high word first) — confirmed in practice by the 2026-10-29 readings.
* Joining both meters on one bus requires readdressing one of them and a shared transport.

## Implementation outcome (2026-10-29)

The driver was built from this design and probed on live hardware. Three points changed
during implementation; they are recorded here rather than rewritten above, so the approved
design stays readable as a historical record.

* **A Modbus exception aborts the cycle.** The implemented state machine (and the plan's
  deliberate refinement) aborts the current cycle on an exception and counts it toward the
  offline threshold, with two carve-outs: the E03 line-1 block switches permanently to the
  totals, and a phase-sequence exception is ignored. The "that step fails, other steps go
  ahead" sentence in *Error handling* is superseded.
* **The energy cross-check is active-only with a relative tolerance.** The per-counter
  30 Wh check against both `50946` and `50770/50776` does not hold on this hardware: the
  probe showed the per-direction per-line reactive sums differ from the system reactive
  totals by 1.7–1.9 %, and the per-line active sums carry a static ~3 kWh
  installation-history offset (0.014 %). `energyMatchesTotals()` therefore checks forward
  and reverse *active* energy against `50946` with `max(30 Wh, 1 % of gross active)`;
  reactive energy is published from the line blocks without a totals cross-check, and the
  `50770/50776` kWh registers are not read.
* **Other probe findings now in the implementation:** the phase-sequence register (`51315`)
  returns `132` on E23 firmware 3.00 and is treated as unknown; line-block residuals jitter
  backwards by a few 0.1 Wh counts, so backward steps within 10 Wh are noise and published
  counters stay on the high-water value; the E03 answers the line-block reads with Modbus
  exception 2 (no per-line energy) and returns `0xFFFFFFFF` for the `50950` apparent-energy
  slot.

Evidence: [probe and hardware-verification report](../integrations/socomec-countis-probe-2026-10-29.md).
Operator documentation: [SUPLA driver setup](../integrations/socomec-countis-supla.md).

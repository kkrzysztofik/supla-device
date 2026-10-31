# Socomec COUNTIS E03/E23 — commissioning report

**Site:** Raspberry Pi 5 `pi-szn` (192.168.2.120), Debian 13 (trixie), kernel 6.18.50
**Goal:** verify that two Socomec electricity meters connected through two USB-RS485
adapters communicate correctly.
**Result:** **resolved — both meters read reliably.** The initial failure was an RS-485
polarity fault, not a settings or software fault.

Reported 2026-10-29. Diagnosis 2026-10-28, fix and verification 2026-10-29.

Companion documents:

* [socomec-countis-modbus.md](socomec-countis-modbus.md) — Modbus register reference
  (addresses, types, scale factors, model differences)
* [socomec-countis-config.md](socomec-countis-config.md) — front-panel configuration
  field guide (terminal pinouts, button sequences, Modbus write fallback)

---

## 1. Hardware under test

| Item | Detail |
| --- | --- |
| Host | RPi 5, `pi-szn`, Debian 13, kernel 6.18.50+rpt-rpi-2712, aarch64 |
| Adapter 1 | FTDI FT232R, USB serial `BG03FMMW`, bus `1-1` → `/dev/ttyUSB0` |
| Adapter 2 | FTDI FT232R, USB serial `BG041JVI`, bus `3-1` → `/dev/ttyUSB1` |
| Meter on adapter 1 | **COUNTIS E03** (single-phase, ref 4850 3039), identity verified over Modbus |
| Meter on adapter 2 | **COUNTIS E23** (three-phase 80 A, ref 4850 3050), identity verified over Modbus |

Stable device paths (survive re-plugging, use these in configuration):

```
/dev/serial/by-id/usb-FTDI_FT232R_USB_UART_BG03FMMW-if00-port0  -> ttyUSB0  (E03)
/dev/serial/by-id/usb-FTDI_FT232R_USB_UART_BG041JVI-if00-port0  -> ttyUSB1  (E23)
```

Both adapters sit on separate USB buses, and each meter has its own RS-485 bus. The two
buses do not see each other's traffic (verified before the fix).

Both meters are **self-powered** from their voltage inputs — with no mains on L1/L2/L3
they are completely dead on the bus.

---

## 2. Symptom

Neither meter answered Modbus, on either adapter. Critical detail: there was **not a
single byte** received — no reply, and no Modbus exception either. An exception would
already have proven that the meter heard the request.

## 3. What was ruled out

The following were tested before touching the wiring, all with zero replies:

| Dimension | Coverage |
| --- | --- |
| Baud rate | 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200 |
| Parity | none, even, odd |
| Stop bits | 1 and 2 |
| Slave address | 1–247, full range, for every baud/parity combination |
| Reply timeout | raised to 1.5 s (excludes slow-responding meters) |
| RS-485 direction control | auto-direction, RTS high/low, DTR high/low (excludes DE/RE wired to a modem line) |
| Idle line | 2 s listen per port — no noise, no break, no framing errors |
| Adapter sanity | both enumerate, driver loads cleanly, no `ftdi_sio` errors, ports open/close at all settings |

Both meters were confirmed powered (LCDs lit). The host key of `192.168.2.120` had also
changed since the last known connection — consistent with the box having been reflashed,
but noted here because it would otherwise look like an impersonation.

## 4. Root cause

**RS-485 polarity.** Two independent traps combined:

1. **The terminal numbering is mirrored between the two models.**

   | | pin 1 | pin 2 | pin 3 |
   | --- | --- | --- | --- |
   | COUNTIS E03 | **+** | **−** | NC / shield |
   | COUNTIS E23 | NC / shield | **−** | **+** |

   Wiring both meters "by terminal number" from one drawing reverses the pair on one of
   them. A reversed pair is totally silent — no reply, no exception.

2. **`A`/`B` convention mismatch.** Socomec prints `+`/`−` and never `A`/`B`. Following
   FENECON's commissioning guide for this meter, `+` = A = D+ and `−` = B = D−, whereas
   the TIA/EIA-485 convention (used by many USB-RS485 dongles) calls the *inverting*
   line A. "A to A, B to B" can therefore be exactly wrong.

Fix applied on site: **the RS-485 pair was swapped at the adapter**, and the meters were
configured from their front panels. Communication came up immediately.

## 5. Final configuration

| Setting | E03 | E23 |
| --- | --- | --- |
| Slave address | **2** | **2** |
| Baud rate | 9600 | 9600 |
| Parity | none | none |
| Stop bits | not settable — fixed 1 | 1 |
| Data bits | fixed 8 | fixed 8 |

Both meters ended up on address 2 (nothing answers at address 1). That is harmless
because each meter is alone on its own bus; **it will collide if the two buses are ever
joined.**

## 6. Verification

Identity read over Modbus confirmed the expected models (`extended name` = `Countis E03`
and `Countis E23`), firmware **3.00** on both.

Live readings, both meters in service:

| Quantity | COUNTIS E03 (`ttyUSB0`) | COUNTIS E23 (`ttyUSB1`) |
| --- | --- | --- |
| V1 | 238.91 V | 240.09 V (V2 240.97 V, V3 239.77 V) |
| Frequency | 50.000 Hz | 50.030 Hz |
| I1 | 2.417 A | 2.744 A (I2 0.434 A, I3 0.464 A, In 2.855 A) |
| P (active power) | 470 W | 570 W |
| Q (reactive power) | −330 var | −570 var |
| S (apparent power) | 600 VA | 870 VA |
| PF | −0.773 | −0.652 |
| Ea+ (import, lifetime) | 4846 kWh | 20744 kWh |
| Ea− (export, lifetime) | 5507 kWh | 24106 kWh |

The readings are mutually consistent, not merely well-formed bytes: on both meters the
reported PF agrees with the meter's own P/S (E03 0.783 vs 0.773, E23 0.655 vs 0.652), and
the E03's phase-2/phase-3 registers return `0xFFFF`, correctly marking it as single-phase.

**Reliability:** 30 consecutive polls per meter, 0 failures.
Latency per 2-register poll at 9600 baud: ≈62 ms (E03), ≈47 ms (E23).

## 7. Findings that affect the driver implementation

1. **The frequency scale differs between the models**, at both the 32-bit register
   (`50526`) and the 16-bit mirror (`51287`):
   * E03: LSB **0.01 Hz** — raw `5000` = 50.000 Hz
   * E23: LSB **0.001 Hz** — raw `50030` = 50.030 Hz

   Sharing one scale between the models reads 5.000 Hz on the E03 (or 500 Hz on the E23).
2. **Absent phases on the E03 return `0xFFFF` / `0xFFFFFFFF`** (V2, V3, I2, I3, In). Treat
   as "not available", never as a measurement.
3. **`S` is the arithmetic sum of per-phase apparent powers**, so on an unbalanced load
   it legitimately exceeds √(P²+Q²) — 870 vs 806 VA here. Not a fault; do not "correct" it.
4. **Retry on timeout.** One back-to-back request, issued immediately after a 66-register
   block read, timed out once on the E03. A ≥100 ms inter-frame gap, or a single retry,
   made it 100 % reliable. The driver should retry rather than treat a timeout as a dead
   meter.
5. **Energy registers are kWh-LSB**, not Wh, despite the tables' `Wh / 0.001` wording:
   raw `50770` = 4846 means 4846 kWh. See the register reference.

## 8. Current deployment state

The Pi is still a **bare Debian 13 installation**:

* no SUPLA software installed (no binary, no systemd unit, no container),
* no other serial or Modbus service running,
* `python3-serial` was installed only to run the diagnostic scripts.

So the meters are proven readable, but nothing is publishing them yet.

## 9. Next steps

1. Build `sd4linux` for `pi-szn` (`extras/examples/linux/`) and run it as a systemd unit.
2. Map both meters to SUPLA `ElectricityMeter` channels: the E03 with phase-1 data only
   (V2/V3/I2/I3 must be suppressed, not reported as 655.35 V), the E23 with all three
   phases.
3. Poll over the stable `/dev/serial/by-id/...` paths, not `ttyUSB*` — the enumeration
   order is not guaranteed across re-plugs.
4. Use 9600 8N1, per-meter address 2, one retry on timeout, and a poll interval of a few
   seconds (the E23 needs ≈47 ms, the E03 ≈62 ms per poll).

## 10. Artefacts

Diagnostic tools kept in this repository:

| File | Purpose |
| --- | --- |
| [`extras/tools/modbus_probe.py`](../../extras/tools/modbus_probe.py) | baud/parity/address discovery, identity read, raw register read |
| [`extras/tools/wiring_check.py`](../../extras/tools/wiring_check.py) | physical-layer checks: modem lines, cross-bus crosstalk, idle noise |
| [`extras/tools/direction_probe.py`](../../extras/tools/direction_probe.py) | manual RS-485 direction-control probing (RTS/DTR variants) |

Ad-hoc read/decode scripts used for the measurements above remain on the Pi under
`/root/` (`read_meters.py`, `read_e03.py`, `reliability.py`, `mirror_check.py`); they are
throwaway and should be superseded by the driver.

## 11. Open items and caveats

* **Factory defaults remain unconfirmed** for both models. Socomec publishes no default
  column and marks defaults only as an `XX` vector-art glyph in the manuals. Read the
  settings off the display rather than assuming them.
* **Whether a settings change self-commits or needs a power cycle** is not documented.
  Over Modbus, register `57856 = 0xA1` is the documented "store configuration" action.
* **The exact E23 key mapping** is taken from the FENECON guide plus manual §9.1.1; if
  `ENTER` > 3 s fails to open a programming screen on a unit, re-identify the model
  (the older 4850 3035 generation has a different 7-pin terminal map).
* **Host key of 192.168.2.120 changed** during this work; the stale entry was bypassed
  without modifying `~/.ssh/known_hosts`. Investigate if the box was not deliberately
  reflashed.

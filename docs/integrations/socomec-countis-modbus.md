# Socomec COUNTIS E03 / E23 — Modbus register reference

Distilled from Socomec's published MODBUS communication tables (file date 2017-08,
table version 1.1) for the single-phase **COUNTIS E03** (ref. 4850 3039) and the
three-phase **COUNTIS E23** (ref. 4850 3050). Only the registers that matter for an
energy-meter integration are reproduced; the source documents also carry per-tariff
reactive energies, residual/overflow counters, reset commands and production-test
registers.

## Conventions used by the Socomec tables

**Addressing — 0-based raw PDU address.** Every row gives a decimal and a hex address
that are numerically identical (`50000` = `0xC350`, `36865` = `0x9001`). That number is
what goes into the Modbus PDU: do **not** add 40001, and do **not** apply the
"register number − 1" rule. To read V1 on an E23: FC 03, start `50520`, 2 registers.

**Word count / type.** `U8`/`U16`/`S16` = 1 register, `U32`/`S32` = 2 registers,
`U64` = 4, `STRING_16` = 4, `STRING_NORM` = 8. Function codes per the E23/E24
instruction manual §8.3: `03` read words (max 128 per request), `06` write one word,
`16` write several words (max 128). Broadcast is supported for tariff log writes.

**Scale.** The "Unit" column reads `unit / N`, meaning the value is `raw ÷ N` in that
unit — the LSB is `1/N`:

| Table text | LSB | Multiply raw by |
| --- | --- | --- |
| `V / 100` | 0.01 V | 0.01 |
| `A / 1000` | 0.001 A | 0.001 |
| `Hz / 1000` | 0.001 Hz | 0.001 |
| `- / 1000` (PF) | 0.001 | 0.001 |
| `W / 0.1` (also `var`, `VA`) | 10 W | 10 |
| `Wh / 0.001` | 1 kWh | 1000 (→ Wh) |
| `Wh / 0.1` | 10 Wh | 10 |
| `Wh / 10` (U16 residuals) | 0.1 Wh | 0.1 |

The `Wh / 0.001` energy registers (`50770`…) are therefore **kWh counters, not Wh** —
the usual factor-1000 mistake with these meters. Confirmed against two independent
implementations: a Loxone template reads `50770` raw as kWh and `50536` as `raw/100`
kW; the aggsoft register listing publishes ×1000 for `50770` and ×10 for `50536`.

**Word order (uncertain).** 32-bit values span two consecutive registers. The Socomec
tables do not state the order; the public register-map libraries for these meters
declare big-endian byte *and* word order (high word at the lower address). Treat as
expected-but-unverified on hardware.

**Sign conventions.** The tables mark power/PF as `+/-`, `-: leading`, `+ : lagging`.
P is positive for import, negative for export (no explicit statement — inferred from
the `+/-` marking plus the separate Ea+/Ea− counters; verify against the display).

## COUNTIS E03 (single-phase, direct, ref. 4850 3039)

The E03 is a single-phase meter: only `V1`, `I1` and phase-1 power registers exist.
`U12/U23/U31`, `V2/V3`, `I2/I3` and neutral current are absent or reserved in the map.

### Identification

| Addr | Hex | W | Type | × | Unit | Description |
| --- | --- | --- | --- | --- | --- | --- |
| 50004 | 0xC354 | 1 | U16 | 1 | – | Product order ID (100 = Countis, 200 = Protection, 300 = Atys, 400 = Diris) |
| 50005 | 0xC355 | 1 | U16 | 1 | – | Product ID |
| 50006 | 0xC356 | 1 | U16 | 1 | – | JBUS table version (e.g. 101 = v1.01) |
| 50007 | 0xC357 | 1 | U16 | 1 | – | Product software version (e.g. 100 = v1.00) |
| 50008 | 0xC358 | 3 | U16 hex | 1 | – | Serial number fields: `Serial_AA_SS` (50008), `Serial_SST_L` (50009), `Serial_order` (50010). Packing is **not documented** → treat the assembled serial as uncertain |
| 50042 | 0xC37A | 8 | STRING | – | – | Vendor name (`"SOCOMEC"`) |
| 50050 | 0xC382 | 8 | STRING | – | – | Product name |
| 50058 | 0xC38A | 8 | STRING | – | – | Extended name |
| 50030 | 0xC36E | 4 | STRING | – | – | Product VLO (e.g. `"880100"`) |
| 50018 | 0xC362 | 1 | U16 | 1 | – | Firmware version major (50019 = minor) |

### Measurement

| Addr | Hex | W | Type | × | Unit | Description |
| --- | --- | --- | --- | --- | --- | --- |
| 50520 | 0xC558 | 2 | U32 | 0.01 | V | Simple voltage V1 |
| 50526 | 0xC55E | 2 | U32 | **0.01** | Hz | Frequency F — **hardware-verified on an E03: raw 5000 = 50.000 Hz**. The E23 uses 0.001 for the same register; do not share the scale between models |
| 50528 | 0xC560 | 2 | U32 | 0.001 | A | Current I1 |
| 50536 | 0xC568 | 2 | S32 | 10 | W | Sum active power P (+/−) |
| 50538 | 0xC56A | 2 | S32 | 10 | var | Sum reactive power Q (+/−) |
| 50540 | 0xC56C | 2 | U32 | 10 | VA | Sum apparent power S |
| 50542 | 0xC56E | 2 | S32 | 0.001 | – | Power factor PF (− leading, + lagging) |
| 50544 | 0xC570 | 2 | S32 | 10 | W | Active power phase 1 P1 |
| 50550 | 0xC576 | 2 | S32 | 10 | var | Reactive power phase 1 Q1 |
| 50556 | 0xC57C | 2 | U32 | 10 | VA | Apparent power phase 1 S1 |
| 50562 | 0xC582 | 2 | S32 | 0.001 | – | Power factor phase 1 PF1 |
| 51284 | 0xC854 | 1 | U16 | 0.01 | V | V1, 16-bit variant |
| 51287 | 0xC857 | 1 | U16 | **0.01** | Hz | Frequency, 16-bit variant. Verified: E03 returns `5000` = 50.000 Hz at ×0.01; the same address on the E23 returns `50040` = 50.040 Hz at ×0.001 — same model split as the 32-bit register |
| 51295 | 0xC85F | 1 | S16 | 0.001 | – | PF, 16-bit variant |
| 40576 | 0x9E80 | 2 | U32 | 0.001 | A | Current I1 without CT (raw current path) |

### Energies

All `U32` energy registers hold two words. "No resetable" = lifetime counter;
"Partial" = resettable.

| Addr | Hex | W | Type | × | Unit | Description |
| --- | --- | --- | --- | --- | --- | --- |
| 50770 | 0xC652 | 2 | U32 | 1000 | Wh | **Total positive active energy Ea+ (import), non-resettable** — raw value is kWh |
| 50776 | 0xC658 | 2 | U32 | 1000 | Wh | Total negative active energy Ea− (export), non-resettable |
| 50772 | 0xC654 | 2 | U32 | 1000 | varh | Total positive reactive energy Er+ |
| 50778 | 0xC65A | 2 | U32 | 1000 | varh | Total negative reactive energy Er− |
| 50774 | 0xC656 | 2 | U32 | 1000 | VAh | Total apparent energy Eap |
| 50780 | 0xC65C | 2 | U32 | 1000 | Wh | Partial positive active energy Ea+ (resettable) |
| 50782 | 0xC65E | 2 | U32 | 1000 | varh | Partial positive reactive energy Er+ |
| 50786 | 0xC662 | 2 | U32 | 1000 | Wh | Partial negative active energy Ea− |
| 50788 | 0xC664 | 2 | U32 | 1000 | varh | Partial negative reactive energy Er− |
| 50946 | 0xC702 | 2 | U32 | 10 | Wh | Total Ea+ in the coarser "Unit/100" block (10 Wh LSB, ~42.9 GWh span) |
| 50948 | 0xC704 | 2 | U32 | 10 | varh | Total Er+ in the same fine block |
| 50950 | 0xC706 | 2 | U32 | 10 | VAh | Total Eap in the same fine block — **absent (`0xFFFFFFFF`) on the probed E03** |
| 50952 | 0xC708 | 2 | U32 | 10 | Wh | Total Ea− in the same fine block |
| 50954 | 0xC70A | 2 | U32 | 10 | varh | Total Er− in the same fine block |
| 50850 | 0xC6A2 | 2 | U32 | 1000 | Wh | Positive active energy, tariff 1 |
| 50852 | 0xC6A4 | 2 | U32 | 1000 | Wh | Positive active energy, tariff 2 |
| 50866 | 0xC6B2 | 2 | U32 | 1000 | varh | Positive reactive energy, tariff 1 |
| 50868 | 0xC6B4 | 2 | U32 | 1000 | varh | Positive reactive energy, tariff 2 |
| 50882 | 0xC6C2 | 2 | U32 | 0.01 | h | Time counter, tariff 1 |
| 50884 | 0xC6C4 | 2 | U32 | 0.01 | h | Time counter, tariff 2 |
| 26368 | 0x6700 | 1 | U16 | 1 | – | Tariff in progress (1 or 2) |
| 26369 | 0x6701 | 2 | U32 | 1000 | Wh | **T1 — Ea+** (tariff meter block) |
| 26371 | 0x6703 | 2 | U32 | 1000 | Wh | **T2 — Ea+** |
| 26385 | 0x6711 | 2 | U32 | 1000 | Wh | T1 — Ea− |
| 26387 | 0x6713 | 2 | U32 | 1000 | Wh | T2 — Ea− |
| 26401 | 0x6721 | 2 | U32 | 1000 | varh | T1 — Er+ |
| 26403 | 0x6723 | 2 | U32 | 1000 | varh | T2 — Er+ |
| 36868 | 0x9004 | 2 | U32 | 1 | Wh | Energy index (plain `Wh`, 1 Wh LSB) — pulse/LED index, not the same object as Ea+ |

The E03 shares the `50946–50955` layout with the E23 (one 10-register read covers all five
U32 totals); on the probed unit only the `Eap` slot returned `0xFFFFFFFF`. The driver reads
this block for the E03 because the E03 has no per-line energy blocks.

The tariff count and current tariff position are also exposed at 50848 (U8, "1 ≥ tariff
number ≤ 2") and 50849 (U8, tariff in progress). Tariff can be forced by writing
40481 (U8, "tariff number to set").

### Configuration / control (E03)

| Addr | Hex | W | Type | Description |
| --- | --- | --- | --- | --- |
| 36865 | 0x9001 | 1 | U16 | RS485 address, 1–247 |
| 36866 | 0x9002 | 1 | U16 | Baud rate code (see below) |
| 36867 | 0x9003 | 1 | U16 | Parity code (0 none, 1 odd, 2 even) |
| 40512 | 0x9E40 | 1 | U8 | Reset partial energies (`0x20` = partial energies) |
| 40544 | 0x9E60 | 1 | U8 | Indus mode flag (40545 = MID status: 0 non-MID, 1 MID) |
| 40608 | 0x9EA0 | 1 | U8 | COM board product ID (`0x01` = E3x/E4x board, `0x02` = E63 board) |
| 57856 | 0xE200 | 1 | U8 | Action: `0xA1` store configuration, `0xB2` product reset |

## COUNTIS E23 (three-phase, direct 80 A, ref. 4850 3050)

Same map as the E24; differs from the E03 mainly by the full three-phase/neutral
block and by having its RS485 settings in the `0xE0xx` block instead of `0x900x`.

### Identification

Identical layout to the E03: 50004 order ID, 50005 product ID, 50006 JBUS table
version, 50007 software version, 50008–50010 serial fields (same undocumented packing),
50042 vendor name, 50050 product name, 50018/50019 version major/minor.

### Measurement

| Addr | Hex | W | Type | × | Unit | Description |
| --- | --- | --- | --- | --- | --- | --- |
| 50514 | 0xC552 | 2 | U32 | 0.01 | V | Phase-to-phase U12 |
| 50516 | 0xC554 | 2 | U32 | 0.01 | V | Phase-to-phase U23 |
| 50518 | 0xC556 | 2 | U32 | 0.01 | V | Phase-to-phase U31 |
| 50520 | 0xC558 | 2 | U32 | 0.01 | V | **Simple voltage V1** |
| 50522 | 0xC55A | 2 | U32 | 0.01 | V | **V2** |
| 50524 | 0xC55C | 2 | U32 | 0.01 | V | **V3** |
| 50526 | 0xC55E | 2 | U32 | 0.001 | Hz | Frequency F |
| 50528 | 0xC560 | 2 | U32 | 0.001 | A | **Current I1** |
| 50530 | 0xC562 | 2 | U32 | 0.001 | A | **I2** |
| 50532 | 0xC564 | 2 | U32 | 0.001 | A | **I3** |
| 50534 | 0xC566 | 2 | U32 | 0.001 | A | Neutral current In |
| 50536 | 0xC568 | 2 | S32 | 10 | W | **Sum active power P** (+/−) |
| 50538 | 0xC56A | 2 | S32 | 10 | var | Sum reactive power Q (+/−) |
| 50540 | 0xC56C | 2 | U32 | 10 | VA | Sum apparent power S |
| 50542 | 0xC56E | 2 | S32 | 0.001 | – | **Power factor PF** (− leading, + lagging) |
| 50544 | 0xC570 | 2 | S32 | 10 | W | Active power P1 |
| 50546 | 0xC572 | 2 | S32 | 10 | W | Active power P2 |
| 50548 | 0xC574 | 2 | S32 | 10 | W | Active power P3 |
| 50550 | 0xC576 | 2 | S32 | 10 | var | Reactive power Q1 |
| 50552 | 0xC578 | 2 | S32 | 10 | var | Reactive power Q2 |
| 50554 | 0xC57A | 2 | S32 | 10 | var | Reactive power Q3 |
| 50556 | 0xC57C | 2 | U32 | 10 | VA | Apparent power S1 |
| 50558 | 0xC57E | 2 | U32 | 10 | VA | Apparent power S2 |
| 50560 | 0xC580 | 2 | U32 | 10 | VA | Apparent power S3 |
| 50562 | 0xC582 | 2 | S32 | 0.001 | – | Power factor PF1 |
| 50564 | 0xC584 | 2 | S32 | 0.001 | – | Power factor PF2 |
| 50566 | 0xC586 | 2 | S32 | 0.001 | – | Power factor PF3 |
| 50568 | 0xC588 | 2 | U32 | 0.001 | A | System current I Sys = (I1+I2+I3)/3 |
| 50570 | 0xC58A | 2 | U32 | 0.01 | V | System voltage U Sys = (U12+U23+U31)/3 |
| 50572 | 0xC58C | 2 | U32 | 0.01 | V | System voltage V Sys = (V1+V2+V3)/3 |
| 51315 | 0xC873 | 1 | U16 | 1 | – | Phase sequence: 0 = 123 CW, 1 = 132 CCW, 3 = ND |

16-bit mirrors of the same quantities exist in the `0xC85x` block (51281–51310: U12,
U23, U31, V1, V2, V3, F, PF, PF1–PF3, I/U/V Sys) — single-register reads, lower
resolution.

### Energies

| Addr | Hex | W | Type | × | Unit | Description |
| --- | --- | --- | --- | --- | --- | --- |
| 50770 | 0xC652 | 2 | U32 | 1000 | Wh | **Total positive active energy Ea+ (import), non-resettable** — raw is kWh |
| 50776 | 0xC658 | 2 | U32 | 1000 | Wh | Total negative active energy Ea− (export), non-resettable |
| 50772 | 0xC654 | 2 | U32 | 1000 | varh | Total positive reactive energy Er+ |
| 50778 | 0xC65A | 2 | U32 | 1000 | varh | Total negative reactive energy Er− |
| 50774 | 0xC656 | 2 | U32 | 1000 | VAh | Total apparent energy Eap |
| 50780 | 0xC65C | 2 | U32 | 1000 | Wh | Partial positive active energy Ea+ (resettable) |
| 50782 | 0xC65E | 2 | U32 | 1000 | varh | Partial positive reactive energy Er+ |
| 50786 | 0xC662 | 2 | U32 | 1000 | Wh | Partial negative active energy Ea− |
| 50788 | 0xC664 | 2 | U32 | 1000 | varh | Partial negative reactive energy Er− |
| 50946 | 0xC702 | 2 | U32 | 10 | Wh | Total Ea+ in the coarser "Unit/100" block |
| 33024 | 0x8100 | 2 | U32 | 1000 | Wh | Net active energy balance = Σ(Ea+ − Ea−) by tariff |
| 32768 | 0x8000 | 2 | U32 | 1000 | Wh | **Partial Ea+ tariff 1** |
| 32771 | 0x8003 | 2 | U32 | 1000 | Wh | **Partial Ea+ tariff 2** |
| 32792 | 0x8018 | 2 | U32 | 1000 | Wh | Partial Ea− tariff 1 |
| 32795 | 0x801B | 2 | U32 | 1000 | Wh | Partial Ea− tariff 2 |
| 50850 | 0xC6A2 | 2 | U32 | 1000 | Wh | Positive active energy, tariff 1 |
| 50852 | 0xC6A4 | 2 | U32 | 1000 | Wh | Positive active energy, tariff 2 |
| 50866 | 0xC6B2 | 2 | U32 | 1000 | varh | Positive reactive energy, tariff 1 |
| 50868 | 0xC6B4 | 2 | U32 | 1000 | varh | Positive reactive energy, tariff 2 |
| 50882 | 0xC6C2 | 2 | U32 | 0.01 | h | Time counter, tariff 1 |
| 50884 | 0xC6C4 | 2 | U32 | 0.01 | h | Time counter, tariff 2 |
| 26368 | 0x6700 | 1 | U16 | 1 | – | Tariff in progress (1 or 2) |
| 26369 | 0x6701 | 2 | U32 | 1000 | Wh | **T1 — Ea+** (total, tariff meter block) |
| 26371 | 0x6703 | 2 | U32 | 1000 | Wh | **T2 — Ea+** |
| 26385 | 0x6711 | 2 | U32 | 1000 | Wh | T1 — Ea− |
| 26387 | 0x6713 | 2 | U32 | 1000 | Wh | T2 — Ea− |
| 26497 | 0x6781 | 2 | U32 | 1000 | VAh | T1 — Eap |
| 26499 | 0x6783 | 2 | U32 | 1000 | VAh | T2 — Eap |

Per-line (`Line #1/#2/#3`) energy blocks also exist at 0x4D80, 0x5580, 0x5D80 with
main `U32` + `U16` residual pairs (`Wh / 0.001` + `Wh / 10`), and the E23 additionally
exposes meter-overflow handling at 0x9000: 36864 = overflow value (10 Wh), 36866–36873
= number of overflows per energy direction.

### Configuration / control (E23)

| Addr | Hex | W | Type | Description |
| --- | --- | --- | --- | --- |
| 57344 | 0xE000 | 1 | U8 | Slave address (`USER` lock level) |
| 57345 | 0xE001 | 1 | U16 | Baud rate code |
| 57346 | 0xE002 | 1 | U16 | Stop bits: 0 = 1, 1 = 2 |
| 57347 | 0xE003 | 1 | U16 | Parity: 0 = none, 1 = odd, 2 = even |
| 40481 | 0x9E21 | 1 | U8 | Tariff number to set |
| 40512 | 0x9E40 | 1 | U8 | Reset partial energies (`0x20`) |
| 40544 | 0x9E60 | 1 | U8 | Indus mode (40545 = MID status) |
| 40608 | 0x9EA0 | 1 | U8 | COM board product ID |
| 57856 | 0xE200 | 1 | U8 | Action: `0xA1` store config, `0xB2` product reset |
| 57890 | 0xE222 | 1 | U16 | Reset: `0x0001` partial Ea+, `0x0002` partial Er+, `0x0004` partial Eap, `0x0008` partial Ea−, `0x0010` partial Er− |
| 57892 | 0xE224 | 1 | U16 | Reset tariff counters: `0x0001` T1, `0x0002` T2, `0xFFFF` all |

## Serial / communication settings

Both meters expose their RS485 parameters as ordinary holding registers. The
**encodings are documented; the factory defaults are not.**

### COUNTIS E03

| Register | Hex | Description | Values |
| --- | --- | --- | --- |
| 36865 | 0x9001 | RS485 address | 1–247 |
| 36866 | 0x9002 | Baud rate | 0 = 1200, 1 = 2400, 2 = 4800, 3 = 9600, 4 = 19200, 5 = 38400 |
| 36867 | 0x9003 | Parity | 0 = none, 1 = odd, 2 = even |

The E03 table has **no stop-bit register and no data-bit register** (three settings
registers total): the frame is fixed at 8 data bits and the stop-bit count is not
configurable per this document.

### COUNTIS E23

| Register | Hex | Description | Values |
| --- | --- | --- | --- |
| 57344 | 0xE000 | Slave address | 1–247 |
| 57345 | 0xE001 | Baud rate | 0 = 1200, 1 = 2400, 2 = 4800, 3 = 9600, 4 = 19200, 5 = 38400, 6 = 57600, 7 = 115200 |
| 57346 | 0xE002 | Stop bits | 0 = 1 stop bit, 1 = 2 stop bits |
| 57347 | 0xE003 | Parity | 0 = none, 1 = odd, 2 = even |

`57344` is marked lock level `USER` (writable); `57345`–`57347` share the `USER`
settings block and accept `WRITE` / `WRITE_MANY`.

### Defaults — not stated (uncertain)

* Neither communication table documents a factory default for baud rate, parity,
  stop bits or slave address. There is no "default" column and no "factory settings"
  prose in either document.
* The only hard numbers are the **supported address range 1–247** (stated in the E03
  table field description and shown in the E23/E24 instruction manual's SETUP 2 menu:
  `1, 2, ... 5, ..., 246, 247`).
* The E23/E24 instruction manual (545 877 A) presents its defaults as graphical
  "XX = default value" marks over the SETUP 2 values; those marks do not survive text
  extraction, so the default baud/parity/stop combination could not be read off
  reliably. A third-party register map independently notes the same gap ("the factory
  RS485 settings are not given in text form; check SETUP 2 on the display").
* Practical rule: do not assume 9600 8N1 / address 1. Read 36865–36867 (E03) or
  57344–57347 (E23) at the current bus settings, or read SETUP 2 on the display, before
  configuring a driver's defaults.
* Data bits are 8: the manual describes RTU frames "composed of at least 8 bits".

## Per-line energy blocks (E23)

Each line has a 66-register `Info` block (lock level `NONE`, `READ` only). Line #1 at
`19840` / `0x4D80` is transcribed from the E23 communication table (V103.18); Line #2
(`0x5580`) and Line #3 (`0x5D80`) share the layout at +0x800 / +0x1000.

All three blocks were read and decoded on live hardware on 2026-10-29
([probe report](socomec-countis-probe-2026-10-29.md)): E23 per-line values are
self-consistent and their active sums track the system totals. The E03 answers all three
reads with **Modbus exception 2 (illegal data address)** — it has no per-line energy
block, so the driver uses the `50946` totals for the E03.

| Offset | Dec (L1) | Hex (L1) | W | Type | Unit | Description |
| --- | --- | --- | --- | --- | --- | --- |
| +0 | 19840 | 0x4D80 | 1 | – | – | Reserved |
| +1 | 19841 | 0x4D81 | 2 | – | – | Reserved |
| +3 | 19843 | 0x4D83 | 2 | U32 | Wh / 0.001 | Total positive active energy Ea+ |
| +5 | 19845 | 0x4D85 | 1 | U16 | Wh / 10 | Residual rEa+ |
| +6 | 19846 | 0x4D86 | 2 | U32 | Wh / 0.001 | Total negative active energy Ea− |
| +8 | 19848 | 0x4D88 | 1 | U16 | Wh / 10 | Residual rEa− |
| +9 | 19849 | 0x4D89 | 2 | U32 | varh / 0.001 | Total positive reactive energy Er+ |
| +11 | 19851 | 0x4D8B | 1 | U16 | varh / 10 | Residual rEr+ |
| +12 | 19852 | 0x4D8C | 2 | U32 | varh / 0.001 | Total negative reactive energy Er− |
| +14 | 19854 | 0x4D8E | 1 | U16 | varh / 10 | Residual rEr− |
| +15 | 19855 | 0x4D8F | 2 | U32 | VAh / 0.001 | Total apparent energy Eap |
| +17 | 19857 | 0x4D91 | 1 | U16 | VAh / 10 | Residual rEap |
| +18 | 19858 | 0x4D92 | 2 | U32 | varh / 0.001 | Er+ (lagging) |
| +20 | 19860 | 0x4D94 | 1 | U16 | varh / 10 | Residual rEr+ (lagging) |
| +21 | 19861 | 0x4D95 | 2 | U32 | varh / 0.001 | Er− (lagging) |
| +23 | 19863 | 0x4D97 | 1 | U16 | varh / 10 | Residual rEr− (lagging) |
| +24 | 19864 | 0x4D98 | 2 | U32 | varh / 0.001 | Er+ (leading) |
| +26 | 19866 | 0x4D9A | 1 | U16 | varh / 10 | Residual rEr+ (leading) |
| +27 | 19867 | 0x4D9B | 2 | U32 | varh / 0.001 | Er− (leading) |
| +29 | 19869 | 0x4D9D | 1 | U16 | varh / 10 | Residual rEr− (leading) |

Main counter has a 1 kWh LSB, residual a 0.1 Wh LSB. The combination rule is not
stated; the working assumption is `Wh = main × 1000 + residual × 0.1`, which implies a
valid residual is `0…9999`. Σ(L1..L3) must agree with the 10 Wh totals below.

### 10 Wh totals block

From the third-party `mbmap` E24 map (citing Socomec table V103.8) — five contiguous
U32 totals at 0.01 kWh LSB, one 10-register read:

| Dec | Hex | Description |
| --- | --- | --- |
| 50946 | 0xC702 | Total Ea+ |
| 50948 | 0xC704 | Total Er+ |
| 50950 | 0xC706 | Total Eap |
| 50952 | 0xC708 | Total Ea− |
| 50954 | 0xC70A | Total Er− |

50956–50965 hold the partial (resettable) equivalents in the same order.

## Hardware verification (2026-10-29)

Verified live on the RPi5 (`pi-szn`) with two FTDI FT232 USB-RS485 adapters, one meter
per bus: **9600 8N1**, both meters at **slave address 2**, firmware **3.00**, 30/30 polls
without a failure (≈62 ms per poll on the E03, ≈47 ms on the E23).

| Quantity | E03 (`/dev/ttyUSB0`) | E23 (`/dev/ttyUSB1`) |
| --- | --- | --- |
| V1 | 238.91 V | 240.09 V (V2 240.97 V, V3 239.77 V) |
| F | 50.000 Hz (raw 5000, LSB 0.01) | 50.030 Hz (raw 50030, LSB 0.001) |
| I1 | 2.417 A | 2.744 A (I2 0.434 A, I3 0.464 A, In 2.855 A) |
| P | 470 W | 570 W |
| Q | −330 var | −570 var |
| S | 600 VA | 870 VA |
| PF | −0.773 | −0.652 |
| Ea+ | 4846 kWh | 20744 kWh |
| Ea− | 5507 kWh | 24106 kWh |

What the hardware run established:

* **The frequency LSB differs by model.** E03 raw `5000` = 50.000 Hz (×0.01); E23 raw
  `50030` = 50.030 Hz (×0.001). Applying the E23 scale to the E03 reads 5.000 Hz.
* **Absent phases on the E03 read `0xFFFF` / `0xFFFFFFFF`** (V2, V3, I2, I3, In). Treat
  those as "not available", never as a measurement.
* **`S` is the arithmetic sum of per-phase apparent powers**, so on the unbalanced E23 it
  is larger than √(P²+Q²) (870 vs 806 VA). PF agrees with the meter's own P/S
  (0.655 measured vs −0.652 reported), so the registers are self-consistent.
* **Both meters answered at address 2; nothing answered at address 1.** Addresses are
  per-bus, so this works, but record it if the two buses are ever joined.
* **One back-to-back request timed out on the E03** immediately after a 66-register block
  read. With a ≥100 ms inter-frame gap (or a single retry) it was 100 % reliable — the
  driver should retry on timeout rather than treat it as a dead meter.

## Notes and gotchas

* **Same address, different meaning.** `36868` is the *energy index* (U32, Wh) on the
  E03 but *number of Ea− overflows* (U8) on the E23. Do not share that constant.
* **Energy resolution.** `50770` has a 1 kWh LSB (U32, ~4.29 TWh span): fine for a
  lifetime total, coarse for incremental readings. `50946` gives a 10 Wh LSB
  (~42.9 GWh span), and the 0x4D8x/0x558x/0x5D8x blocks add a `U16` residual (0.1 Wh)
  for sub-kWh detail. The E23's overflow counters (36866–36873) relate to the meter's
  internal counting; how to apply them is not documented here.
* **Duplicate representations.** Total Ea+ appears in at least three places
  (0x4D83, 0xC652, 0xC702) with different scales, plus per-tariff copies
  (0x6701/0x6703, 0xC6A2/0xC6A4). Pick one and stay consistent.
* **Suspect units.** The U16 energy registers at 51311–51314 (and 0xC86F region) are
  labelled `Wh / 1E-06`, which is physically implausible for a 16-bit counter. Treat
  those four registers as undocumented/broken and do not use them.
* **Residual counters.** `U16` "Residual … Energy" registers (`Wh / 10` = 0.1 Wh LSB)
  sit next to the `U32` totals in the 0x4D80/0x5580/0x5D80/0x6580 blocks; the tables
  do not explain how to combine them with the main counter, so their use is uncertain.
* **E03 capabilities.** No `V2/V3`, `I2/I3`, neutral current, phase-to-phase voltages,
  or system averages — that is a hardware difference, not a documentation gap.
* The E03 supports 2 tariffs; the tariff source is configurable via its T1/T2 inputs or
  over Modbus (40481).

## Sources

* COUNTIS E03 MODBUS communication table (4850 3039, 2017-08):
  <https://www.socomec.fr/sites/default/files/2021-05/COUNTIS-E03-MODBUS-48503039_COMMUNICATION-TABLE_2017-08_CMT_EN.html>
* COUNTIS E23 MODBUS communication table (4850 3050 V103.18, 2017-08):
  <https://www.socomec.fr/sites/default/files/2021-05/COUNTIS-E23-MODBUS-48503050-V103.18_COMMUNICATION-TABLE_2017-08_CMT_EN.html>
  *(Both socomec.fr URLs answer HTTP 403 to non-browser clients via Cloudflare; the
  tables above were extracted from the pages' rendered text. Mirrors exist on other
  Socomec country sites, e.g. emea.socomec.com and socomec.pt, with the same or a
  later revision.)*
* COUNTIS E23/E24 instruction manual 545 877 A (RS485 rules, SETUP 2 menu, function
  codes): <https://docs.rs-online.com/aae9/0900766b817228f3.pdf>
* Cross-check of scale factors — Loxone device template for COUNTIS E24:
  <https://github.com/eisber/lox-cli/blob/main/docs/library/plugins/socomec-countis-e24-3ph-80a-modbus-mid/socomec-countis-e24-3ph-80a-modbus-mid.xml>
* Cross-check of register list and scale — aggsoft Socomec Countis register listing:
  <https://www.aggsoft.com/modbus-data-logging/socomec.htm>
* Cross-check of addressing convention and the missing factory settings —
  `nobrega8/mbmap-library` COUNTIS E23/E24 maps:
  <https://github.com/nobrega8/mbmap-library/blob/main/maps/socomec/countis-e24.mbmap.json>

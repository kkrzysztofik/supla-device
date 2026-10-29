# Socomec COUNTIS E03/E23 — raw register probe, 2026-10-29

Pre-implementation probe for the [SUPLA driver plan](../plans/2026-10-29-socomec-countis.md).
It validates the decode rules in the [register reference](socomec-countis-modbus.md)
before any C++ was written, on the live installation described in the
[commissioning report](socomec-countis-commissioning-report.md).

**Host:** Raspberry Pi 5 `pi-szn` (192.168.2.120), Debian 13, kernel 6.18.50, aarch64
**Tool:** `extras/tools/modbus_probe.py` (copied to `/root/modbus_probe.py` on the Pi)
**Bus:** 9600 8N1, one meter per FTDI RS-485 adapter, both at slave address 2
**Sample:** one coherent back-to-back dump per meter (all blocks below come from the
same read sequence; consecutive reads are ~0.12 s apart, so sub-kWh counters move)

| Meter | Adapter | Stable path |
| --- | --- | --- |
| COUNTIS E03 | `BG03FMMW` | `/dev/serial/by-id/usb-FTDI_FT232R_USB_UART_BG03FMMW-if00-port0` |
| COUNTIS E23 | `BG041JVI` | `/dev/serial/by-id/usb-FTDI_FT232R_USB_UART_BG041JVI-if00-port0` |

Registers are decimal PDU addresses (no 40001 offset), big-endian, high word first.
Identity is read at 50058 (extended name), firmware at 50018/50019.

## Identity (both meters)

| Meter | 50058 ×8 raw words | Text | Firmware |
| --- | --- | --- | --- |
| E03 | `17263, 30062, 29801, 29472, 17712, 13088, 8224, 8224` | `Countis E03` | 3.00 |
| E23 | `17263, 30062, 29801, 29472, 17714, 13088, 8224, 8224` | `Countis E23` | 3.00 |

## Measurement block, 50520 ×48

Unit conversions per the register reference: V ×0.01, I ×0.001, F ×0.01 (E03) / ×0.001 (E23),
P/Q/S ×10, PF ×0.001.

### E03

Raw:

```
0, 23875, 65535, 65535, 65535, 65535, 0, 5000, 0, 2810, 65535, 65535, 65535, 65535, 65535, 65535,
0, 55, 65535, 65502, 0, 66, 65535, 64723, 0, 55, 32767, 65535, 32767, 65535, 65535, 65502,
32767, 65535, 32767, 65535, 0, 66, 65535, 65535, 65535, 65535, 65535, 64723, 32767, 65535, 32767, 65535
```

| Field | Raw | Decoded |
| --- | --- | --- |
| V1 | 23875 | 238.75 V |
| V2, V3 | `0xFFFFFFFF` | absent |
| F | 5000 | 50.00 Hz (LSB 0.01 Hz) |
| I1 | 2810 | 2.810 A |
| I2, I3, In | `0xFFFFFFFF` | absent |
| P (sum) | 55 | 550 W |
| Q (sum) | −34 | −340 var |
| S (sum) | 66 | 660 VA |
| PF (sum) | −813 | −0.813 |
| P1 / Q1 / S1 / PF1 | 55 / −34 / 66 / −813 | 550 W / −340 var / 660 VA / −0.813 |
| P2/P3, Q2/Q3, S2/S3, PF2/PF3 | `0x7FFFFFFF` | absent |

The E03 fills absent per-phase 32-bit slots with `0xFFFFFFFF` (V, I) and
`0x7FFFFFFF` (P/Q/S/PF); the driver never reads phases 2/3 on an E03.

### E23

Raw:

```
0, 24004, 0, 24040, 0, 24006, 0, 50070, 0, 3006, 0, 105, 0, 466, 0, 3085,
0, 53, 65535, 65477, 0, 85, 65535, 64910, 0, 53, 0, 0, 0, 0, 65535, 65491,
65535, 65534, 65535, 65525, 0, 72, 0, 2, 0, 11, 65535, 64800, 65535, 65401, 65535, 65505
```

| Field | Raw | Decoded |
| --- | --- | --- |
| V1 / V2 / V3 | 24004 / 24040 / 24006 | 240.04 / 240.40 / 240.06 V |
| F | 50070 | 50.070 Hz (LSB 0.001 Hz) |
| I1 / I2 / I3 / In | 3006 / 105 / 466 / 3085 | 3.006 / 0.105 / 0.466 / 3.085 A |
| P (sum) | 53 | 530 W |
| Q (sum) | −59 | −590 var |
| S (sum) | 85 | 850 VA |
| PF (sum) | −626 | −0.626 |
| P1 / P2 / P3 | 53 / 0 / 0 | 530 / 0 / 0 W |
| Q1 / Q2 / Q3 | −45 / −2 / −11 | −450 / −20 / −110 var |
| S1 / S2 / S3 | 72 / 2 / 11 | 720 / 20 / 110 VA |
| PF1 / PF2 / PF3 | −736 / −135 / −31 | −0.736 / −0.135 / −0.031 |

## Energy registers

### E23 per-line blocks (0x4D83 / 0x5583 / 0x5D83, 12 words each)

Each word row is: `Ea+ (2)`, `rEa+ (1)`, `Ea− (2)`, `rEa− (1)`, `Er+ (2)`, `rEr+ (1)`,
`Er− (2)`, `rEr− (1)`. The main counter has a 1 kWh LSB, the residual a 0.1 Wh LSB.

| Line | Raw 12 words | Ea+ | Ea− | Er+ | Er− |
| --- | --- | --- | --- | --- | --- |
| L1 | `0, 5909, 4269, 0, 0, 0, 0, 176, 6735, 0, 2084, 6893` | 5 909 426.9 Wh | 0 Wh | 176 673.5 Wh | 2 084 689.3 Wh |
| L2 | `0, 12239, 2756, 0, 0, 0, 0, 274, 3766, 0, 2964, 8611` | 12 239 275.6 Wh | 0 Wh | 274 376.6 Wh | 2 964 861.1 Wh |
| L3 | `0, 2599, 9099, 0, 0, 0, 30, 0, 55, 6186, 0, 502, 3799` | 2 599 909.9 Wh | 3.0 Wh | 55 618.6 Wh | 502 379.9 Wh |

The L3 residual `30` under a zero `Ea−` main is a genuine 3 Wh export residual, not a
decode error: it is inside the 0…9999 residual range.

### E23 50946 ×10 — 0.01 kWh totals

```
31, 43545, 0, 39437, 36, 52294, 0, 0, 8, 20926
```

| Register | Counter | Raw U32 | Decoded |
| --- | --- | --- | --- |
| 50946 | Ea+ (import active) | 2 075 161 | 20 751 610 Wh |
| 50948 | Er+ (import reactive) | 39 437 | 394 370 Wh |
| 50950 | Eap (apparent) | 2 411 590 | 24 115 900 Wh |
| 50952 | Ea− (export active) | 0 | 0 Wh |
| 50954 | Er− (export reactive) | 545 214 | 5 452 140 Wh |

### Independent confirmation of the combine rule (0x6583)

The same quantities are also exposed at 0x6583 as `main U32 (1 kWh) + residual U16 (0.1 Wh)`.
Read 25987 ×12: `0, 20751, 6106, 0, 0, 0, 0, 394, 3791, 0, 5452, 1442`.

| Counter | `main × 1000 + residual × 0.1` Wh | 50946 block | Delta |
| --- | --- | --- | --- |
| Ea+ | 20 751 610.6 | 20 751 610 | 0.6 Wh |
| Er+ | 394 379.1 | 394 370 | 9.1 Wh |
| Er− | 5 452 144.2 | 5 452 140 | 4.2 Wh |

**The `main × 1000 + residual × 0.1 Wh` rule is confirmed independently on both models**
(E03 0x6583: Ea+ 4 851 894.0 vs 50946 4 851 890; Er− 1 777 841.8 vs 1 777 840).

### E03 per-line blocks — absent

`19843 ×12`, `21891 ×12`, `23939 ×12` all return **Modbus exception 2 (illegal data
address)**. The E03 has no `Line #1/#2/#3` energy blocks at all, so it must use the
50946 totals.

### E03 totals

50946 ×10: `7, 26437, 0, 0, 65535, 65535, 0, 0, 2, 46712`

| Register | Counter | Raw U32 | Decoded |
| --- | --- | --- | --- |
| 50946 | Ea+ | 485 189 | 4 851 890 Wh |
| 50948 | Er+ | 0 | 0 Wh |
| 50950 | Eap | `0xFFFFFFFF` | **absent in this block on the E03** |
| 50952 | Ea− | 0 | 0 Wh |
| 50954 | Er− | 177 784 | 1 777 840 Wh |

0x6583 ×12 on the E03: `0, 4851, 8940, 0, 0, 0, 0, 0, 0, 0, 1777, 8418` →
Ea+ 4 851 894.0 Wh, Er− 1 777 841.8 Wh, and Eap present as 5514.40 kWh, while the
coarse 50774 register reads 5514 kWh. So the E03's apparent energy exists at 50774 and
0x6583 but is `0xFFFFFFFF` in the 50946 fine block. Apparent energy is not published to
SUPLA, so this does not affect the driver; it is recorded so nobody "fixes" it later.

50770 ×12 (`0, 4851, 0, 0, 0, 5514, 0, 0, 0, 1777, 0, 4851`) confirms the kWh-scale
duplicates the commissioning report already used: Ea+ 4851 kWh, Er+ 0, Eap 5514 kWh,
Ea− 0, Er− 1777 kWh.

### Cross-check: Σ per-line vs the 50946 totals

The E23 sums over its three line blocks (`main × 1000 + residual × 0.1 Wh`), versus the
system totals; the deltas are static across repeated samples.

| Counter | Σ lines (Wh) | Total (Wh) | Δ (Wh) | Δ as % of gross |
| --- | --- | --- | --- | --- |
| Ea+ | 20 748 612.4 | 20 751 610 | −2 997.6 | −0.014 % |
| Ea− | 3.0 | 0 | +3.0 | ≈0 |
| Er+ | 506 668.7 | 394 370 | +112 298.7 | +1.92 % |
| Er− | 5 551 930.3 | 5 452 140 | +99 790.3 | +1.71 % |

Net values (Ea+ − Ea−, Er+ − Er−): active Δ = −3 000.6 Wh (0.014 %); reactive
Δ = +12 508.4 Wh (**0.25 %**). The per-direction reactive counters do *not* add up to the
meter's reactive totals, while the reactive **net** does, and active energy agrees in both
formulations.

**Consequence:** the plan's "Σ lines equals the 10 Wh total within 30 Wh, for every
counter" premise is false on this hardware. It fails by two orders of magnitude for
active import and by 1.7–1.9 % for reactive. A per-counter cross-check with a fixed 30 Wh
tolerance would reject every cycle and the driver would never publish energy.

## Answers to the probe questions

1. **Does the E03 answer 0x4D83, or return an exception?** Exception 2 on all three line
   blocks. It has no per-line energy; it must use the 50946 totals (phase 1 only).
2. **Is every residual in 0…9999?** Yes. Non-zero residuals observed: 4269, 2756, 9099,
   6735, 3766, 6186, 6893, 8611, 3799, 30 — all inside the range. The 30 sits under a
   zero `Ea−` main (3 Wh export) and is legitimate.
3. **Does Σ lines equal the 10 Wh total within 30 Wh for every counter?** No.
   −2 997.6 Wh (Ea+), +112 298.7 Wh (Er+), +99 790.3 Wh (Er−), +3.0 Wh (Ea−).
   Active nets and reactive nets agree, per-direction reactive values do not.
4. **Does the 10 Wh Ea+ equal the kWh Ea+ within 1 kWh?** Yes. E23 20 751 610 vs
   20 751 000 Wh (610 Wh); E03 4 851 890 vs 4 851 000 Wh (890 Wh).
5. **What does the extended name hold, and does it contain the model?** `Countis E03` and
   `Countis E23`, firmware 3.00 on both; matching the configured model works by searching
   for `E03` / `E23` in the extended name.

## Additional findings carried into the driver

* **Phase sequence 51315 on the E23 returns `132`, repeatedly and stable**, not the
  documented `0` / `1` / `3`. Registers 51316 and above return exception 2, and the E03
  returns `0xFFFF` at 51315. So on firmware 3.00 this register cannot be mapped to the
  documented enum. The driver treats any value outside 0…1 as "unknown" and clears the
  SUPLA `VOLTAGE_PHASE_SEQUENCE` flag instead of guessing.
* **The cross-check must be active-energy only, with a relative tolerance.** Rationale:
  per-line active energy is physically additive and agrees with the system total to
  0.014 % (the residual is an installation-history offset, so it scales with the total);
  per-line reactive counters are a different object from the system reactive totals.
  Cross-checking only active energy still catches the failures it exists for — swapped
  word order, wrong scale, wrong register — because those are orders of magnitude off,
  while never blocking a healthy cycle. This refinement is recorded in the plan ledger
  and replaces `kCrossCheckTolerance = 3000` (30 Wh, per-counter, all counters).
* **The combine rule is confirmed**, so `Energy` can be built as `main × 100000 +
  residual × 10` in 0.00001 kWh units exactly as planned.
* **E03 absent-register sentinels:** `0xFFFFFFFF` for V/I, `0x7FFFFFFF` for P/Q/S/PF on
  the phase-2/3 slots. Phase 1 is fully populated.

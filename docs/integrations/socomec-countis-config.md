# Socomec COUNTIS E03 / E23 — front-panel configuration field guide

Hands-on procedure for commissioning two Socomec energy meters **from their own
display**, plus the RS-485 wiring checks, for the case where the meters are installed,
powered, and answer Modbus at no baud/parity/stop-bit/address combination tried from a
Raspberry Pi.

| Model | Reference | Type | Manual used |
| --- | --- | --- | --- |
| COUNTIS E03 | 4850 3039 | 1-phase direct, 40 A, 1 DIN module | **545865A** (E03/E04), German issue |
| COUNTIS E23 | 4850 3050 | 3-phase direct, 80 A, 4 DIN modules | **545 877 A** (E23/E24), English issue |

The register map, scale factors and address conventions are in
[socomec-countis-modbus.md](socomec-countis-modbus.md); this document covers only the
communication settings, the terminals, and the buttons.

**Status marks.** `[DOC]` = stated in a Socomec document (section given). `[3P]` =
confirmed by an independent third-party source. `[UNVERIFIED]` = not confirmed by any
source, or a working assumption to check on the unit.

---

## 1. Settings: what to set, and what "default" means here

### 1.1 Factory defaults — **unconfirmed**

| Model | Address | Baud | Parity | Stop bits |
| --- | --- | --- | --- | --- |
| E03 | **unconfirmed** | **unconfirmed** | **unconfirmed** | n/a — fixed |
| E23 | **unconfirmed** | **unconfirmed** | **unconfirmed** | **unconfirmed** |

Neither instruction manual, neither communication table (2017-08 and 2025-01
revisions), nor any integration guide located states a factory default in text.
`[DOC]` Both manuals mark the defaults **graphically** — an `XX` glyph over the default
value in the SETUP 2 / KONFIGURATION figures, with the legend `XX = default value`
(545 877 A §9.1.3, p.17) and `XX = Standardwert` (545865A §9.1.2, p.16). Those marks
are vector artwork and do not survive text extraction. The E23 communication table has
no "default" column at all.

**Do not assume the meters are at 9600 8N1 / address 1.** Read what they are set to
(§4), then change it. Anything else is guessing.

### 1.2 Values to set for this installation (a decision, not a default)

| Setting | E03 (4850 3039) | E23 (4850 3050) | Note |
| --- | --- | --- | --- |
| Slave address | **1** | **2** | 1–247 on both. Distinct numbers keep them separable if they are ever put on one bus. |
| Baud rate | **9600** | **9600** | Common to both models (E03 2400–38400, E23 1200–115200). |
| Parity | **n** (none) | **n** (none) | Simplest; both always use 8 data bits. |
| Stop bits | *not settable* — fixed | **1** | The E03 has no stop-bit register **and** no stop-bit screen. |
| Data bits | 8 (fixed) | 8 (fixed) | Modbus RTU. |

Resulting line: **9600 8N1** on both.

This table is the **target configuration** for a future shared-bus setup. The current
installation keeps each meter alone on its own bus at address **2** (see the
[commissioning report](socomec-countis-commissioning-report.md)), and the shipped
[`supla-device-socomec.yaml`](../../extras/examples/linux/supla-device-socomec.yaml) uses
`modbus_address: 2` for both channels accordingly. If you do change the E03 to address 1,
update both channels' `device.modbus_address` in the YAML as well, otherwise the driver
polls the wrong address and the E03 stays offline.

### 1.3 What each model can actually be set to

| Setting | E03 (4850 3039) | E23 (4850 3050) |
| --- | --- | --- |
| Address | 1–247 | 1–247 |
| Baud | 2400 / 4800 / 9600 / 19200 / 38400 `[DOC]` §9.1.2 + characteristics. The Modbus table also lists code `0` = 1200, which the display menu and the characteristics table do **not** offer | 1200 / 2400 / 4800 / 9600 / 19200 / 38400 / 57600 / 115200 `[DOC]` |
| Parity | n / o / E | n / o / E |
| Stop bits | **not configurable** — no screen, no register | 1 or 2 |
| Data bits | 8 | 8 |

Consequence for the Pi: against an **E03 you only need 6 baud rates × 3 parities ×
247 addresses with 1 stop bit**. Include **1200**: the front panel does not offer it, but
the Modbus table defines code `0` = 1200 and accepts writes to the settings block, so a
unit can be at 1200. Sweeping 2 stop bits against an E03 can never be right — it has no
stop-bit setting at all.

---

## 2. RS-485 terminals — the polarity trap (most likely root cause)

Socomec prints **`+` and `−`, never `A`/`B`**, on both meters. Both use screw
terminals; **neither model has an RJ45**. `[DOC]` 545 877 A §6.2, 545865A §6.2.

### 2.1 COUNTIS E23 (4850 3050)

The bus lands on the 7-way auxiliary block silkscreened `COM RS485 MODBUS T1/2`,
shared with the pulse output and the tariff input.

| Terminal | Function | Notes |
| --- | --- | --- |
| 1 | **NC** — not connected | "May be used for shielding continuity" |
| 2 | **−** (B / D−) | RS-485 inverting |
| 3 | **+** (A / D+) | RS-485 non-inverting |
| 4 | pulse output − | optocoupler, external 5–27 VDC, 27 mA max |
| 5 | pulse output + | " |
| 6 | tariff input | 0 VAC/DC → tariff 1 |
| 7 | tariff input | 80–276 VAC/DC → tariff 2 |

Caption as printed `[DOC]` 545 877 A §6.2, verbatim, because the drawing is ambiguous
after text extraction:

> `1: NC (not connected). May be used for shielding continuity.`
> `2: -` · `3: +`
> `4: -` / `5: +` — optocoupler pulse outputs; terminals 4-5 must be supplied with
> voltage between 5 and 27 VDC (27 mA max)
> `6-7: Switch tariffs: 0 VAC/DC -> Tariff 1, 80-276 VAC/DC -> Tariff 2`

Unused for RS-485: **1, 4, 5, 6, 7**. Only 2 and 3 carry the bus.

Independently repeated by EpiSensor's device directory for the E23/E24 (citing manual
revision 545877B): *"Terminals 2 negative and 3 positive; terminal 1 is unconnected and
may continue the shield."* `[3P]`

**No RJ45 exists on this model.** No source found describes an RJ45 on the E23/E24 —
the RS-485 is on the screw block above. `[DOC]` `[3P]` If the unit in hand physically
shows an RJ45, stop and re-identify it: the **older** COUNTIS E23 generation (ref
**4850 3035**, manual 542 103 A) has a different 7-pin map, and the two are not
interchangeable:

| 4850 3035 (old E23, 63 A) | Function |
| --- | --- |
| 1–2 | tariff input |
| 3 | NC |
| 4 | **120 Ω termination link** — link 4–5 to enable internal resistors |
| 5 | − |
| 6 | + |
| 7 | NC |

Applying that numbering to a 4850 3050 puts the bus on the pulse-output terminals: no
damage, no communication, no error message. `[DOC]` 542 103 A §5.4.1.

### 2.2 COUNTIS E03 (4850 3039)

The bus uses the 3-way group marked `NC` / `+` / `−` beside the `COM RS485 MODBUS`
silkscreen.

| Terminal | Function | Notes |
| --- | --- | --- |
| 1 | **+** (A / D+) | non-inverting |
| 2 | **−** (B / D−) | inverting |
| 3 | **NC** (shield) | "Kann zum Herstellen einer durchgängigen Abschirmung verwendet werden" — carries the shield through |
| 4 | pulse output − | optocoupler, 5–27 VDC, 27 mA max |
| 5 | pulse output + | " |

`[DOC]` 545865A §6.2 caption. The §8.1 bus drawing labels the same connector
`+ - NC` inside the daisy-chain loop, agreeing with that numbering.

**Signal common: there is none.** The third terminal is `NC` — a shield-continuity
pass-through, not a circuit node. Do not land the Pi adapter's `GND`/`COM` there
expecting a reference; the ports are SELV-isolated. `[DOC]` (characteristics: "RS485
2 Leiter + Abschirmung / Halbduplex", "Isolierung SELV"). That `NC` is purely passive
is inferred from the caption, not from a schematic `[UNVERIFIED]`.

### 2.3 The trap: `+` and `−` swap ends between the two models

| | pin 1 | pin 2 | pin 3 |
| --- | --- | --- | --- |
| E23 4850 3050 | NC | **−** | **+** |
| E03 4850 3039 | **+** | **−** | NC |

Both put **− on the middle pin** and `NC` at one end, but `+` is on the **opposite**
side. Wire both meters "by terminal number" from one drawing and **one of them is
reversed** — which by itself produces total silence on that meter, exactly the symptom
seen. **Wire by the printed `+` / `−`, never by number.**

### 2.4 A/B mapping

Socomec prints only `+`/`−`, never `A`/`B` — the mapping comes from the FENECON FEMS
commissioning guide for this exact meter, which labels its RS-485 conductors
"Data conductor minus (-) (B conductor)" and "Data conductor plus (+) (A conductor)":

* `+` = **A** = D+ (non-inverting)
* `−` = **B** = D− (inverting)

Note the collision: the TIA/EIA-485 standard calls the *inverting* line A, i.e. the
opposite convention, and cheap USB-RS485 dongles follow either one and are frequently
mislabelled. So "A to A, B to B" can be exactly wrong.

Practical rule: D+ → `+`, D− → `−`. If the dongle says `A`/`B` and nothing answers,
**swap the pair at the dongle** — 10 seconds, damages nothing, and one of the two
orientations is correct. Each meter has its own dongle here, so each can be swapped
independently.

---

## 3. Termination, biasing, shielding, signal common

What Socomec states, 545 877 A §8.2 and 545865A §8.2 (equivalent texts) `[DOC]`:

* Cable: **shielded twisted pair, LIYCY**. In a noisy environment or a very long
  network, shielded twisted pair with overall shield, **LIYCY-CY**.
* **120 Ω at both ends of the connection.** Nothing more: on the 4850 3050 generation
  there is no built-in termination switch and no jumper — the resistors are external,
  at the two physical ends of the trunk.
* Up to **32 devices** per segment, up to **1200 m**; a **repeater** beyond either
  limit.
* Half duplex, 2 wires + shield. Insulation SELV, load unit 1/8 (≈ 96 kΩ input
  impedance) — the electrical basis for the 32-device limit.

What Socomec does **not** state `[UNVERIFIED]`:

* **No bias/polarisation resistors.** Neither manual mentions pull-up/pull-down
  biasing. Do not add bias to a Socomec segment unless the master needs it.
* **No signal common.** With no `0V` terminal, the shield is the only third conductor.
  Ground it at **one** point (usually the master / cabinet PE) and carry it through
  each meter's `NC` pin. Single-point grounding is standard practice, not Socomec text.
* **No stated stub-length limit.** Keep drops under ~20 cm as a working rule.
* Over a metre or two at 9600 baud it usually works without termination; fit the
  resistors anyway if the run is long, noisy, or refuses to talk.

---

## 4. Front panel

### 4.1 COUNTIS E23 (4850 3050) — `SET` (recessed), `UP` (`△`), `ENTER`

Three buttons, per the front-panel legend `[DOC]` 545 877 A §4.3:
**1. SET** (recessed — "press SET for 3 seconds using a screwdriver", §9.1.1),
**2. UP** (`△`), **3. ENTER**.

They divide the work like this `[DOC]` §9.1.1/§9.1.2, and the FENECON FEMS
commissioning guide for this meter follows exactly this sequence step by step:

* `SET` > 3 s → **SETUP 1** (tariff source, COM vs DiG) — not what we need.
* `ENTER` > 3 s on the `SETUP` page → **SETUP 2** (address, baud, parity, stop bits).
* `UP` → step between setting screens, and change the flashing digit/value.
* `ENTER` → select, accept a digit, confirm, and leave the menu via the SAVE prompt.

| Function | Button | Press |
| --- | --- | --- |
| Switch menus (`tar.1`, `tar.2`, `tot`, `Par.b`, `rt`, `info`, `SETUP`) | `ENTER` | short |
| Enter SETUP 1 (tariff source) | `SET` | > 3 s |
| Enter SETUP 2 (communications) | `ENTER` | > 3 s, on the `SETUP` page |
| Step `Addr` → `bAud` → `Prty` → `StoP` → `rES` | `UP` | short |
| Change the flashing digit / value | `UP` | short |
| Accept a digit / advance | `ENTER` | short |
| Exit and raise the SAVE prompt | `ENTER` | > 3 s |
| Choose `y` / `N` / `C` | `UP` | short |
| Confirm the choice | `ENTER` | short |

**Procedure — address, baud, parity, stop bits**

1. Power the meter; wait for a normal measurement page.
2. Press `ENTER` until the `SETUP` page is shown.
3. Hold `ENTER` **3 s** → programming mode (SETUP 2).
4. `UP` until **`Addr`**. `ENTER` — the left-hand digit flashes. `UP` changes it;
   `ENTER` accepts it and moves to the middle, then the right-hand digit. Set **`002`**.
5. `UP` → **`bAud`**. `ENTER`; `UP` to **`9.6`**; `ENTER`.
6. `UP` → **`Prty`**. `ENTER`; `UP` to **`n`**; `ENTER`.
7. `UP` → **`StoP`**. `ENTER`; `UP` to **`1`**; `ENTER`.
8. Hold `ENTER` **3 s** → `SAVE` flashes **`y`**. `ENTER` to save
   (`UP` cycles `y` → `N` = exit without saving → `C` = continue without saving).

If `ENTER` > 3 s does not open a programming screen, the unit is not behaving as the
manual describes — stop and re-check the model rather than guessing at key combinations.

**MID gotcha.** On an E24 the recessed `SET` button is locked by the sealed terminal
cover: *"Make sure the SET program button is locked after fitting the terminal cover."*
`[DOC]` §7. That locks SETUP 1 (tariff); the `ENTER`-driven SETUP 2 above is still
reachable.

### 4.2 COUNTIS E03 (4850 3039) — one button (`ENTER`, marked `△`)

The E03 front panel has a **single button**, listed in the part legend as "2.
ENTER-Taste". `[DOC]` 545865A §4.3. The navigation table therefore maps every action
onto press length:

| Function | Where | Press |
| --- | --- | --- |
| Scroll within a menu | any page in a menu | short |
| Open the KONFIGURATION menu | KONFIGURATION menu page | > 3 s |
| Change a value / digit | KONFIGURATION pages | short |
| Confirm a value / digit | KONFIGURATION pages | > 3 s |
| Leave KONFIGURATION | SAVE (`SPEICHERN`) screen | > 3 s |

`[DOC]` 545865A §9.1.

**Procedure — address, baud, parity**

1. Power the meter; short-press until the `Konfiguration?` page.
2. **Hold > 3 s** → programming mode.
3. Short-press through `Addr` → `bAu` → `Prty` → partial-energy reset (Ea+ part.,
   Ea− part., Es part., Er+ part., Er− part.) → back to the first screen. `[DOC]` §9.1.1
4. Short-press changes the flashing digit/value; **hold > 3 s confirms** and advances.
   `[DOC]` §9.1.3 gives the worked example as `>3 s` → `x2` → `>3 s` → `x4` → `>3 s`
   → `x2` → `>3 s` → `x1`.
5. At the SAVE screen, hold > 3 s to leave. `[DOC]`

The number of short presses depends on the value already displayed — watch the flashing
digit rather than counting presses.

### 4.3 Power cycle?

No power cycle is documented for either meter. Both store settings in non-volatile
memory (E03 characteristics: counters "im EEPROM-Speicher"; the E23 retains settings in
FRAM), and values marked `y` take effect at the exit prompt. `[DOC]`

Practical rule `[UNVERIFIED]`: after a baud/parity change the meter switches to the new
frame immediately and the old master settings stop working — expected, not a fault. If
a setting seems not to have taken, power-cycle the meter before concluding the write
failed.

---

## 5. Reading the current values first

Before changing anything, walk the menu and write down what is already set:

* E23: SETUP 2 → `Addr`, `bAud`, `Prty`, `StoP`
* E03: KONFIGURATION → `Addr`, `bAu`, `Prty`

Two useful outcomes:

* Already at §1.2 values → the settings are not the fault; the wiring is (§6).
* Different values → note them: a baud/parity mismatch is just as silent as a reversed
  pair, and the old values tell you which fault you had.

---

## 6. Writing the same settings over Modbus instead

Only usable once a link exists — if the meters answer nothing, §4 is the only way in.
Register numbers below are raw 0-based PDU addresses (see
[socomec-countis-modbus.md](socomec-countis-modbus.md) for why no 40001 offset).
Function codes `03` read, `06` write one word, `16` write n words (max 128). `[DOC]`
§8.3.

**COUNTIS E03 — lock level `NONE`**

| Register | Hex | Meaning | Values |
| --- | --- | --- | --- |
| 36865 | 0x9001 | RS-485 address | 1–247 |
| 36866 | 0x9002 | Baud rate | `0`=1200, `1`=2400, `2`=4800, `3`=9600, `4`=19200, `5`=38400 |
| 36867 | 0x9003 | Parity | `0`=none, `1`=odd, `2`=even |

Lock level **NONE**: the table lists `READ | WRITE | WRITE_MANY` in *both* the locked
and unlocked columns, i.e. always writable, no password. `[DOC]`

**COUNTIS E23 — lock level `USER`**

| Register | Hex | Meaning | Values |
| --- | --- | --- | --- |
| 57344 | 0xE000 | Slave address (U8) | 1–247 |
| 57345 | 0xE001 | Baud rate | `0`=1200, `1`=2400, `2`=4800, `3`=9600, `4`=19200, `5`=38400, `6`=57600, `7`=115200 |
| 57346 | 0xE002 | Stop bits | `0`=1 stop bit, `1`=2 stop bits |
| 57347 | 0xE003 | Parity | `0`=none, `1`=odd, `2`=even |

Lock level **USER**: locked = `READ` only, unlocked = `READ | WRITE | WRITE_MANY`. At
USER level (the normal state) the four registers are writable; no password or unlock
sequence is documented. `[DOC]`

**Store the configuration — both models**

| Register | Hex | Value | Effect |
| --- | --- | --- | --- |
| 57856 | 0xE200 | `0xA1` | **store product configuration** |
| 57856 | 0xE200 | `0xB2` | product reset — do not use |

`[DOC]` both tables, "Actions / Action system", lock level `NONE`. Write `0xA1` after
the settings registers, otherwise the new values may not survive a power cycle.

Changing the baud rate or address over the link breaks that link — write the new values
last, then reconnect with the new settings.

---

## 7. Wiring checks, before touching the Pi again

Work with the bus disconnected from the Pi. RS-485 is SELV, but isolate at the Pi end
while measuring.

1. **Identify terminals by silkscreen, not number.** E23: bus on **2 (−) / 3 (+)**;
   E03: bus on **1 (+) / 2 (−)**. Label both meters before starting.
2. **Confirm you are not on the pulse output.** On both models 4/5 sit next to the bus
   and are also a `−`/`+` pair. Landing the bus there is completely silent.
3. **Minus-to-minus at every device.** With the bus disconnected, buzz E23 pin 2 to E03
   pin 2, and E23 pin 3 to E03 pin 1. Both ≈ 0 Ω. One open + one short = the two meters
   are wired with opposite polarity; fix it here.
4. **Shield continuity** through the meters (E23 pin 1, E03 pin 3), and **not** shorted
   to either signal wire or to a phase.
5. **Termination.** With meters and Pi disconnected, measure across A/B of the trunk:
   two 120 Ω end resistors read ≈ 60 Ω. No resistors → open/cable resistance only. One
   → ≈ 120 Ω. Dead short → mis-wire or a failed transceiver on a node.
6. **No third device on the segment** — another master, logger or gateway still driving
   it produces exactly the "answers nothing, at any setting" symptom.
7. **The dongle itself.** Check its A/B labelling (swap it, see §2.4) and its DE/RE
   direction handling. Loopback-test it against a known-good device if nothing on the
   bus ever answers.

---

## 8. Verification, in the order that isolates the fault fastest

1. **Read the settings off each display** (§4, §5) and write them down. That removes
   every unknown except wiring.
2. **Commission the E23 alone** on a short bench cable: 120 Ω at the Pi end, meter at
   the other end with its own 120 Ω, address and frame as read from the display, one
   stop bit. If that works, the meters were fine and the fault is in the installed cable
   or its termination.
3. **First read: one register, not a block.** On the E23, FC 03, start `51287`
   (`0xC857`), 1 register, unsigned ÷ 1000 = Hz. `[3P]` A plausible answer (~50 000 for
   50 Hz) proves address, frame and polarity at once. The E03 has the same 16-bit
   frequency mirror at `51287` per its own table.
4. **Then two registers** (`50520`, U32, ÷100 = volts) to confirm word order, which is
   expected high-word-first but unverified on hardware.
5. **Compare with the display** before wiring anything into the SUPLA driver: displayed
   kWh and register value must agree in magnitude (note `50770` is a kWh-LSB register,
   i.e. raw = kWh).
6. **Record** address, baud, parity, stop bits, firmware (`50018`/`50019`, `50007`) and
   the terminal numbers used, in the installation notes.

A reply of any kind — including a Modbus *exception* — proves the meter heard the
request. Total silence means the pair is still reversed, the frame/address is still
wrong, or the meter is not powered.

---

## 9. Quick reference card

```
COUNTIS E23 4850 3050              COUNTIS E03 4850 3039
  bus:  pin 2 = -   pin 3 = +        bus:  pin 1 = +   pin 2 = -
        pin 1 = NC (shield)                pin 3 = NC (shield)
  buttons: SET (recessed), UP, ENTER  button: one, ENTER / TRIANGLE
  SET >3 s     -> SETUP 1 (tariff)    TRIANGLE >3 s on "Konfiguration?"
  ENTER >3 s   -> SETUP 2 (comms)       -> programming mode
  UP: Addr bAud Prty StoP rES         TRIANGLE: Addr bAu Prty rES
  UP change / ENTER confirm             (no stop-bit screen)
  y / N / C on exit                   short = change, >3 s = confirm
                                      SAVE screen to exit

Both: 120 ohm at both ends, LIYCY(-CY), <=32 nodes, <=1200 m,
      no 0V terminal, no documented bias resistors, shield to the NC pin only.
      (Bias and shield behaviour is [UNVERIFIED] -- see the section above.)
Modbus store action: 57856 (0xE200) = 0xA1
```

---

## Sources

* COUNTIS E23/E24 instruction manual **545 877 A** (SETUP 2 §9.1, RS485 rules §8.2,
  terminal caption §6.2). Strictly Socomec hosts this behind Cloudflare and returns
  HTTP 403 to normal clients; it was read through the `r.jina.ai` text-rendering proxy
  from the RS Components mirror:
  <https://docs.rs-online.com/aae9/0900766b817228f3.pdf>
* COUNTIS E03/E04 instruction manual **545865A**, German issue (§4.3 buttons, §6.2
  terminals, §8 RS485, §9 configuration, characteristics):
  <https://www.e-term.at/media/pdf/40/cc/30/COUNTIS-E03-E04_Bedienungsanleitung.pdf>
* COUNTIS E23 MODBUS communication table V103.18 (57344–57347 lock level `USER`,
  57856 action `0xA1`):
  <https://www.socomec.fr/sites/default/files/2021-05/COUNTIS-E23-MODBUS-48503050-V103.18_COMMUNICATION-TABLE_2017-08_CMT_EN.html>
* COUNTIS E03 MODBUS communication table (36865–36867 lock level `NONE`):
  <https://www.socomec.fr/sites/default/files/2021-05/COUNTIS-E03-MODBUS-48503039_COMMUNICATION-TABLE_2017-08_CMT_EN.html>
* Older COUNTIS E23 generation, ref 4850 3035, manual **542 103 A** §5.4.1 — different
  7-pin map with a 120 Ω termination link:
  <https://docs.rs-online.com/c624/0900766b814eecf4.pdf>
* Independent cross-check of the E23 terminals (2 = −, 3 = +, 1 = NC/shield) and of
  the single-register frequency check:
  <https://episensor.com/devices/socomec-countis-e23-e24>
* FENECON FEMS — Installation and Configuration Manual, 3-phase sensor 80 A (SOCOMEC
  COUNTIS E23/E24). Independent of Socomec, with screenshot-by-screenshot confirmation
  of the `ENTER`/`UP` programming sequence and of the `+` = A / `−` = B conductor
  naming:
  <https://docs.fenecon.de/en/fems/fems-app/installationsanleitungen/Socomec_Countis_E23_Installationsanleitung.html>

Register map and scale factors: [socomec-countis-modbus.md](socomec-countis-modbus.md).

**Still open:** factory defaults (not published anywhere found — the manuals mark them
only graphically); whether `NC` is purely passive; and whether a settings change needs a
power cycle (the manuals are silent; register 57856 = `0xA1` is the documented "store"
mechanism for changes made over Modbus). The E23 key assignment above follows the
FENECON guide together with manual §9.1.1 — if `ENTER` > 3 s does not open a programming
screen on the unit in front of you, stop and re-identify the model rather than guessing
at key combinations.

Compiled 2026-10-28 for Krzysztof Krzysztofik.

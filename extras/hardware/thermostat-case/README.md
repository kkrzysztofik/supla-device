# Thermostat wall box: XIAO ESP32-S3 + Grove

A wall-mounted, two-part printed box for a SUPLA thermostat built from:

| Part | Where it goes |
|---|---|
| Seeed XIAO ESP32-S3 on a Grove Shield for XIAO (103020312, full 58 mm board) | base, vertical, USB-C down |
| Grove Relay v1.3 (switches the boiler's **low-voltage** thermostat input) | base, right column |
| Grove OLED 0.96" (SSD1308) | lid, window at the top |
| Grove LED Button, yellow | lid, cap through the lid |
| DS18B20 probe SEN0002, 2 m | cable through the bottom wall |
| DFRobot FIT0505 6 dBi antenna, U.FL | socket on the top wall |

```sh
/usr/bin/python3 check.py              # layout checks, stdlib only
uv run parts.py --part all --check     # solid checks + interference against board envelopes
uv run parts.py --part all             # -> out/*.stl, out/*.step, out/assembly_preview.step
```

About 75 × 72 × 41 mm closed, plus a 10 mm antenna socket on top.

## Print (FDM, no supports)

| Part | Orientation |
|---|---|
| `out/thermostat_base.stl` | back on the bed, as exported |
| `out/thermostat_lid.stl` | face on the bed, as exported |
| `out/thermostat_tab_x3.stl` | print 3, flat |

Holes through the walls are teardrops, so they print without supports. PETG is the safer
choice for the two shield snap hooks, which strain about 1.8% when flexed.

## Hardware

No self-tapping screws. Everything is from a standard M2/M3 kit plus heat-set inserts:

- 4 × M3 heat-set inserts (M3 × 5.7, OD ~4.6, e.g. Ruthex) pressed into the corner columns (Ø4.0 holes)
- 4 × M3×8 countersunk machine screws (DIN 7991), lid into the inserts
- 3 × M2×6 machine screws, one per locking tab. They thread straight into Ø1.6 plastic posts.
  If the thread feels loose in your filament, lower `M2_PILOT`; if it's too stiff, raise it.
- 2 × 4 mm countersunk wood screws + wall plugs, box to the wall
- 2 small zip ties, probe and relay cables to the anchors above their holes
- 4 Grove cables: 10 cm for the relay, 20 cm for the OLED and the button (they ride on the lid)

## Assembly

1. Press the four inserts into the columns with a soldering iron (before anything else goes in).
2. Screw the base to the wall through the two countersunk holes in the middle strip.
3. Snap the shield in between its hooks, XIAO end down, then plug in the USB-C from below.
4. **Relay, button, OLED** all mount the same way: tilt the board, slide its Grove-connector end
   under the two lips, lower the other end onto its supports, put a tab on the post so it overlaps
   the board edge, and screw it down with an M2×6. The post stops the board sliding out of the lips
   even before the tab goes on.
5. Wire the Grove cables, close the lid with the four M3×8 screws.

DS18B20: cut one end off a Grove cable and join it to the probe's bare wires
(red → VCC, black → GND, yellow/white → SIG). Add 4.7 kΩ between SIG and VCC.

## Measure before printing

These values in `params.py` aren't in any Seeed file, so they are estimates. Update them and re-run both checks:

- `BTN_SW_H`, `BTN_CAP_D`: button cap height above the PCB and its diameter. Together these set the lid post length and the cap hole.
- `OLED_PANEL_H`, `OLED_AA_TOP`: glass height above the PCB, and the gap from the glass top edge to the lit area. These set the pad height and where the window sits.
- `USB_OVERMOLD`: the plug body of your USB-C cable.
- `PROBE_CABLE_D`, `RELAY_CABLE_D`, `ANT_BASE_D`: the cable holes and the antenna socket bore.
- `RELAY_BODY_H`, `GROVE_PLUG_UP`, `GROVE_CONN_H`: these set the box depth (worst case, `IN_D` in `params.py`).

## Sources

Board outlines, holes and part positions were parsed from Seeed's CAD files, not traced from images:
`Grove_Shield_for_Seeeduino_XIAO_v1.2_KiCad_SCH_PCB.zip`, `Grove-Relay_Eagle_Files.zip` (v1.2 board,
the v1.3 outline is assumed the same), `Grove-Red_LED_Button.zip` and `OLED 128x64.zip`, all
linked from wiki.seeedstudio.com. Panel data comes from the LY190-128064 datasheet.

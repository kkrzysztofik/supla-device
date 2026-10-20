# DIN-rail Raspberry Pi 5 case: Display HAT Mini top plate

A replacement top plate and button plungers for the
[DIN Rail Case for Raspberry Pi 3/4/5](https://www.printables.com/model/1257367) (CC-BY).
They mount a [Pimoroni Display HAT Mini](https://shop.pimoroni.com/products/display-hat-mini)
in the case's DIN front face. The HAT connects to the Pi with a 10 cm cable.

```sh
/usr/bin/python3 check.py              # layout checks, stdlib only
uv run parts.py --part all --check     # solid checks + interference against ref/case_*.stl
uv run parts.py --part all             # -> out/*.stl, out/*.step
```

## Print

| Part | Qty | Orientation |
|---|---|---|
| `out/top_display_hat_mini.stl` | 1 | face down, no supports |
| `out/button_plunger_x4.stl` | 4 | flange down |

All other case parts are the original ones from Printables (Pi 5 front/back plates).

## Hardware

- 4 × M2.5×6 self-tapping screws (from below, through the HAT into the posts)
- 1 × 40-pin **male–female** GPIO ribbon, 10 cm. The HAT has a female socket underneath, so the male end goes into the HAT.
- If the image glitches over the ribbon, lower `spi_speed_hz` in the displayhatmini library (to ~32 MHz).

## Measure before printing

These values in `params.py` are estimates because the drawing doesn't give them. Update them and re-run both checks:

- `H_GLASS`: HAT PCB top to glass top. Sets the post length.
- `H_SWITCH`: HAT PCB top to the top of the buttons. Sets the plunger length.
- `GLASS_BX`: LCD glass edges. Scaled off the drawing's pixels; the left plungers clear the glass by 0.55 mm.
- `H_IDC`: height of the ribbon connector below the HAT's socket.

## Sources and attribution

- `ref/case_*.stl`: from Printables model 1257367, licensed CC-BY 4.0. Only the parts the
  interference check needs are included here.
- `ref/DisplayHATMiniDimensionalDrawing.png`: Pimoroni's dimensional drawing of the Display HAT Mini (PIM589).
- `case_bottom_4te_4du.stl` is not watertight, so the interference check skips it. It sits well below the HAT.

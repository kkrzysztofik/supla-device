# NVLink riser block (RTX 3090 pair, open mining frame)

The Palit GameRock and PNY XLR8 REVEL 3090s hang by their bracket flanges from one rail.
The PNY's NVLink finger sits ~25 mm lower, so a 3-slot bridge won't seat.
This block goes between the rail and the PNY flange and lifts the whole card by `OFFSET`.

```sh
/usr/bin/python3 check.py                  # parameter invariants, stdlib only
uv run parts.py --check                    # 1 solid / 1 shell / height / interference vs rail, flange, nut, screw
NVLINK_OFFSET=24.6 uv run parts.py         # out/nvlink_riser_p2s.stl, .step, assembly_preview.step
```

## Before printing

Measure `OFFSET` with both cards bolted to the rail: rail top to the top edge of each NVLink finger, then take the difference.
Pass it as `NVLINK_OFFSET`; don't edit the code. Also check `RAIL_D` (rail width) and `FLANGE_W` (PNY flange width; the block must stay under it).

## Print and fit

- ASA/ABS, bottom on the bed as modelled, no supports. 4 walls, 40 % infill.
- Press two M3×5.7 heat-set inserts into the bottom (HeatSync press).
- Fix the block loosely to the rail with two M3 screws up through the rail slot. Slide an M3 nut into the side channel.
  Put the PNY flange on top and fit a loose M3×10 through the flange into the nut.
- **Fit the bridge first**, then tighten: rail screws, then the flange screw. The bridge sets the 60.96 mm pitch and
  the lengthwise position. The block slides in the rail slot, and the screw slides along the top slot.
- If it's still off by under 1 mm, add washers under the flange instead of reprinting.
- The PCIe risers are flex ribbons, so the PNY's just follows the card up; no riser spacer needed.

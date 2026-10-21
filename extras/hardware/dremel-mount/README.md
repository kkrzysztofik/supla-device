# Dremel 3000 mount for the HeatSync insert press

Swaps the HeatSync soldering-iron collar for a Dremel 3000 holder, which turns the press into a small drill press.
It bolts to the stock rack with the collar's two M4x10 screws, into the rack's M4 inserts. Nothing else on the press changes.

| Feature | Taken from |
|---|---|
| 41 mm slot, ears, M4 counterbores | HeatSync iron collar (`A1_HeatSync.3mf`, object_77) |
| Ø19.4 nose hole, 3 mm floor, taper cup | Dremel_3000_holder_V1 (`Dremel+Drill+Press+Small+Printbed.3mf`, object_5) |
| R25.2 rear cradle 140–160 mm up | same holder |

```sh
/usr/bin/python3 check.py      # parameter invariants, stdlib only
uv run parts.py --check        # 1 solid / 1 shell / bed fit; interference vs the real HeatSync rack mesh
uv run parts.py                # out/dremel_mount.stl, .step, assembly_preview.step (with a Dremel envelope)
```

`--check` reads the rack straight from `A1_HeatSync.3mf` (HeatSync by Thinkable on MakerWorld).
The `.3mf` files are gitignored, so download them into this directory first.
The check also places the original collar to prove the rack registration (0 mm³).

## Print and fit

- PETG, prints upright as modelled with no supports: the cradle sits on a 45° undercut. 4 walls.
- Fitting: slide the nose thread down through the floor with the tool tilted forward, rock it back into the cradle, then screw the nose cap on from below.
  A strap or zip tie through the two windows holds the body against the cradle.
- Turn the on/off switch to the front: the body clearance to the spine assumes r ≤ 26 mm (`BODY_R`, MEASURE).
- The Dremel axis sits 35 mm in front of the rack, about 17 mm further out than the iron. Re-zero your jigs.

`MEASURE` values in `params.py` came from the drill-press holder, not from a Dremel on the bench.
Check them with calipers before printing (`FLOOR_T`, `CRADLE_Z`, `BODY_R`).

# /// script
# requires-python = ">=3.10,<3.14"
# dependencies = ["build123d>=0.9"]
# ///
# Copyright (C) Krzysztof Krzysztofik
"""NVLink riser block: lifts the PNY RTX 3090 flange by OFFSET so a 3-slot bridge meets the GameRock.

uv run parts.py            export out/nvlink_riser_p2s.stl, .step, assembly_preview.step
uv run parts.py --check    solid checks + interference vs rail, flange, nut and screw envelopes
NVLINK_OFFSET=24.2 uv run parts.py   rebuild for a measured offset

Both paths run the params.py invariants first and refuse to continue when one fails, so an
offset that does not fit the rail inserts never becomes a model.
"""
import argparse
import os
import sys

from build123d import Align, Box, Compound, Cylinder, Pos, export_step, export_stl

import params as P
from check import invariants

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "out")
MIN = (Align.MIN, Align.MIN, Align.MIN)


def wbox(x, y, z):
    """Box from (lo, hi) ranges."""
    return Pos(x[0], y[0], z[0]) * Box(x[1] - x[0], y[1] - y[0], z[1] - z[0], align=MIN)


def riser():
    """The block: rail-insert bores from below, screw slot on top, nut channel through both X ends."""
    w, d, h = P.BLOCK_W / 2, P.BLOCK_D / 2, P.H
    body = wbox((-w, w), (-d, d), (0, h))
    for s in (1, -1):   # rail inserts, from below
        body -= Pos(s * P.RAIL_SCREW_X, 0, -1) * Cylinder(P.INSERT_D / 2 + P.HOLE_COMP / 2, P.INSERT_L + 1,
                                                          align=(Align.CENTER, Align.CENTER, Align.MIN))
    sx, sw = w - P.SLOT_END, P.SLOT_W / 2
    body -= wbox((-sx, sx), (-sw, sw), (h - P.FLANGE_SCREW_L - 1, h + 1))      # screw slot + tip relief
    nz = h - P.NUT_TOP - P.NUT_T
    body -= wbox((-w - 1, w + 1), (-P.NUT_AF / 2, P.NUT_AF / 2), (nz, nz + P.NUT_T))  # nut channel, open ends
    return body


def envelopes():
    """Things the block must touch only flush: rail, PNY flange, the nut, the screw at both slot ends."""
    h, nz = P.H, P.H - P.NUT_TOP - P.NUT_T
    rail = wbox((-60, 60), (-P.RAIL_D / 2, P.RAIL_D / 2), (-3, 0))
    flange = wbox((-P.FLANGE_W / 2, P.FLANGE_W / 2), (-P.RAIL_D / 2, P.RAIL_D / 2), (h, h + P.FLANGE_T))
    out = {"rail": rail, "PNY flange": flange}
    for x in (-(P.BLOCK_W / 2 - P.SLOT_END - P.SLOT_W / 2), P.BLOCK_W / 2 - P.SLOT_END - P.SLOT_W / 2):
        nut = Pos(x, 0, nz + 0.2) * Box(5.5, 5.5, 2.4, align=(Align.CENTER, Align.CENTER, Align.MIN))
        screw = Pos(x, 0, h + P.FLANGE_T - P.FLANGE_SCREW_L) * Cylinder(
            P.FLANGE_SCREW / 2, P.FLANGE_SCREW_L, align=(Align.CENTER, Align.CENTER, Align.MIN))
        out[f"nut x={x:+.1f}"], out[f"M3x{P.FLANGE_SCREW_L:g} x={x:+.1f}"] = nut, screw
    return out


def param_check():
    """Print the parameter invariants and return the messages that failed."""
    tests = invariants()
    for cond, msg in tests:
        print(("ok   " if cond else "FAIL ") + msg)
    return [msg for cond, msg in tests if not cond]


def check_part(part):
    """Solid, shell, height, bed-fit and interference checks on the built part."""
    n = len(part.solids())
    shells = len(part.solids()[0].shells()) if n else 0
    bb = part.bounding_box()
    size = (bb.size.X, bb.size.Y, bb.size.Z)
    tests = [(n == 1, f"{n} solid(s)"), (shells == 1, f"{shells} shell(s)"),
             (part.volume > 0, f"volume {part.volume:.0f} mm3"),
             (abs(bb.size.Z - P.OFFSET) < 1e-6, f"height {bb.size.Z:.2f} = OFFSET"),
             (all(s <= b for s, b in zip(size, P.BED, strict=True)),
              f"size {tuple(round(s, 1) for s in size)} fits bed")]
    for name, e in envelopes().items():
        v = (part & e).volume
        tests.append((v < 1e-3, f"block x {name}: {v:.4f} mm3"))
    for cond, msg in tests:
        print(("ok   " if cond else "FAIL ") + msg)
    return all(c for c, _ in tests)


def main():
    """Check the parameters, then either self-check the part or export it."""
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()

    bad = param_check()
    if bad:
        print(f"\n{len(bad)} FAILED: fix the parameters before printing, nothing written")
        sys.exit(1)

    part = riser()
    if a.check:
        good = check_part(part)
        print("\nall green" if good else "\nFAILED")
        sys.exit(0 if good else 1)
    os.makedirs(OUT, exist_ok=True)
    export_stl(part, os.path.join(OUT, "nvlink_riser_p2s.stl"), tolerance=0.01)
    export_step(part, os.path.join(OUT, "nvlink_riser_p2s.step"))
    export_step(Compound([part, *envelopes().values()]), os.path.join(OUT, "assembly_preview.step"))
    print("wrote", sorted(os.listdir(OUT)))


if __name__ == "__main__":
    main()

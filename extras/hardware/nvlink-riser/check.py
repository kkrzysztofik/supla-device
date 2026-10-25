# Copyright (C) Krzysztof Krzysztofik
"""Invariant checks on params.py. Plain /usr/bin/python3, no CAD."""
import sys

import params as P

failed = []


def ok(cond, msg):
    print(("ok   " if cond else "FAIL ") + msg)
    if not cond:
        failed.append(msg)


ok(abs(P.H - P.OFFSET) < 1e-9, f"block height = finger offset {P.OFFSET:g}")
ok(P.BLOCK_W <= P.FLANGE_W, f"block {P.BLOCK_W:g} under the PNY flange {P.FLANGE_W:g}: never touches the GameRock flange")
gap = P.NVLINK_PITCH - 59.06   # worst case: 3-slot GameRock flange right next to the PNY one
ok(gap > 0, f"3-slot flanges at {P.NVLINK_PITCH:.2f} pitch leave {gap:.2f} mm between them")

ok(P.RAIL_SCREW_X + P.INSERT_D / 2 + P.WALL_MIN <= P.BLOCK_W / 2, "rail inserts inside the block ends")
ok(2 * P.RAIL_SCREW_X - P.INSERT_D >= 2 * P.WALL_MIN, "wall between the rail inserts")
ok(P.NUT_AF + 2 * P.WALL_MIN <= P.BLOCK_D, f"nut channel {P.NUT_AF:g} has side walls in depth {P.BLOCK_D:g}")
ok(P.SLOT_W < P.NUT_AF - 1.0, "nut can't pull up through the top slot")
ok(P.SLOT_END >= P.WALL_MIN, "top slot ends leave the top one piece")

nut_floor = P.NUT_TOP + P.NUT_T
tip = P.FLANGE_SCREW_L - P.FLANGE_T
ok(tip >= nut_floor, f"M3x{P.FLANGE_SCREW_L:g} passes the nut (tip {tip:g} >= nut floor {nut_floor:g} below top)")
relief = P.FLANGE_SCREW_L + 1.0
ok(P.H - relief >= P.INSERT_L + P.WALL_MIN,
   f"screw relief ({relief:g} deep) clear of the rail inserts: OFFSET >= {relief + P.INSERT_L + P.WALL_MIN:g}")

size = (P.BLOCK_W, P.BLOCK_D, P.H)
ok(all(s <= b for s, b in zip(size, P.BED)), f"size {size} fits bed {P.BED}")

print("\nall green" if not failed else f"\n{len(failed)} FAILED")
sys.exit(1 if failed else 0)

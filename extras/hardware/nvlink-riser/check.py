# Copyright (C) Krzysztof Krzysztofik
"""Invariant checks on params.py. Plain /usr/bin/python3, no CAD."""
import sys

import params as P


def inv(cond, msg):
    """One parameter invariant as a (passed, message) pair."""
    return (bool(cond), msg)


def invariants():
    """Every relation between the numbers in params.py that the model relies on.

    Shared with parts.py, which runs these before it writes anything: OFFSET comes from the
    environment, so a measured value that fails a relation must not silently become a part.
    """
    gap = P.NVLINK_PITCH - 59.06   # worst case: 3-slot GameRock flange right next to the PNY one
    nut_floor = P.NUT_TOP + P.NUT_T
    tip = P.FLANGE_SCREW_L - P.FLANGE_T
    relief = P.FLANGE_SCREW_L + 1.0
    size = (P.BLOCK_W, P.BLOCK_D, P.H)
    return [
        inv(abs(P.H - P.OFFSET) < 1e-9, f"block height = finger offset {P.OFFSET:g}"),
        inv(P.BLOCK_W <= P.FLANGE_W,
            f"block {P.BLOCK_W:g} under the PNY flange {P.FLANGE_W:g}: never touches the GameRock flange"),
        inv(gap > 0, f"3-slot flanges at {P.NVLINK_PITCH:.2f} pitch leave {gap:.2f} mm between them"),
        inv(P.RAIL_SCREW_X + P.INSERT_D / 2 + P.WALL_MIN <= P.BLOCK_W / 2, "rail inserts inside the block ends"),
        inv(2 * P.RAIL_SCREW_X - P.INSERT_D >= 2 * P.WALL_MIN, "wall between the rail inserts"),
        inv(P.NUT_AF + 2 * P.WALL_MIN <= P.BLOCK_D,
            f"nut channel {P.NUT_AF:g} has side walls in depth {P.BLOCK_D:g}"),
        inv(P.SLOT_W < P.NUT_AF - 1.0, "nut can't pull up through the top slot"),
        inv(P.SLOT_END >= P.WALL_MIN, "top slot ends leave the top one piece"),
        inv(tip >= nut_floor,
            f"M3x{P.FLANGE_SCREW_L:g} passes the nut (tip {tip:g} >= nut floor {nut_floor:g} below top)"),
        inv(P.H - relief >= P.INSERT_L + P.WALL_MIN,
            f"screw relief ({relief:g} deep) clear of the rail inserts: OFFSET >= {relief + P.INSERT_L + P.WALL_MIN:g}"),
        inv(all(s <= b for s, b in zip(size, P.BED, strict=True)), f"size {size} fits bed {P.BED}"),
    ]


def main():
    """Print every invariant and exit non-zero when one of them failed."""
    tests = invariants()
    for cond, msg in tests:
        print(("ok   " if cond else "FAIL ") + msg)
    failed = [msg for cond, msg in tests if not cond]
    print("\nall green" if not failed else f"\n{len(failed)} FAILED")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()

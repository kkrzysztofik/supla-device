# Copyright (C) Krzysztof Krzysztofik
"""Invariant checks on params.py. Plain /usr/bin/python3, no CAD."""
import sys

import params as P

failed = []


def ok(cond, msg):
    print(("ok   " if cond else "FAIL ") + msg)
    if not cond:
        failed.append(msg)


dy, dz = P.RACK_TO_MOUNT[1], P.RACK_TO_MOUNT[2]
ok(abs(-15.45 + dy) < 1e-6, "rack front face (mesh y -15.45) lands on y = 0")
ok(abs(-50.0 + dz) < 1e-6, "rack block bottom (mesh z -50) lands on z = 0")
ok(abs(-9.45 + dy - P.BOLT_Y) < 0.05 and abs(-38.5 + dz - P.BOLT_Z) < 0.05,
   f"bolt ({P.BOLT_Y}, {P.BOLT_Z}) on the rack insert")
ok(P.BOLT_Y + P.M4_HEAD_D / 2 < P.SLOT_DEPTH and P.BOLT_Z + P.M4_HEAD_D / 2 < P.H_BASE,
   "counterbore inside the ear")

ear_wall = P.M4_HEAD_SEAT_X - P.SLOT_W / 2
ok(ear_wall >= P.WALL_MIN, f"wall under the screw head {ear_wall:.2f} >= {P.WALL_MIN}")
ok(P.EAR_X > P.M4_HEAD_SEAT_X, "counterbore has depth")
bite = P.M4_SCREW_L - (P.M4_HEAD_SEAT_X - P.RACK_W / 2)
ok(bite >= P.RACK_INSERT_L, f"M4x{P.M4_SCREW_L:g} reaches {bite:.1f} into the rack >= insert {P.RACK_INSERT_L}")
ok(P.EAR_X <= 25.65, "ears no wider than the original collar (carriage tabs above)")

cup_wall = P.CUP_R - P.cone_r(P.H_BASE)
ok(cup_wall >= 3 * P.WALL_MIN, f"cup wall at the top {cup_wall:.1f} >= {3 * P.WALL_MIN}")
ok(P.cone_r(P.FLOOR_T) > P.NOSE_HOLE_D / 2 + 2, "floor ring under the nose cap >= 2 mm")
ok(P.FRONT_OPEN_W >= P.NOSE_HOLE_D, "front gap passes the nose thread")
ok(P.FRONT_OPEN_Z > P.FLOOR_T, "floor stays a closed ring")

ok(P.CRADLE_R <= P.BODY_R and P.cone_r(P.H_BASE) <= P.BODY_R, "body envelope covers cup top and cradle")
ok(P.W - P.CRADLE_R >= 3.0, f"cradle arm {P.W - P.CRADLE_R:.1f} >= 3")
ok(P.W - (P.BODY_R + P.BODY_CLR) >= 2 * P.WALL_MIN, f"undercut side wall {P.W - P.BODY_R - P.BODY_CLR:.1f} >= {2 * P.WALL_MIN}")
spine_front = -(P.SPINE_GAP + P.SPINE_T)
ok(spine_front - (P.AXIS_Y + P.BODY_R) >= P.BODY_CLR - 1e-9, "body clears the spine")
ok(P.AXIS_Y + P.CRADLE_R + 3.0 <= spine_front + P.SPINE_T, "cradle back meets the spine")
ok(P.AXIS_Y + P.BODY_R < -P.SPINE_GAP, "body clears the rack front face")
ok(P.wedge_z0() > P.H_BASE, f"45-degree undercut starts above the base (z {P.wedge_z0():.1f})")
band = P.CRADLE_Z[1] - P.CRADLE_Z[0]
ok(P.STRAP_H <= band - 2 * P.WALL_MIN * 2, "strap window inside the cradle band")
ok(P.STRAP_Y + P.STRAP_W < spine_front, "strap window in front of the spine")

size = (2 * P.W, P.SLOT_DEPTH - (P.AXIS_Y - P.CUP_R), P.CRADLE_Z[1])
ok(all(s <= b for s, b in zip(size, P.BED)), f"size {tuple(round(s, 1) for s in size)} fits bed {P.BED}")

print("\nall green" if not failed else f"\n{len(failed)} FAILED")
sys.exit(1 if failed else 0)

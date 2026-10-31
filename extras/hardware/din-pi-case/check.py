# Copyright (C) Krzysztof Krzysztofik
"""Invariant checks on params.py. Plain /usr/bin/python3, no CAD."""
import math
import sys

import params as P

failed = []


def ok(cond, msg):
    """Print and record one check line; failures decide the exit status."""
    print(("ok   " if cond else "FAIL ") + msg)
    if not cond:
        failed.append(msg)


def dist(a, b):
    """2D distance between two board-frame or world-frame points."""
    return math.hypot(a[0] - b[0], a[1] - b[1])


def wall_to_window(p, hole_d):
    """Thinnest wall between a hole edge and the window edge, in the board frame.

    Signed distance to the active area (positive outside, negative inside, so a
    hole moved into the window can never pass WALL_MIN). The 45 deg chamfer
    widens the window by WINDOW_CHAMFER on the outside face, which is the face
    the plate prints against the bed, so it eats into the wall there.
    """
    dx = max(P.ACTIVE_B[0][0] - p[0], p[0] - P.ACTIVE_B[0][1])
    dy = max(P.ACTIVE_B[1][0] - p[1], p[1] - P.ACTIVE_B[1][1])
    gap = math.hypot(max(dx, 0.0), max(dy, 0.0)) + min(max(dx, dy), 0.0)
    return gap - P.WINDOW_MARGIN - P.WINDOW_CHAMFER - hole_d / 2


# Board -> world is a rotation, not a mirror (else the LED lands on the wrong end).
o, ex, ey = P.b2w(0, 0), P.b2w(1, 0), P.b2w(0, 1)
det = (ex[0] - o[0]) * (ey[1] - o[1]) - (ex[1] - o[1]) * (ey[0] - o[0])
ok(abs(abs(det) - 1) < 1e-9 and det < 0, f"b2w is a 90 deg rotation (det {det:+.0f}, by-down board frame)")

# Plate in its slot.
ok(P.PLATE_X[1] - P.PLATE_X[0] == 42 and P.PLATE_Y[1] - P.PLATE_Y[0] == 68, "plate 42 x 68 like Top_blank")
ok(P.PLATE_X[0] >= -33 + 0.5 and P.PLATE_X[1] <= 10 - 0.5, "plate has 0.5 per side in the x slot -33..10")

# Board in the cavity.
c = [P.b2w(0, 0), P.b2w(*P.BOARD)]
bx = sorted(p[0] for p in c)
by = sorted(p[1] for p in c)
ok(by[0] - P.INNER_Y[0] >= 0.5 and P.INNER_Y[1] - by[1] >= 0.5, f"board y {by} clears walls {P.INNER_Y} by >=0.5")
ok(bx[0] >= P.RIB_X[0][1] + 0.5 and bx[1] <= P.RIB_X[1][0] - 0.5, f"board x {bx} clears guide ribs")
ok(bx[0] >= P.LEDGE_X[0][1] + 0.5 and bx[1] <= P.LEDGE_X[1][0] - 0.5, f"board x {bx} clears plate ledges")

# Window: active area + margin, inside the visible opening.
w = [P.b2w(P.ACTIVE_B[0][0], P.ACTIVE_B[1][0]), P.b2w(P.ACTIVE_B[0][1], P.ACTIVE_B[1][1])]
wx = sorted(p[0] for p in w)
wy = sorted(p[1] for p in w)
m = P.WINDOW_MARGIN + P.WINDOW_CHAMFER
ok(wx[0] - m >= P.OPENING_X[0] and wx[1] + m <= P.OPENING_X[1], "window + chamfer inside opening (x)")
ok(wy[0] - m >= P.OPENING_Y[0] and wy[1] + m <= P.OPENING_Y[1], "window + chamfer inside opening (y)")

# Posts: off the glass, inside the hole keep-out.
ok(P.POST_D / 2 <= P.HOLE_KEEPOUT_R, "post inside the HAT's hole keep-out")
for h in P.HOLES_B:
    gap = min(abs(h[0] - P.GLASS_BX[0]), abs(h[0] - P.GLASS_BX[1])) - P.POST_D / 2
    ok(gap >= 0.5, f"post at {h} clears glass edge by {gap:.2f}")

# Plungers: flange off glass, posts, LED, each other; wall to window >= WALL_MIN.
for b in P.BUTTONS_B:
    # Flange is D-shaped (flat at the shaft on the glass side), so the shaft is what nears the glass.
    g = min(abs(b[0] - P.GLASS_BX[0]), abs(b[0] - P.GLASS_BX[1])) - P.PLUNGER_D / 2
    ok(g >= 0.3, f"shaft at {b} clears glass edge by {g:.2f} (glass edge MEASURE)")
    for h in P.HOLES_B:
        g = dist(b, h) - P.FLANGE_D / 2 - P.POST_D / 2
        ok(g >= 0.5, f"flange {b} clears post {h} by {g:.2f}") if dist(b, h) < 10 else None
    g = dist(b, P.LED_B) - P.FLANGE_D / 2 - P.LED_HOLE / 2
    ok(g >= 0.5, f"flange {b} clears LED hole by {g:.2f}") if dist(b, P.LED_B) < 10 else None
    g = wall_to_window(b, P.PLUNGER_HOLE)
    ok(g >= P.WALL_MIN, f"plate wall between button {b} hole and window {g:.2f} >= {P.WALL_MIN}")
g = wall_to_window(P.LED_B, P.LED_HOLE)
ok(g >= P.WALL_MIN, f"plate wall between LED hole and window {g:.2f} >= {P.WALL_MIN}")
ok(dist(P.BUTTONS_B[0], P.BUTTONS_B[1]) >= P.FLANGE_D + 0.5, "flanges of a button pair don't touch")

# Plunger stack: rests on the switch, flange stays below the plate, travel available.
z_flange_top = P.z_pcb_top() + P.H_SWITCH + P.FLANGE_T
ok(z_flange_top + P.PLUNGER_SLACK <= P.PLATE_Z[0] + 1e-9,
   f"flange top {z_flange_top:.2f} + slack below plate underside {P.PLATE_Z[0]} (needs H_GLASS-H_SWITCH >= {P.FLANGE_T + P.PLUNGER_SLACK - P.GLASS_GAP:.1f})")
ok(P.PLUNGER_PROUD > P.SWITCH_TRAVEL, "plunger sticks out more than switch travel")
ok(P.plunger_len() > 0, f"plunger length {P.plunger_len():.2f}")

# Socket + ribbon connector hanging below the HAT stay above the Pi 5's tallest part.
z_low = P.z_pcb_top() - P.PCB_T - P.H_SOCKET - P.H_IDC
z_pi = P.Z_PI_PCB + P.PCB_T + P.H_PI_TALL
ok(z_low - z_pi >= P.CABLE_BEND, f"ribbon connector bottom z {z_low:.1f} is {z_low - z_pi:.1f} above Pi top {z_pi:.1f}")

print(f"\nFAILED: {len(failed)}" if failed else "\nall green")
sys.exit(1 if failed else 0)

# Copyright (C) Krzysztof Krzysztofik
"""Invariant checks on params.py. Plain /usr/bin/python3, no CAD."""
import math
import sys

import params as P

M2 = P.M2_PILOT

failed = []


def ok(cond, msg):
    print(("ok   " if cond else "FAIL ") + msg)
    if not cond:
        failed.append(msg)


def gap(a, b):
    """Clearance between two world rects ((x0, x1), (y0, y1)); negative = overlap."""
    dx = max(b[0][0] - a[0][1], a[0][0] - b[0][1])
    dy = max(b[1][0] - a[1][1], a[1][0] - b[1][1])
    return max(dx, dy) if dx < 0 or dy < 0 else math.hypot(dx, dy)


def circle_gap(c, r, rc):
    """Clearance from circle (c, r) to rect rc; negative = overlap."""
    dx = max(rc[0][0] - c[0], 0, c[0] - rc[0][1])
    dy = max(rc[1][0] - c[1], 0, c[1] - rc[1][1])
    return math.hypot(dx, dy) - r


def fmt(r):
    return "x %.1f..%.1f y %.1f..%.1f" % (*r[0], *r[1])


shield = P.rect(P.shield_xy, (0, P.SH[0]), (0, P.SH[1]))
relay = P.rect(P.relay_xy, P.RELAY_X, P.RELAY_Y)
oled = P.rect(P.oled_xy, P.OLED_X, P.OLED_Y)
btn = P.rect(P.btn_xy, P.BTN_X, P.BTN_Y)
cavity = ((0, P.IN_W), (0, P.IN_H))
print(f"cavity {P.IN_W:.1f} x {P.IN_H:.1f} x {P.IN_D}, outside "
      f"{P.OUTER[0][1] - P.OUTER[0][0]:.1f} x {P.OUTER[1][1] - P.OUTER[1][0]:.1f} x {P.OUTER[2][1]:.1f}")

# Boards inside the cavity, clear of each other and of the corner columns.
boards = {"shield": shield, "relay": relay, "oled": oled, "button": btn}
for n, r in boards.items():
    ok(r[0][0] >= P.CLR and r[0][1] <= P.IN_W - P.CLR and r[1][1] <= P.IN_H - P.CLR and r[1][0] >= 0,
       f"{n} {fmt(r)} inside cavity")
    for c in P.BOSSES:
        g = circle_gap(c, P.BOSS_D / 2, r)
        ok(g >= 0.5, f"{n} clears corner column {c} by {g:.2f}") if g < 5 else None
ok(gap(shield, relay) >= 2 * P.WOOD_HEAD_D / 2 + 1, f"shield-relay strip {gap(shield, relay):.1f} fits wall screw heads")
ok(gap(oled, btn) >= 1.0, f"oled-button gap {gap(oled, btn):.1f}")

# Wall screws: heads clear of the shield (incl. its locating posts) and the relay.
for s in P.WALL_SCREWS:
    for n, r in (("shield", shield), ("relay", relay)):
        g = circle_gap(s, P.WOOD_HEAD_D / 2, r)
        ok(g >= 1.2 + P.CLR + 0.5, f"wall screw {s} head clears {n} by {g:.2f} (posts need {1.2 + P.CLR:.1f})")

xiao = P.rect(P.shield_xy, *P.XIAO_B)

# Shield tabs: overlap only bare board, posts inside the cavity and off the wall screws/columns,
# screws reachable (outside the XIAO footprint).
for i, ((post, edge), site) in enumerate(zip(P.shield_tab_sites(), P.SHIELD_TABS)):
    ov = P.shield_tab_overlap(i)
    hit = [k for k in P.SHIELD_KEEPOUT if gap(ov, k) < 0.3]
    ok(not hit, f"shield tab {site[0]} overlap {fmt(ov)} clear of parts {hit}")
    r = P.TAB_POST_D / 2
    ok(r + P.CLR <= post[0] <= P.IN_W - r - P.CLR and r + P.CLR <= post[1] <= P.IN_H - r - P.CLR, f"shield tab post {post} inside cavity")
    for s in P.WALL_SCREWS:
        ok(math.dist(s, post) - r - P.WOOD_HEAD_D / 2 >= 0.5, f"shield tab post {post} clears wall screw {s}")
    for c in P.BOSSES:
        ok(math.dist(c, post) - r - P.BOSS_D / 2 >= 0.5, f"shield tab post {post} clears column {c}")
    ok(circle_gap(post, r, xiao) > 0, f"shield tab screw {post} reachable beside the XIAO")

# Shield rails leave the THT pin stubs free, and are tall enough.
ok(P.LEDGE_H >= P.PIN_STUB + 0.3, f"rails {P.LEDGE_H} > pin stubs {P.PIN_STUB}")
ok(min(P.XIAO_B[1][0], P.SH[1] - P.XIAO_B[1][1]) >= 1.5 + 0.5, "rails (1.5 wide) clear of XIAO header rows")

# USB-C through the bottom wall: overmold in the opening, clear of columns.
ux, uz = P.usb_center()
face_y = P.SH_Y0 - P.XIAO_USB_PROTRUDE
ok(face_y - P.USB_PLUG_METAL < 0 <= face_y - P.USB_PLUG_METAL + 1.0,
   f"plug overmold starts {face_y - P.USB_PLUG_METAL:.2f} from inner wall (just inside)")
uo = ((ux - P.USB_OVERMOLD[0] / 2 - 0.5, ux + P.USB_OVERMOLD[0] / 2 + 0.5),
      (uz - P.USB_OVERMOLD[1] / 2 - 0.5, uz + P.USB_OVERMOLD[1] / 2 + 0.5))
ok(uo[1][0] >= P.BACK_T + 1.0 and uo[1][1] <= P.Z_RIM - 1.0, f"USB opening z {uo[1][0]:.1f}..{uo[1][1]:.1f} inside the wall")
ok(uo[0][0] >= P.BOSS_IN + P.BOSS_D / 2 + 1.0, "USB opening clear of the corner column")

# Cable holes: apart from each other, the USB opening and the far corner column.
xs = sorted([(uo[0][0], uo[0][1], "usb")] + [(x - d / 2, x + d / 2, n) for n, x, d in P.CABLE_HOLES])
for a, b in zip(xs, xs[1:]):
    ok(b[0] - a[1] >= 2.0, f"bottom wall between {a[2]} and {b[2]}: {b[0] - a[1]:.1f}")
ok(xs[-1][1] <= P.IN_W - P.BOSS_IN - P.BOSS_D / 2 - 1.0, f"{xs[-1][2]} hole clear of corner column")
for n, x, d in P.CABLE_HOLES:
    ok(P.CABLE_Z - d / 2 >= P.BACK_T + 0.5, f"{n} hole above the floor")
rx = next(x for n, x, _ in P.CABLE_HOLES if n == "relay")
tie = ((rx - P.TIE[0] / 2, rx + P.TIE[0] / 2), (P.TIE_Y - P.TIE[1] / 2, P.TIE_Y + P.TIE[1] / 2))
ok(gap(tie, relay) >= 0.5, f"relay-cable tie anchor clears relay by {gap(tie, relay):.1f}")

# Probe clamp: really squeezes, room to tighten, clear of its neighbours on the floor.
cx = P.clamp_x()
clamp = ((cx - P.CLAMP_W / 2, cx + P.CLAMP_W / 2), P.CLAMP_Y)
zs, zc = P.clamp_z()
ok(P.CLAMP_GAP >= 0.2, f"clamp bar stands {P.CLAMP_GAP:.1f} off the saddle when tight (room to tighten)")
ok(P.CLAMP_SQUEEZE > 0, f"clamp squeezes the cable by {P.CLAMP_SQUEEZE}")
ok(zs - P.BACK_T >= 2.0, f"saddle {zs - P.BACK_T:.1f} tall")
ok(P.CLAMP_DX - P.PROBE_CABLE_D / 2 - P.M2_PILOT / 2 >= 1.0, "clamp screws clear of the cable groove")
ok(P.CLAMP_Y[0] >= 0, "clamp starts inside the cavity")
for n, r in (("relay tie", tie), ("relay", relay), ("shield", shield)):
    ok(gap(clamp, r) >= 1.0, f"clamp clears {n} by {gap(clamp, r):.1f}")
for s in P.WALL_SCREWS:
    g = circle_gap(s, P.WOOD_HEAD_D / 2, clamp)
    ok(g >= 1.0, f"clamp clears wall screw {s} by {g:.1f}")

# Antenna socket on the top wall; it also sits on the bed.
ok(P.ANT_CABLE_HOLE < P.ANT_BASE_D - 2 * P.WALL_MIN, "antenna socket has a floor around the cable hole")
ok(gap(((P.ANT_X - P.ANT_BASE_D, P.ANT_X + P.ANT_BASE_D), (P.IN_H - 1, P.IN_H)), ((P.IN_W - P.BOSS_IN - 4, P.IN_W), (0, P.IN_H))) > 0,
   "antenna clear of the top-right column")

# Window, button hole, lid posts.
w = P.oled_window()
glass = P.rect(P.oled_xy, *P.OLED_PANEL)
c = P.WINDOW_CHAMFER
ok(w[0][0] - c >= glass[0][0] - 0.01 and w[0][1] + c <= glass[0][1] + 0.01, f"window {fmt(w)} + chamfer inside glass x {glass[0]}")
ok(w[1][0] >= glass[1][0] and w[1][1] <= glass[1][1], "window inside glass y (AA margin only)")
bh = (P.BTN_C[1] + P.BTN_HOLE / 2 + c)
ok(w[1][0] - bh >= P.WALL_MIN, f"lid between window and button hole {w[1][0] - bh:.1f}")
for n, xs in (("A", P.OLED_PAD_A), ("B", P.OLED_PAD_B)):
    g = gap(P.rect(P.oled_xy, xs, P.OLED_Y), glass)
    ok(g >= 0.3, f"oled pad {n} clears glass by {g:.2f}")

# Slide-in mounts: lips clear the connector end's parts, the tab clears the far end's parts,
# the board can't slide out of the lips before the tab is on, posts stay in the cavity.
conn = {"oled": P.OLED_GROVE[1][1], "button": max(P.BTN_GROVE[1][1], 6.35 + 0.6)}
for n, (f, xs, ys, lip_y) in P.MODULES.items():
    ok(lip_y >= conn[n] + 0.3 and min(ys[1], -ys[0]) - lip_y >= 2.0, f"{n} lips |y| {lip_y}..edge clear of parts up to |y| {conn[n]:.2f}")
    ok(xs[1] - P.TAB_OVER - P.TAB_CLEAR_OF[n] >= 0.3, f"{n} tab overlap clears T-side parts by {xs[1] - P.TAB_OVER - P.TAB_CLEAR_OF[n]:.2f}")
    ok(P.CLR < P.LIP, f"{n} can slide {P.CLR} toward the post: less than the {P.LIP} lip overlap")
    for rr in P.rails(n):
        ok(rr[0][0] >= P.CLR and rr[0][1] <= P.IN_W - P.CLR and rr[1][0] >= P.CLR and rr[1][1] <= P.IN_H - P.CLR,
           f"{n} rail {fmt(rr)} inside cavity")
        for c in P.BOSSES:
            ok(circle_gap(c, P.BOSS_D / 2, rr) >= 0.5, f"{n} rail clears column {c}")
        for m, other in (("oled", oled), ("button", btn)):
            ok(m == n or gap(rr, other) >= 1.0, f"{n} rail clears {m} board") if m != n else None
    px, py = P.tab_post(n)
    r = P.TAB_POST_D / 2
    ok(r + P.CLR <= px <= P.IN_W - r - P.CLR and r + P.CLR <= py <= P.IN_H - r - P.CLR, f"{n} tab post ({px:.1f}, {py:.1f}) inside cavity")
    for c in P.BOSSES:
        ok(math.dist(c, (px, py)) - r - P.BOSS_D / 2 >= 0.5, f"{n} tab post clears column {c}") if math.dist(c, (px, py)) < 12 else None
    for s in P.WALL_SCREWS:
        ok(math.dist(s, (px, py)) - r - P.WOOD_HEAD_D / 2 >= 0.5, f"{n} tab post clears wall screw {s}") if math.dist(s, (px, py)) < 12 else None
for h in P.BTN_HOLES:
    sw = ((P.BTN_C[0] - P.BTN_SW / 2, P.BTN_C[0] + P.BTN_SW / 2), (P.BTN_C[1] - P.BTN_SW / 2, P.BTN_C[1] + P.BTN_SW / 2))
    g = circle_gap(P.btn_xy(*h), P.POST_D / 2, sw)
    ok(g >= 0.5, f"button post {h} clears switch by {g:.2f}")
ok(P.BTN_POST > P.BTN_SW_H - P.BTN_CAP_H, f"switch body ({P.BTN_SW_H - P.BTN_CAP_H}) stays under the lid (post {P.BTN_POST:.1f})")
ok(P.BTN_PROUD > P.BTN_TRAVEL, "cap sticks out more than its travel")
ok(P.OLED_POST >= P.OLED_PANEL_H + 0.1, "glass not pressed by the lid")

# Screws: enough thread in plastic, and the pilot doesn't break through the outside.
ok(P.BOSS_D - P.INSERT_D >= 2 * 1.6, f"insert column wall {(P.BOSS_D - P.INSERT_D) / 2:.1f} >= 1.6")
ok(P.LID_SCREW - P.LID_T <= P.INSERT_HOLE - P.SCREW_SPARE, "lid screw tip stays inside the insert hole")
for h in P.RELAY_HOLES:   # notch holes: the M2 head (3.8) must still land on the board
    inside = min(h[0] - P.RELAY_X[0], P.RELAY_X[1] - h[0], h[1] - P.RELAY_Y[0], P.RELAY_Y[1] - h[1])
    ok(inside + 1.9 >= 1.0, f"relay screw head at {h} overlaps the board by {inside + 1.9:.1f}")
for n, (lo, hi, eng, skin) in P.pilots().items():
    need = 5.0 if n == "lid" else 3.0
    ok(eng >= need, f"{n} screw engages {eng:.1f} mm (>= {need})")
    ok(skin >= 0.8, f"{n} pilot leaves {skin:.1f} mm skin outside")

# Depth and bed.
print(f"     depth: back stack {P.BACK_STACK:.1f} + front stack {P.FRONT_STACK:.1f} + gap {P.STACK_GAP} -> IN_D {P.IN_D}")
ok(P.Z_RIM - P.BACK_T - P.BACK_STACK - P.FRONT_STACK >= P.STACK_GAP, "back and front stacks don't meet")
size = [hi - lo for lo, hi in P.OUTER]
ok(all(s <= b for s, b in zip(size, P.BED)), f"box {[round(s, 1) for s in size]} fits bed")

print("\nFAILED: %d" % len(failed) if failed else "\nall green")
sys.exit(1 if failed else 0)

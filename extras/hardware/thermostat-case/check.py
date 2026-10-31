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
oled = P.rect(P.oled_xy, (0, P.MOD[0]), (0, P.MOD[1]))
btn = P.rect(P.btn_xy, (0, P.MOD[0]), (0, P.MOD[1]))
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

# USB-C port in the bottom wall: shell slot, plug body recess, clear of columns.
ux, uz = P.usb_center()
face, pocket, bezel = P.usb_port()
ok(face - bezel <= P.USB_MATE_GAP_MAX, f"plug body stops {face - bezel:.1f} short of the receptacle (<= {P.USB_MATE_GAP_MAX})")
ok(bezel - (-P.WALL) >= 0.4, f"outside recess for the plug body {bezel + P.WALL:.1f} deep")
ok(pocket < 0 and P.PORT_WALL >= 0.8, f"receptacle in a {-pocket:.1f} pocket behind a {P.PORT_WALL} bezel")
ok(P.USB_RECEPT[1] + 0.6 <= P.USB_OVERMOLD[1], "bezel shows round the shell slot")
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

# Window and button hole.
w = P.oled_window()
glass = P.rect(P.oled_xy, *P.OLED_GLASS)
c = P.WINDOW_CHAMFER
ok(w[0][0] - c >= glass[0][0] and w[0][1] + c <= glass[0][1] and w[1][0] - c >= glass[1][0] and w[1][1] + c <= glass[1][1],
   f"window {fmt(w)} + chamfer inside the glass")
bh = P.BTN_C[1] + P.BTN_HOLE / 2 + c
ok(w[1][0] - c - bh >= P.WALL_MIN, f"lid between window and button hole {w[1][0] - c - bh:.1f}")
ok(P.OLED_POST >= P.OLED_GLASS_H + 0.1, "glass not pressed by the lid")
ok(P.BTN_POST + P.BTN_POCKET >= P.BTN_BODY_H + 0.2, f"switch body {P.BTN_BODY_H} fits post {P.BTN_POST:.1f} + pocket {P.BTN_POCKET:.1f}")
ok(P.LID_T - P.BTN_POCKET >= 0.8, f"lid skin over the switch pocket {P.LID_T - P.BTN_POCKET:.1f}")
ok(P.BTN_PROUD >= P.BTN_TRAVEL + 0.3, f"cap {P.BTN_PROUD:.1f} proud, travel {P.BTN_TRAVEL}")
ok(P.BTN_HOLE + 2 * P.WALL_MIN <= P.BTN_SW + 2 * P.MOD_CLR, "pocket ledge around the cap hole")

# Notch mounts: washer covers the notch, posts in the cavity, shoulders bear on bare PCB.
ok(P.WASHER_D - P.NOTCH_D >= 2.0, f"washer {P.WASHER_D} laps the {P.NOTCH_D} notch by {(P.WASHER_D - P.NOTCH_D) / 2:.2f}")
ok(P.TAB_HOLE < 3.5, "M2 head can't pull through the washer")
r = P.NOTCH_POST_D / 2
posts = {n: [f(*nt) for nt in P.MOD_NOTCHES] for n, (f, _) in P.MODULES.items()}
for n, (f, _) in P.MODULES.items():
    (kx0, kx1), (ky0, ky1) = P.MOD_FRONT_KEEPOUT[n]
    for nx, ny in P.MOD_NOTCHES:
        g = circle_gap((nx, ny), P.WASHER_D / 2, P.MOD_GROVE)
        ok(g >= 0.3, f"{n} washer at ({nx}, {ny}) clears the Grove socket by {g:.2f}")
        # shoulder: annulus notch..post, on the board, off the keepout (+CLR); 0.05 grid
        k = 0.05
        area = sum(k * k for i in range(-80, 81) for j in range(-80, 81)
                   if P.NOTCH_D / 2 <= math.hypot(i * k, j * k) <= r
                   and 0 <= nx + i * k <= P.MOD[0] and 0 <= ny + j * k <= P.MOD[1]
                   and not (kx0 - P.CLR <= nx + i * k <= kx1 + P.CLR and ky0 - P.CLR <= ny + j * k <= ky1 + P.CLR))
        ok(area >= 1.5, f"{n} shoulder at ({nx}, {ny}) bears {area:.1f} mm2")
    for px, py in posts[n]:
        ok(r + P.CLR <= px <= P.IN_W - r - P.CLR and r + P.CLR <= py <= P.IN_H - r - P.CLR, f"{n} post ({px:.1f}, {py:.1f}) inside cavity")
        for cc in P.BOSSES:
            ok(math.dist(cc, (px, py)) - r - P.BOSS_D / 2 >= 0.5, f"{n} post clears column {cc}") if math.dist(cc, (px, py)) < 15 else None
        for m, other in (("oled", oled), ("button", btn)):
            ok(gap(((px - r, px + r), (py - r, py + r)), other) >= 0.5, f"{n} post clears {m} board") if m != n else None
plug = P.rect(P.oled_xy, *P.MOD_GROVE), P.rect(P.btn_xy, *P.MOD_GROVE)
for n, pr in zip(("oled", "button"), plug):
    ok(pr[0][0] - P.MOD_PLUG_OUT >= P.CLR, f"{n} Grove plug inside cavity (x {pr[0][0] - P.MOD_PLUG_OUT:.1f})")
fpc = P.rect(P.oled_xy, *P.OLED_FPC)
ok(fpc[1][1] <= P.IN_H - P.CLR, f"oled FPC fold inside cavity (y {fpc[1][1]:.1f})")
for px, py in posts["oled"]:
    ok(gap(((px - r, px + r), (py - r, py + r)), fpc) >= 0.3, f"oled post ({px:.1f}, {py:.1f}) clears the FPC fold")

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

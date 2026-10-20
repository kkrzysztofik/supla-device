# /// script
# requires-python = ">=3.10,<3.14"
# dependencies = ["build123d>=0.9", "trimesh", "manifold3d", "numpy"]
# ///
# Copyright (C) Krzysztof Krzysztofik
"""XIAO ESP32-S3 thermostat wall box: base + lid.

uv run parts.py --part all            export out/*.stl, out/*.step
uv run parts.py --part all --check    per-part solid checks + interference vs board envelopes
"""
import argparse
import math
import os
import sys
import tempfile

from build123d import (Align, Axis, Box, Compound, Cone, Cylinder, Pos, Rectangle, Rot,
                       export_step, export_stl, fillet, loft)

import params as P

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "out")
MIN = (Align.MIN, Align.MIN, Align.MIN)
CC_MIN = (Align.CENTER, Align.CENTER, Align.MIN)


def wbox(x, y, z):
    """Box from (lo, hi) ranges in world coords."""
    return Pos(x[0], y[0], z[0]) * Box(x[1] - x[0], y[1] - y[0], z[1] - z[0], align=MIN)


def zcyl(x, y, z0, z1, d):
    return Pos(x, y, z0) * Cylinder(d / 2, z1 - z0, align=CC_MIN)


def teardrop_y(x, z, d, y0, y1):
    """Hole along Y, point up (+Z): prints without support with Z as the build axis."""
    r = d / 2
    c = Pos(x, (y0 + y1) / 2, z) * Rot(90, 0, 0) * Cylinder(r, y1 - y0)
    k = Pos(x, (y0 + y1) / 2, z + r / math.sqrt(2)) * Rot(0, 45, 0) * Box(r, y1 - y0, r)
    return c + k


def rounded_slab(z0, z1):
    s = wbox(P.OUTER[0], P.OUTER[1], (z0, z1))
    return fillet(s.edges().filter_by(Axis.Z), P.CORNER_R)


def base():
    z_floor, z_pcb = P.BACK_T, P.BACK_T + P.LEDGE_H
    b = rounded_slab(0, P.Z_RIM) - wbox((0, P.IN_W), (0, P.IN_H), (z_floor, P.Z_RIM + 1))

    # Lid screw columns, merged into the corner.
    for cx, cy in P.BOSSES:
        b += zcyl(cx, cy, z_floor, P.Z_RIM, P.BOSS_D)
        b += wbox(sorted((cx, 0 if cx < P.IN_W / 2 else P.IN_W)), sorted((cy, 0 if cy < P.IN_H / 2 else P.IN_H)), (z_floor, P.Z_RIM))
        b -= zcyl(cx, cy, *P.pilots()["lid"][:2], P.INSERT_D)

    # Wall screws, countersunk from inside.
    for x, y in P.WALL_SCREWS:
        b -= zcyl(x, y, -1, z_floor + 1, P.WOOD_CLEAR)
        h = (P.WOOD_HEAD_D - P.WOOD_CLEAR) / 2
        b -= Pos(x, y, z_floor - h) * Cone(P.WOOD_CLEAR / 2, P.WOOD_HEAD_D / 2, h, align=CC_MIN)
        b -= zcyl(x, y, z_floor, z_floor + 1, P.WOOD_HEAD_D)

    # Shield: rails under the long edges, corner posts at the ends, two snap hooks.
    sx0, sx1 = P.SH_X0, P.SH_X0 + P.SH[1]
    sy0, sy1 = P.SH_Y0, P.SH_Y0 + P.SH[0]
    z_top = z_pcb + P.SH_PCB_T
    for x in ((sx0, sx0 + 1.5), (sx1 - 1.5, sx1)):
        b += wbox(x, (sy0, sy1), (z_floor, z_pcb))
    for x in ((sx0, sx0 + 2), (sx1 - 2, sx1)):
        for y in ((sy0 - P.CLR - 2, sy0 - P.CLR), (sy1 + P.CLR, sy1 + P.CLR + 2)):
            b += wbox(x, y, (z_floor, z_top + 1.0))
    # ponytail: 0.9 deflection on a ~9 mm PLA beam is ~1.8 % strain; print PETG if they crack.
    hy = (P.HOOK_Y - 3, P.HOOK_Y + 3)
    for side in (-1, 1):
        e = sx0 - P.CLR if side < 0 else sx1 + P.CLR      # beam face towards the board
        beam = tuple(sorted((e, e + side * 1.2)))
        lip = tuple(sorted((e, e - side * (0.6 + P.CLR))))
        b -= wbox(tuple(sorted((e - side * 0.6, e + side * 1.8))), hy, (z_floor - 2, z_floor + 0.01))
        b += wbox(beam, hy, (z_floor - 2, z_top + 0.1 + 1.2))
        b += wbox(lip, hy, (z_top + 0.1, z_top + 0.1 + 1.2))

    # Relay: standoffs under its three holes/notches, M2x6 machine screws.
    for h in P.RELAY_HOLES:
        x, y = P.relay_xy(*h)
        b += zcyl(x, y, z_floor, z_pcb, P.POST_D)
        b -= zcyl(x, y, *P.pilots()["relay"][:2], P.M2_PILOT)

    # Bottom wall: USB-C opening, cable teardrops, tie anchors.
    ux, uz = P.usb_center()
    w, h = P.USB_OVERMOLD[0] + 1.0, P.USB_OVERMOLD[1] + 1.0
    b -= wbox((ux - w / 2, ux + w / 2), (-P.WALL - 1, 1), (uz - h / 2, uz + h / 2))
    for _, x, d in P.CABLE_HOLES:
        b -= teardrop_y(x, P.CABLE_Z, d, -P.WALL - 1, 1)
        tx, ty, tz = P.TIE
        b += wbox((x - tx / 2, x + tx / 2), (P.TIE_Y - ty / 2, P.TIE_Y + ty / 2), (z_floor, z_floor + tz))
        sw, sh = P.TIE_SLOT
        b -= wbox((x - tx, x + tx), (P.TIE_Y - sw / 2, P.TIE_Y + sw / 2), (z_floor + 1.0, z_floor + 1.0 + sh))

    # Antenna socket on the top wall, sitting on the bed.
    r = P.ANT_BASE_D / 2 + 0.2
    t = P.ANT_SOCKET_WALL
    y0 = P.IN_H + P.WALL
    zc = t + r
    b += wbox((P.ANT_X - r - t, P.ANT_X + r + t), (y0 - 0.01, y0 + P.ANT_SOCKET_L), (0, 2 * r + 2 * t))
    b -= teardrop_y(P.ANT_X, zc, 2 * r, y0, y0 + P.ANT_SOCKET_L + 1)
    b -= teardrop_y(P.ANT_X, zc, P.ANT_CABLE_HOLE, P.IN_H - 1, y0 + 0.5)
    return b


def lid():
    z0, z1 = P.Z_RIM, P.Z_RIM + P.LID_T
    l = rounded_slab(z0, z1)

    for cx, cy in P.BOSSES:
        l -= zcyl(cx, cy, z0 - 1, z1 + 1, P.M3_CLEAR)
        h = (P.M3_CSK_D - P.M3_CLEAR) / 2
        l -= Pos(cx, cy, z1 - h) * Cone(P.M3_CLEAR / 2, P.M3_CSK_D / 2, h + 0.01, align=CC_MIN)

    # OLED window, 45 deg chamfer outside.
    wx, wy = P.oled_window()
    l -= wbox(wx, wy, (z0 - 1, z1 + 1))
    c = P.WINDOW_CHAMFER
    cx, cy, w, h = (wx[0] + wx[1]) / 2, (wy[0] + wy[1]) / 2, wx[1] - wx[0], wy[1] - wy[0]
    l -= loft([Pos(cx, cy, z1 - c) * Rectangle(w, h), Pos(cx, cy, z1 + 0.01) * Rectangle(w + 2 * c, h + 2 * c)])

    # OLED: pads on the glass-free ends (its holes are too close to the glass).
    zb = z0 - P.OLED_POST
    for xs in (P.OLED_PAD_A, P.OLED_PAD_B):
        l += wbox(*P.rect(P.oled_xy, xs, P.OLED_Y), (zb, z0 + 0.01))

    # Button: cap hole + supports under its three holes/notches.
    l -= zcyl(*P.BTN_C, z0 - 1, z1 + 1, P.BTN_HOLE)
    for hh in P.BTN_HOLES:
        l += zcyl(*P.btn_xy(*hh), z0 - P.BTN_POST, z0 + 0.01, P.POST_D)

    for n in ("oled", "button"):
        add, cut = slide_mount(n)
        l = l + add - cut
    return l


def slide_mount(name):
    """Two lips over the T face at the board's -x (connector) end, a post for the tab at +x.
    Returns (material to add, M2 hole to cut)."""
    f, xs, ys, lip_y = P.MODULES[name]
    zm, zb, d = P.module_z(name)
    zt = zb + d * P.GROVE_PCB_T
    za, zl = sorted((zt + d * 0.1, zt + d * (0.1 + P.LIP_T)))     # lip slab
    zw = sorted((zm - d * 0.01, zt + d * (0.1 + P.LIP_T)))        # wall, from the mounting surface
    x0 = xs[0]
    add = []
    for yr in ((lip_y, ys[1]), (ys[0], -lip_y)):
        add.append(wbox(*P.rect(f, (x0 - P.CLR - P.LIP_T, x0 - P.CLR), yr), zw))
        add.append(wbox(*P.rect(f, (x0 - P.CLR - P.LIP_T, x0 + P.LIP), yr), (za, zl)))
    px, py = P.tab_post(name)
    add.append(zcyl(px, py, *sorted((zm - d * 0.01, zt)), P.TAB_POST_D))
    out = add[0]
    for s in add[1:]:
        out += s
    lo, hi, _, _ = P.pilots()["tab_" + name]
    return out, zcyl(px, py, lo, hi, P.M2_PILOT)


def tab():
    """Locking tab, local frame: lies on z=0 (prints flat), hole at the origin, overlap towards -x."""
    r = P.TAB_POST_D / 2
    L = r + P.CLR + P.TAB_POST_D / 2 + P.TAB_OVER
    t = Pos(r, 0, 0) * Box(L, P.TAB_W, P.TAB_T, align=(Align.MAX, Align.CENTER, Align.MIN))
    return t - Cylinder(P.TAB_HOLE / 2, 3 * P.TAB_T)


def tabs_placed():
    """Tabs in the assembly: on the T face, overlap pointing at the board."""
    out = {}
    for n, (f, xs, _, _) in P.MODULES.items():
        zm, zb, d = P.module_z(n)
        zt = zb + d * P.GROVE_PCB_T
        px, py = P.tab_post(n)
        bx, by = f(xs[1], 0.0)                     # board +x edge mid, world
        ang = math.degrees(math.atan2(by - py, bx - px)) + 180   # local -x -> towards the board
        s = Rot(0, 0, ang) * tab()
        if d < 0:
            s = Pos(0, 0, -P.TAB_T) * s
        out["tab_" + n] = Pos(px, py, zt) * s
    return out


def envelopes():
    """Boards and their tall parts, for interference only."""
    e = {}
    zp = P.BACK_T + P.LEDGE_H
    zt = zp + P.SH_PCB_T
    sx, sy = P.rect(P.shield_xy, (0, P.SH[0]), (0, P.SH[1]))
    e["shield"] = wbox(sx, sy, (zp, zt))
    xx, xy = P.rect(P.shield_xy, *P.XIAO_B)
    e["xiao"] = wbox(xx, xy, (zt, zt + P.HDR_H + P.XIAO_PCB_T))
    ux, uz = P.usb_center()
    e["usb"] = wbox((ux - 4.5, ux + 4.5), (P.SH_Y0 - P.XIAO_USB_PROTRUDE, P.SH_Y0 + 7.3), (uz - P.USB_H / 2, uz + P.USB_H / 2))
    for i, (bx, by) in enumerate(P.GROVE_SH_B):
        gx, gy = P.rect(P.shield_xy, (bx - P.GROVE_SOCKET[0] / 2, bx + P.GROVE_SOCKET[0] / 2),
                        (by - P.GROVE_SOCKET[1] / 2, by + P.GROVE_SOCKET[1] / 2))
        e[f"grove{i}"] = wbox(gx, gy, (zt, zt + P.GROVE_PLUG_UP))

    zt = zp + P.GROVE_PCB_T
    e["relay"] = wbox(*P.rect(P.relay_xy, P.RELAY_X, P.RELAY_Y), (zp, zt))
    e["relay_body"] = wbox(*P.rect(P.relay_xy, *P.RELAY_BODY), (zt, zt + P.RELAY_BODY_H))
    e["relay_grove"] = wbox(*P.rect(P.relay_xy, *P.RELAY_GROVE), (zt, zt + P.GROVE_PLUG_UP))

    zb = P.Z_RIM - P.OLED_POST
    e["oled"] = wbox(*P.rect(P.oled_xy, P.OLED_X, P.OLED_Y), (zb - P.GROVE_PCB_T, zb))
    e["oled_glass"] = wbox(*P.rect(P.oled_xy, *P.OLED_PANEL), (zb, zb + P.OLED_PANEL_H))
    e["oled_grove"] = wbox(*P.rect(P.oled_xy, *P.OLED_GROVE), (zb - P.GROVE_PCB_T - P.GROVE_CONN_H, zb - P.GROVE_PCB_T))

    zb = P.Z_RIM - P.BTN_POST
    e["button"] = wbox(*P.rect(P.btn_xy, P.BTN_X, P.BTN_Y), (zb - P.GROVE_PCB_T, zb))
    s = P.BTN_SW / 2
    e["button_sw"] = wbox((P.BTN_C[0] - s, P.BTN_C[0] + s), (P.BTN_C[1] - s, P.BTN_C[1] + s), (zb, zb + P.BTN_SW_H - P.BTN_CAP_H))
    e["button_cap"] = zcyl(*P.BTN_C, zb + P.BTN_SW_H - P.BTN_CAP_H, zb + P.BTN_SW_H, P.BTN_CAP_D)
    e["button_grove"] = wbox(*P.rect(P.btn_xy, *P.BTN_GROVE), (zb - P.GROVE_PCB_T - P.GROVE_CONN_H, zb - P.GROVE_PCB_T))
    return e


def to_print(shape, flip):
    s = Rot(180, 0, 0) * shape if flip else shape
    bb = s.bounding_box()
    return Pos(-bb.min.X, -bb.min.Y, -bb.min.Z) * s


def check_part(name, shape):
    ok = True
    n = len(shape.solids())
    shells = len(shape.solids()[0].shells()) if n else 0
    bb = shape.bounding_box()
    size = (bb.size.X, bb.size.Y, bb.size.Z)
    for cond, msg in [(n == 1, f"{n} solid(s)"), (shells == 1, f"{shells} shell(s)"),
                      (shape.volume > 0, f"volume {shape.volume:.1f} mm3"),
                      (all(s <= b for s, b in zip(size, P.BED)), f"size {tuple(round(s, 1) for s in size)} fits bed")]:
        print(("ok   " if cond else "FAIL ") + f"{name}: {msg}")
        ok &= cond
    return ok


def check_assembly(parts):
    """Pairwise mesh intersection; flush contact is fine. Envelopes of one board may touch."""
    import trimesh
    tmp = tempfile.mkdtemp()
    meshes = {}
    for name, s in parts.items():
        f = os.path.join(tmp, name + ".stl")
        export_stl(s, f, tolerance=0.01)
        meshes[name] = trimesh.load(f)
        if not meshes[name].is_volume:
            print(f"FAIL {name}: exported mesh is not a closed volume")
    group = lambda n: "xiao" if n in ("xiao", "usb") else n.split("_")[0].rstrip("0123456789").replace("grove", "shield")
    ok = True
    names = list(meshes)
    pairs = [(a, b) for i, a in enumerate(names) for b in names[i + 1:] if group(a) != group(b) or a in ("base", "lid")]
    for a, b in pairs:
        v = meshes[a].intersection(meshes[b], engine="manifold").volume
        if v > 0.01:
            print(f"FAIL {a} x {b}: {v:.3f} mm3")
            ok = False
    print(("ok   " if ok else "FAIL ") + f"interference: {len(pairs)} pairs")
    return ok


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--part", default="all", choices=["all", "base", "lid", "tab"])
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()

    shapes = {}
    if a.part in ("all", "base"):
        shapes["base"] = base()
    if a.part in ("all", "lid"):
        shapes["lid"] = lid()
    if a.part in ("all", "tab"):
        shapes["tab"] = tab()

    if a.check:
        ok = all([check_part(n, s) for n, s in shapes.items()])
        if a.part == "all":
            placed = {k: v for k, v in shapes.items() if k != "tab"}
            ok &= check_assembly({**placed, **tabs_placed(), **envelopes()})
        print("\nall green" if ok else "\nFAILED")
        sys.exit(0 if ok else 1)

    os.makedirs(OUT, exist_ok=True)
    for n, s in shapes.items():
        fn = "thermostat_tab_x2" if n == "tab" else f"thermostat_{n}"
        export_stl(to_print(s, flip=(n == "lid")), os.path.join(OUT, fn + ".stl"), tolerance=0.01)
        export_step(s, os.path.join(OUT, f"thermostat_{n}.step"))
    if a.part == "all":
        placed = [v for k, v in shapes.items() if k != "tab"]
        export_step(Compound([*placed, *tabs_placed().values(), *envelopes().values()]), os.path.join(OUT, "assembly_preview.step"))
    print("wrote", sorted(os.listdir(OUT)))


if __name__ == "__main__":
    main()

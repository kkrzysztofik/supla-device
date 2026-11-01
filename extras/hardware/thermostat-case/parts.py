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

import params as P
from build123d import (
    Align,
    Axis,
    Box,
    Compound,
    Cone,
    Cylinder,
    Pos,
    Rectangle,
    Rot,
    export_step,
    export_stl,
    fillet,
    loft,
)

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "out")
MIN = (Align.MIN, Align.MIN, Align.MIN)
CC_MIN = (Align.CENTER, Align.CENTER, Align.MIN)


def wbox(x, y, z):
    """Box from (lo, hi) ranges in world coords."""
    return Pos(x[0], y[0], z[0]) * Box(x[1] - x[0], y[1] - y[0], z[1] - z[0], align=MIN)


def zcyl(x, y, z0, z1, d):
    """Vertical cylinder from z0 to z1, diameter d, axis on (x, y)."""
    return Pos(x, y, z0) * Cylinder(d / 2, z1 - z0, align=CC_MIN)


def ycyl(x, z, r, y0, y1):
    """Cylinder along Y from y0 to y1, radius r, axis on (x, z)."""
    return Pos(x, (y0 + y1) / 2, z) * Rot(90, 0, 0) * Cylinder(r, y1 - y0)


def teardrop_y(x, z, d, y0, y1):
    """Hole along Y, point up (+Z): prints without support with Z as the build axis."""
    r = d / 2
    c = Pos(x, (y0 + y1) / 2, z) * Rot(90, 0, 0) * Cylinder(r, y1 - y0)
    k = Pos(x, (y0 + y1) / 2, z + r / math.sqrt(2)) * Rot(0, 45, 0) * Box(r, y1 - y0, r)
    return c + k


def rounded_slab(z0, z1):
    """Box footprint with rounded vertical corners, between z0 and z1."""
    s = wbox(P.OUTER[0], P.OUTER[1], (z0, z1))
    return fillet(s.edges().filter_by(Axis.Z), P.CORNER_R)


def base():
    """Base: floor and walls, standoffs, openings, probe clamp saddle and antenna socket."""
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

    # Shield (no mounting holes): rails under the long edges, corner posts at the ends,
    # three tab posts; the tabs clamp its edges down with M2x6.
    sx0, sx1 = P.SH_X0, P.SH_X0 + P.SH[1]
    sy0, sy1 = P.SH_Y0, P.SH_Y0 + P.SH[0]
    z_top = z_pcb + P.SH_PCB_T
    for x in ((sx0, sx0 + 1.5), (sx1 - 1.5, sx1)):
        b += wbox(x, (sy0, sy1), (z_floor, z_pcb))
    for x in ((sx0, sx0 + 2), (sx1 - 2, sx1)):
        for y in ((sy0 - P.CLR - 2, sy0 - P.CLR), (sy1 + P.CLR, sy1 + P.CLR + 2)):
            b += wbox(x, y, (z_floor, z_top + 1.0))
    for (px, py), _ in P.shield_tab_sites():
        b += zcyl(px, py, z_floor - 0.01, z_top, P.TAB_POST_D)
        b -= zcyl(px, py, *P.pilots()["tab_shield"][:2], P.M2_PILOT)

    # Relay: standoffs under its three holes/notches, M2x6 machine screws.
    for h in P.RELAY_HOLES:
        x, y = P.relay_xy(*h)
        b += zcyl(x, y, z_floor, z_pcb, P.POST_D)
        b -= zcyl(x, y, *P.pilots()["relay"][:2], P.M2_PILOT)

    # Bottom wall: USB-C opening, cable teardrops, tie anchors.
    ux, uz = P.usb_center()
    _face, pocket, bezel = P.usb_port()
    rw, rh = P.USB_RECEPT[0] + 0.6, P.USB_RECEPT[1] + 0.6
    b -= wbox((ux - rw / 2, ux + rw / 2), (pocket, 1), (uz - rh / 2, uz + rh / 2))
    sw, sh = P.USB_SHELL[0] + 0.4 + P.HOLE_COMP, P.USB_SHELL[1] + 0.4 + P.HOLE_COMP   # stadium slot
    b -= wbox((ux - (sw - sh) / 2, ux + (sw - sh) / 2), (-P.WALL - 1, pocket + 0.01), (uz - sh / 2, uz + sh / 2))
    for dx in (-(sw - sh) / 2, (sw - sh) / 2):
        b -= ycyl(ux + dx, uz, sh / 2, -P.WALL - 1, pocket + 0.01)
    ow, oh = P.USB_OVERMOLD[0] + 1.0, P.USB_OVERMOLD[1] + 1.0
    b -= wbox((ux - ow / 2, ux + ow / 2), (-P.WALL - 1, bezel), (uz - oh / 2, uz + oh / 2))
    for n, x, d in P.CABLE_HOLES:
        b -= teardrop_y(x, P.CABLE_Z, d, -P.WALL - 1, 1)
        if n == "probe":
            continue
        tx, ty, tz = P.TIE
        b += wbox((x - tx / 2, x + tx / 2), (P.TIE_Y - ty / 2, P.TIE_Y + ty / 2), (z_floor, z_floor + tz))
        sw, sh = P.TIE_SLOT
        b -= wbox((x - tx, x + tx), (P.TIE_Y - sw / 2, P.TIE_Y + sw / 2), (z_floor + 1.0, z_floor + 1.0 + sh))

    # Probe strain relief: saddle with a round groove, two pilots for the clamp bar.
    x = P.clamp_x()
    zs, zc = P.clamp_z()
    r = P.PROBE_CABLE_D / 2
    b += wbox((x - P.CLAMP_W / 2, x + P.CLAMP_W / 2), P.CLAMP_Y, (z_floor - 0.01, zs))
    b -= ycyl(x, zc, r, P.CLAMP_Y[0] - 1, P.CLAMP_Y[1] + 1)
    for dx in (-P.CLAMP_DX, P.CLAMP_DX):
        b -= zcyl(x + dx, sum(P.CLAMP_Y) / 2, *P.pilots()["clamp"][:2], P.M2_PILOT)

    # Antenna socket on the top wall, sitting on the bed. The block is taller than the teardrop
    # apex, so the bore keeps a closed ring of plastic at the top instead of slitting the wall.
    r = P.ANT_BORE_R
    t = P.ANT_SOCKET_WALL
    y0 = P.IN_H + P.WALL
    zc = t + r
    b += wbox((P.ANT_X - r - t, P.ANT_X + r + t), (y0 - 0.01, y0 + P.ANT_SOCKET_L), (0, P.ANT_SOCKET_H))
    b -= teardrop_y(P.ANT_X, zc, 2 * r, y0, y0 + P.ANT_SOCKET_L + 1)
    b -= teardrop_y(P.ANT_X, zc, P.ANT_CABLE_HOLE, P.IN_H - 1, y0 + 0.5)
    return b


def lid():
    """Lid plate: module window and cap hole, mount posts, washer pilots and insert columns."""
    z0, z1 = P.Z_RIM, P.Z_RIM + P.LID_T
    s = rounded_slab(z0, z1)

    for cx, cy in P.BOSSES:
        s -= zcyl(cx, cy, z0 - 1, z1 + 1, P.M3_CLEAR)
        h = (P.M3_CSK_D - P.M3_CLEAR) / 2
        s -= Pos(cx, cy, z1 - h) * Cone(P.M3_CLEAR / 2, P.M3_CSK_D / 2, h + 0.01, align=CC_MIN)

    # OLED window, 45 deg chamfer outside.
    wx, wy = P.oled_window()
    s -= wbox(wx, wy, (z0 - 1, z1 + 1))
    c = P.WINDOW_CHAMFER
    cx, cy, w, h = (wx[0] + wx[1]) / 2, (wy[0] + wy[1]) / 2, wx[1] - wx[0], wy[1] - wy[0]
    s -= loft([Pos(cx, cy, z1 - c) * Rectangle(w, h), Pos(cx, cy, z1 + 0.01) * Rectangle(w + 2 * c, h + 2 * c)])

    # Button: cap hole, pocket for the switch body in the inner face.
    s -= zcyl(*P.BTN_C, z0 - 1, z1 + 1, P.BTN_HOLE)
    s -= wbox(*P.rect(P.btn_xy, *[(a - P.MOD_CLR, b + P.MOD_CLR) for a, b in P.BTN_SW_B]), (z0 - 1, z0 + P.BTN_POCKET))

    for n in P.MODULES:
        s += mount_posts(n)
        f = P.MODULES[n][0]
        zf, zk = P.module_z(n)
        lo, hi, _, _ = P.pilots()["mod_" + n]
        for sx, sy in P.MOD_SCREWS:
            x, y = f(sx, sy)
            s -= zcyl(x, y, zk + P.MOD_POST_LIFT, zf, P.MOD_SCREW_CLEAR)   # shank passes the locator band
            s -= zcyl(x, y, lo, hi, P.M2_PILOT)
    return s


def mount_posts(name):
    """A post at every mount point (screw notches + locator): shoulder under the board front,
    clipped off the glass / switch, and outside the board it rises past the PCB edge (from
    MOD_POST_LIFT behind its back face) to locate it."""
    f, _ = P.MODULES[name]
    zf, zk = P.module_z(name)
    c = P.MOD_CLR
    pts = (*P.MOD_SCREWS, *P.MOD_LOCATORS)
    posts = [zcyl(*f(x, y), zk + P.MOD_POST_LIFT, P.Z_RIM + 0.01, P.NOTCH_POST_D) for x, y in pts]
    out = posts[0]
    for post in posts[1:]:
        out += post
    out -= wbox(*P.rect(f, (-c, P.MOD[0] + c), (-c, P.MOD[1] + c)), (zk - 1, zf))
    (kx0, kx1), (ky0, ky1) = P.MOD_FRONT_KEEPOUT[name]
    return out - wbox(*P.rect(f, (kx0 - P.CLR, kx1 + P.CLR), (ky0 - P.CLR, ky1 + P.CLR)), (zf - 0.01, P.Z_RIM))


def tab():
    """Locking tab, local frame: lies on z=0 (prints flat), hole at the origin, overlap towards -x."""
    r = P.TAB_POST_D / 2
    L = r + P.CLR + P.TAB_POST_D / 2 + P.TAB_OVER
    t = Pos(r, 0, 0) * Box(L, P.TAB_W, P.TAB_T, align=(Align.MAX, Align.CENTER, Align.MIN))
    return t - Cylinder(P.TAB_HOLE / 2, 3 * P.TAB_T)


def clamp_bar():
    """Local frame: bottom face on z=0 (groove side), centred on the cable."""
    zs, zc = P.clamp_z()
    zb = zs + P.CLAMP_GAP
    L = P.CLAMP_Y[1] - P.CLAMP_Y[0]
    bar = Box(P.CLAMP_W, L, P.CLAMP_BAR_T, align=CC_MIN)
    bar -= ycyl(0, zc - P.CLAMP_SQUEEZE - zb, P.PROBE_CABLE_D / 2, -L, L)
    for dx in (-P.CLAMP_DX, P.CLAMP_DX):
        bar -= Pos(dx, 0, -1) * Cylinder(P.TAB_HOLE / 2, P.CLAMP_BAR_T + 2, align=CC_MIN)
    return bar


def clamp_placed():
    """Clamp bar in the assembly: on the saddle, groove down."""
    zs, _ = P.clamp_z()
    return Pos(P.clamp_x(), sum(P.CLAMP_Y) / 2, zs + P.CLAMP_GAP) * clamp_bar()


def washer():
    """Lid module notch washer, flat on z=0."""
    return Cylinder(P.WASHER_D / 2, P.WASHER_T, align=CC_MIN) - Cylinder(P.TAB_HOLE / 2, 3 * P.WASHER_T)


def fixings_placed():
    """Shield tabs on its T face, overlap pointing at the board edge; washers on the lid modules' backs."""
    out = {}
    zt = P.BACK_T + P.LEDGE_H + P.SH_PCB_T
    for i, ((px, py), (ex, ey)) in enumerate(P.shield_tab_sites()):
        ang = math.degrees(math.atan2(ey - py, ex - px)) + 180   # local -x -> towards the board
        out[f"tab_shield{i}"] = Pos(px, py, zt) * Rot(0, 0, ang) * tab()
    for n, (f, _) in P.MODULES.items():
        _, zk = P.module_z(n)
        for i, (sx, sy) in enumerate(P.MOD_SCREWS):
            out[f"washer_{n}{i}"] = Pos(*f(sx, sy), zk - P.WASHER_T) * washer()
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
    e["usb"] = wbox((ux - P.USB_RECEPT[0] / 2, ux + P.USB_RECEPT[0] / 2), (P.usb_port()[0], P.SH_Y0 + 7.3), (uz - P.USB_RECEPT[1] / 2, uz + P.USB_RECEPT[1] / 2))
    for i, (bx, by) in enumerate(P.GROVE_SH_B):
        gx, gy = P.rect(P.shield_xy, (bx - P.GROVE_SOCKET[0] / 2, bx + P.GROVE_SOCKET[0] / 2),
                        (by - P.GROVE_SOCKET[1] / 2, by + P.GROVE_SOCKET[1] / 2))
        e[f"grove{i}"] = wbox(gx, gy, (zt, zt + P.GROVE_PLUG_UP))

    zt = zp + P.GROVE_PCB_T
    e["relay"] = wbox(*P.rect(P.relay_xy, P.RELAY_X, P.RELAY_Y), (zp, zt))
    e["relay_body"] = wbox(*P.rect(P.relay_xy, *P.RELAY_BODY), (zt, zt + P.RELAY_BODY_H))
    e["relay_grove"] = wbox(*P.rect(P.relay_xy, *P.RELAY_GROVE), (zt, zt + P.GROVE_PLUG_UP))

    for n, (f, _) in P.MODULES.items():
        zf, zk = P.module_z(n)
        e[n] = wbox(*P.rect(f, (0, P.MOD[0]), (0, P.MOD[1])), (zk, zf))
        gx, gy = P.rect(f, *P.MOD_GROVE)
        e[n + "_grove"] = wbox((gx[0] - P.MOD_PLUG_OUT, gx[1]), gy, (zk - P.GROVE_CONN_H, zk))

    zf, zk = P.module_z("oled")
    e["oled_glass"] = wbox(*P.rect(P.oled_xy, *P.OLED_GLASS), (zf, zf + P.OLED_GLASS_H))
    e["oled_fpc"] = wbox(*P.rect(P.oled_xy, *P.OLED_FPC), (zk, zf + P.OLED_GLASS_H))

    zf, _ = P.module_z("button")
    e["button_sw"] = wbox(*P.rect(P.btn_xy, *P.BTN_SW_B), (zf, zf + P.BTN_BODY_H))
    e["button_cap"] = zcyl(*P.BTN_C, zf + P.BTN_BODY_H, zf + P.BTN_TOP_H, P.BTN_CAP_D)
    return e


def to_print(shape, flip):
    """Shape moved to the bed corner, flipped for the parts that print face-down."""
    s = Rot(180, 0, 0) * shape if flip else shape
    bb = s.bounding_box()
    return Pos(-bb.min.X, -bb.min.Y, -bb.min.Z) * s


def check_part(name, shape):
    """One solid, one shell, positive volume, fits the print bed."""
    ok = True
    n = len(shape.solids())
    shells = len(shape.solids()[0].shells()) if n else 0
    bb = shape.bounding_box()
    size = (bb.size.X, bb.size.Y, bb.size.Z)
    for cond, msg in [(n == 1, f"{n} solid(s)"), (shells == 1, f"{shells} shell(s)"),
                      (shape.volume > 0, f"volume {shape.volume:.1f} mm3"),
                      (all(s <= b for s, b in zip(size, P.BED, strict=True)), f"size {tuple(round(s, 1) for s in size)} fits bed")]:
        print(("ok   " if cond else "FAIL ") + f"{name}: {msg}")
        ok &= cond
    return ok


def check_screw_paths(lid_shape):
    """Each M2 shank must reach its pilot through air: no lid material in the locator band.

    The Ø1.6 pilot starts at the board front face. Below it the shank crosses the band where the
    post locates the PCB edge, so that band needs a MOD_SCREW_CLEAR bore at every notch.
    """
    ok = True
    for n, (f, _) in P.MODULES.items():
        zf, zk = P.module_z(n)
        for sx, sy in P.MOD_SCREWS:
            x, y = f(sx, sy)
            v = (zcyl(x, y, zk + P.MOD_POST_LIFT, zf, P.M2_SHANK) & lid_shape).volume
            if v > 0.01:
                print(f"FAIL screw {n} at ({sx:.2f}, {sy:.2f}): {v:.3f} mm3 of lid blocks the shank below the pilot")
                ok = False
    print(("ok   " if ok else "FAIL ") + f"module screw paths: {len(P.MODULES) * len(P.MOD_SCREWS)} notch screws")
    return ok


def check_assembly(parts):
    """Pairwise mesh intersection; flush contact is fine. Envelopes of one board may touch.

    A mesh that is not a closed volume is a failure too: manifold intersection volumes are
    meaningless for an open mesh, so it could hide real interference.
    """
    import trimesh
    ok = True
    with tempfile.TemporaryDirectory() as tmp:
        meshes = {}
        for name, s in parts.items():
            f = os.path.join(tmp, name + ".stl")
            export_stl(s, f, tolerance=0.01)
            meshes[name] = trimesh.load(f)
            if not meshes[name].is_volume:
                print(f"FAIL {name}: exported mesh is not a closed volume")
                ok = False

    def group(n):
        """Mesh name -> board group; envelopes of one board are allowed to touch."""
        return "xiao" if n in ("xiao", "usb") else n.split("_")[0].rstrip("0123456789").replace("grove", "shield")

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
    """Export STLs/STEPs; with --check also run the per-part and assembly checks."""
    ap = argparse.ArgumentParser()
    ap.add_argument("--part", default="all", choices=["all", "base", "lid", "tab", "washer", "clamp"])
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()

    shapes = {}
    if a.part in ("all", "base"):
        shapes["base"] = base()
    if a.part in ("all", "lid"):
        shapes["lid"] = lid()
    if a.part in ("all", "tab"):
        shapes["tab"] = tab()
    if a.part in ("all", "washer"):
        shapes["washer"] = washer()
    if a.part in ("all", "clamp"):
        shapes["clamp"] = clamp_bar()

    if a.check:
        ok = all(check_part(n, s) for n, s in shapes.items())
        if a.part == "all":
            ok &= check_screw_paths(shapes["lid"])
            placed = {k: v for k, v in shapes.items() if k not in ("tab", "washer", "clamp")}
            ok &= check_assembly({**placed, **fixings_placed(), "clampbar": clamp_placed(), **envelopes()})
        print("\nall green" if ok else "\nFAILED")
        sys.exit(0 if ok else 1)

    os.makedirs(OUT, exist_ok=True)
    for n, s in shapes.items():
        fn = {"tab": "thermostat_tab_x3", "washer": "thermostat_washer_x6"}.get(n, f"thermostat_{n}")
        export_stl(to_print(s, flip=(n in ("lid", "clamp"))), os.path.join(OUT, fn + ".stl"), tolerance=0.01)
        export_step(s, os.path.join(OUT, f"thermostat_{n}.step"))
    if a.part == "all":
        placed = [v for k, v in shapes.items() if k not in ("tab", "washer", "clamp")]
        export_step(Compound([*placed, *fixings_placed().values(), clamp_placed(), *envelopes().values()]), os.path.join(OUT, "assembly_preview.step"))
    print("wrote", sorted(os.listdir(OUT)))


if __name__ == "__main__":
    main()

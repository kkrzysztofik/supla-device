# /// script
# requires-python = ">=3.10,<3.14"
# dependencies = ["build123d>=0.9", "trimesh", "manifold3d", "numpy"]
# ///
# Copyright (C) Krzysztof Krzysztofik
"""Display HAT Mini top plate + button plungers for Printables 1257367.

uv run parts.py --part all            export out/*.stl, out/*.step
uv run parts.py --part all --check    per-part solid checks + interference vs ref/*.stl
"""
import argparse
import glob
import os
import sys
import tempfile

from build123d import (Align, Box, Cylinder, Location, Pos, Rectangle, Rot,
                       export_step, export_stl, loft)

import params as P

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "out")
MIN = (Align.MIN, Align.MIN, Align.MIN)
CC_MIN = (Align.CENTER, Align.CENTER, Align.MIN)


def wbox(x, y, z):
    """Box from (lo, hi) ranges in world coords."""
    return Pos(x[0], y[0], z[0]) * Box(x[1] - x[0], y[1] - y[0], z[1] - z[0], align=MIN)


def board_rect(bx, by):
    """Board-frame (lo, hi) ranges -> world x, y ranges."""
    a, b = P.b2w(bx[0], by[0]), P.b2w(bx[1], by[1])
    return sorted((a[0], b[0])), sorted((a[1], b[1]))


def plate():
    z0, z1 = P.PLATE_Z
    p = wbox(P.PLATE_X, P.PLATE_Y, P.PLATE_Z)

    # Screen window, 45 deg chamfer on the outside face.
    m = P.WINDOW_MARGIN
    wx, wy = board_rect((P.ACTIVE_B[0][0] - m, P.ACTIVE_B[0][1] + m), (P.ACTIVE_B[1][0] - m, P.ACTIVE_B[1][1] + m))
    p -= wbox(wx, wy, (z0 - 1, z1 + 1))
    c = P.WINDOW_CHAMFER
    cx, cy, w, h = (wx[0] + wx[1]) / 2, (wy[0] + wy[1]) / 2, wx[1] - wx[0], wy[1] - wy[0]
    p -= loft([Pos(cx, cy, z1 - c) * Rectangle(w, h), Pos(cx, cy, z1 + 0.01) * Rectangle(w + 2 * c, h + 2 * c)])

    for b in P.BUTTONS_B:
        x, y = P.b2w(*b)
        p -= Pos(x, y, z0 - 1) * Cylinder(P.PLUNGER_HOLE / 2, 4, align=CC_MIN)
    x, y = P.b2w(*P.LED_B)
    p -= Pos(x, y, z0 - 1) * Cylinder(P.LED_HOLE / 2, 4, align=CC_MIN)

    # Posts for the HAT's M2.5 holes; blind pilot hole leaves a 0.5 skin on the face.
    for h in P.HOLES_B:
        x, y = P.b2w(*h)
        p += Pos(x, y, z0 - P.POST_LEN) * Cylinder(P.POST_D / 2, P.POST_LEN + 0.01, align=CC_MIN)
        p -= Pos(x, y, z0 - P.POST_LEN - 0.01) * Cylinder(P.POST_HOLE / 2, P.POST_LEN + 1.0, align=CC_MIN)
    return p


def plunger():
    """Local frame: switch contact at z=0, D-flange flat faces +Y (towards the glass)."""
    r = P.PLUNGER_D / 2
    s = Cylinder(r, P.plunger_len(), align=CC_MIN)
    f = Cylinder(P.FLANGE_D / 2, P.FLANGE_T, align=CC_MIN)
    f &= Pos(0, r, 0) * Box(P.FLANGE_D, P.FLANGE_D, P.FLANGE_T, align=(Align.CENTER, Align.MAX, Align.MIN))
    return s + f


def plungers_placed():
    out = []
    z = P.z_pcb_top() + P.H_SWITCH
    for b in P.BUTTONS_B:
        x, y = P.b2w(*b)
        glass_up = b[0] < P.BOARD[0] / 2   # glass is at larger bx -> world +Y
        out.append(Location((x, y, z), (0, 0, 0 if glass_up else 180)) * plunger())
    return out


def hat():
    """Envelope of the HAT, socket and ribbon connector, for interference only."""
    zt = P.z_pcb_top()
    bx, by = board_rect((0, P.BOARD[0]), (0, P.BOARD[1]))
    parts = [wbox(bx, by, (zt - P.PCB_T, zt))]
    gx, gy = board_rect(P.GLASS_BX, P.ACTIVE_B[1])
    parts.append(wbox(gx, gy, (zt, zt + P.H_GLASS)))
    for b in P.BUTTONS_B:
        x, y = P.b2w(*b)
        parts.append(Pos(x, y, zt) * Box(6.0, 3.5, P.H_SWITCH, align=CC_MIN))
    hx, hy = board_rect(*P.HEADER_B)
    z = zt - P.PCB_T
    parts.append(wbox(hx, hy, (z - P.H_SOCKET, z)))
    parts.append(wbox(hx, hy, (z - P.H_SOCKET - P.H_IDC, z - P.H_SOCKET)))
    return parts


def to_print(shape, flip):
    """Plate prints face-down, plunger flange-down; drop onto z=0."""
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


def check_assembly(new):
    import trimesh
    tmp = tempfile.mkdtemp()
    meshes = {}
    for name, s in new.items():
        f = os.path.join(tmp, name + ".stl")
        export_stl(s, f, tolerance=0.01)
        meshes[name] = trimesh.load(f)
        if not meshes[name].is_volume:
            print(f"FAIL {name}: exported mesh is not a closed volume")
    case = {}
    for f in sorted(glob.glob(os.path.join(HERE, "ref", "case_*.stl"))):
        m = trimesh.load(f)
        if not m.is_watertight:
            print(f"skip {os.path.basename(f)}: not watertight (well below the HAT)")
            continue
        case[os.path.basename(f)[:-4]] = m
    ok = True
    names = list(meshes)
    pairs = [(a, b) for a in names for b in case] + [(names[i], names[j]) for i in range(len(names)) for j in range(i + 1, len(names))]
    for a, b in pairs:
        v = meshes[a].intersection(meshes.get(b, case.get(b)), engine="manifold").volume
        if v > 0.01:   # flush contact is fine
            print(f"FAIL {a} x {b}: {v:.3f} mm3")
            ok = False
    print(("ok   " if ok else "FAIL ") + f"interference: {len(pairs)} pairs vs {sorted(case)}")
    return ok


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--part", default="all", choices=["all", "plate", "plunger"])
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()

    pl, pu = plate(), plunger()
    if a.check:
        ok = True
        if a.part in ("all", "plate"):
            ok &= check_part("plate", pl)
        if a.part in ("all", "plunger"):
            ok &= check_part("plunger", pu)
        if a.part == "all":
            new = {"plate": pl}
            new.update({f"plunger{i}": s for i, s in enumerate(plungers_placed())})
            new.update({f"hat{i}": s for i, s in enumerate(hat())})
            ok &= check_assembly(new)
        print("\nall green" if ok else "\nFAILED")
        sys.exit(0 if ok else 1)

    os.makedirs(OUT, exist_ok=True)
    if a.part in ("all", "plate"):
        export_stl(to_print(pl, flip=True), os.path.join(OUT, "top_display_hat_mini.stl"), tolerance=0.01)
        export_step(pl, os.path.join(OUT, "top_display_hat_mini.step"))
    if a.part in ("all", "plunger"):
        export_stl(to_print(pu, flip=False), os.path.join(OUT, "button_plunger_x4.stl"), tolerance=0.01)
    if a.part == "all":
        from build123d import Compound
        export_step(Compound([pl, *plungers_placed(), *hat()]), os.path.join(OUT, "assembly_preview.step"))
    print("wrote", sorted(os.listdir(OUT)))


if __name__ == "__main__":
    main()

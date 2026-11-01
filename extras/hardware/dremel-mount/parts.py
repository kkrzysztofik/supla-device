# /// script
# requires-python = ">=3.10,<3.14"
# dependencies = ["build123d>=0.9", "trimesh", "manifold3d", "numpy"]
# ///
# Copyright (C) Krzysztof Krzysztofik
"""Dremel 3000 mount for the HeatSync insert press (replaces the soldering-iron collar).

uv run parts.py            export out/dremel_mount.stl, .step, assembly_preview.step
uv run parts.py --check    solid checks + interference vs the real HeatSync rack and a Dremel envelope
"""
import argparse
import os
import sys
import tempfile
import xml.etree.ElementTree as ET
import zipfile

from build123d import (Align, Box, Compound, Cone, Cylinder, Plane, Polyline, Pos, Rot,
                       export_step, export_stl, extrude, make_face)

import params as P

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "out")
MIN = (Align.MIN, Align.MIN, Align.MIN)
CC_MIN = (Align.CENTER, Align.CENTER, Align.MIN)
HEATSYNC_3MF = os.path.join(HERE, "A1_HeatSync.3mf")


def wbox(x, y, z):
    """Box from (lo, hi) ranges."""
    return Pos(x[0], y[0], z[0]) * Box(x[1] - x[0], y[1] - y[0], z[1] - z[0], align=MIN)


def zcyl(y, z0, z1, r):
    """Cylinder on the Dremel axis line x = 0."""
    return Pos(0, y, z0) * Cylinder(r, z1 - z0, align=CC_MIN)


def xcyl(y, z, x0, x1, r):
    return Pos((x0 + x1) / 2, y, z) * Rot(0, 90, 0) * Cylinder(r, x1 - x0)


def mount():
    ay, w, hb = P.AXIS_Y, P.W, P.H_BASE
    # base: round-fronted block carrying the nose cup, ears reaching back around the rack block
    base = wbox((-w, w), (ay, 0), (0, hb)) + zcyl(ay, 0, hb, P.CUP_R)
    base += wbox((-P.EAR_X, P.EAR_X), (-1, P.SLOT_DEPTH), (0, hb))
    base -= wbox((-P.SLOT_W / 2, P.SLOT_W / 2), (0, P.SLOT_DEPTH + 1), (-1, hb + 1))
    base -= xcyl(P.BOLT_Y, P.BOLT_Z, -w, w, P.M4_CLEAR / 2)
    for s in (1, -1):
        lo, hi = sorted((s * P.M4_HEAD_SEAT_X, s * (P.EAR_X + 1)))
        base -= xcyl(P.BOLT_Y, P.BOLT_Z, lo, hi, P.M4_HEAD_D / 2)

    # spine up the rack front, then a cradle band with a 45-degree undercut (no supports)
    y_back, y_spine = -P.SPINE_GAP, -(P.SPINE_GAP + P.SPINE_T)
    z0, (c0, c1) = P.wedge_z0(), P.CRADLE_Z
    spine = wbox((-w, w), (y_spine, y_back), (hb, c1))
    prof = Polyline((y_back, z0), (y_back, c1), (ay, c1), (ay, c0), (y_back, z0), close=True)
    wedge = extrude(make_face(Plane.YZ * prof), amount=w, both=True)
    body = base + spine + wedge

    # Dremel: nose thread through the floor, taper cup, body clearance, cradle bore, strap windows
    body -= zcyl(ay, -1, P.FLOOR_T, P.NOSE_HOLE_D / 2)
    body -= Pos(0, ay, P.FLOOR_T) * Cone(P.CONE_R0, P.cone_r(hb + 0.01), hb - P.FLOOR_T + 0.01, align=CC_MIN)
    body -= wbox((-P.FRONT_OPEN_W / 2, P.FRONT_OPEN_W / 2), (ay - P.CUP_R - 1, ay), (P.FRONT_OPEN_Z, hb + 1))
    body -= zcyl(ay, hb, c0, P.BODY_R + P.BODY_CLR)
    body -= zcyl(ay, c0, c1 + 1, P.CRADLE_R)
    body -= wbox((-w - 1, w + 1), (ay - P.CUP_R - 1, ay), (hb, c1 + 1))   # cradle open to the front
    sz = (c0 + c1 - P.STRAP_H) / 2
    body -= wbox((-w - 1, w + 1), (P.STRAP_Y, P.STRAP_Y + P.STRAP_W), (sz, sz + P.STRAP_H))
    return body


def dremel_envelope():
    """Revolved stand-in for the tool: nose thread, taper, body, cradle band. Preview + interference."""
    ay, hb, (c0, _) = P.AXIS_Y, P.H_BASE, P.CRADLE_Z
    e = zcyl(ay, -25, P.FLOOR_T, P.NOSE_HOLE_D / 2 - 0.2)
    e += Pos(0, ay, P.FLOOR_T) * Cone(P.CONE_R0 - 0.2, P.cone_r(hb) - 0.2, hb - P.FLOOR_T, align=CC_MIN)
    e += zcyl(ay, hb, c0, P.BODY_R)
    e += zcyl(ay, c0, P.BODY_TOP, P.CRADLE_R)
    return e


def heatsync_mesh(obj):
    """One object of the HeatSync 3mf as a trimesh, moved from rack coords into this frame."""
    import numpy as np
    import trimesh
    ns = "{http://schemas.microsoft.com/3dmanufacturing/core/2015/02}"
    with zipfile.ZipFile(HEATSYNC_3MF) as z:
        root = ET.fromstring(z.read(f"3D/Objects/{obj}.model"))
    mesh = root.find(f".//{ns}mesh")
    v = np.array([[float(p.get(k)) for k in "xyz"] for p in mesh.find(f"{ns}vertices")])
    t = np.array([[int(p.get(k)) for k in ("v1", "v2", "v3")] for p in mesh.find(f"{ns}triangles")])
    m = trimesh.Trimesh(v, t)
    if obj == "object_77":   # iron collar -> rack: its bolt (15.7, -2.16) onto the rack insert (-9.45, -38.5)
        m.apply_translation((0, -9.45 - 15.7, -38.5 + 2.16))
    m.apply_translation(P.RACK_TO_MOUNT)
    return m


def check_part(name, shape):
    ok = True
    n = len(shape.solids())
    shells = len(shape.solids()[0].shells()) if n else 0
    bb = shape.bounding_box()
    size = (bb.size.X, bb.size.Y, bb.size.Z)
    for cond, msg in [(n == 1, f"{n} solid(s)"), (shells == 1, f"{shells} shell(s)"),
                      (shape.volume > 0, f"volume {shape.volume:.1f} mm3"),
                      (bb.min.Z > -1e-6, "nothing below the bed plane"),
                      (all(s <= b for s, b in zip(size, P.BED, strict=True)), f"size {tuple(round(s, 1) for s in size)} fits bed")]:
        print(("ok   " if cond else "FAIL ") + f"{name}: {msg}")
        ok &= cond
    return ok


def to_mesh(shape):
    import trimesh
    f = os.path.join(tempfile.mkdtemp(), "m.stl")
    export_stl(shape, f, tolerance=0.01)
    return trimesh.load(f)


def check_assembly(part):
    """Mount and the original collar vs the real rack; mount vs the Dremel envelope. Flush contact is fine."""
    if not os.path.exists(HEATSYNC_3MF):
        print(f"FAIL {HEATSYNC_3MF} missing (the HeatSync download from MakerWorld)")
        return False
    rack = heatsync_mesh("object_74")
    pairs = [("mount x rack", to_mesh(part), rack),
             ("original collar x rack (registration)", heatsync_mesh("object_77"), rack),
             ("mount x dremel", to_mesh(part), to_mesh(dremel_envelope()))]
    ok = True
    for name, a, b in pairs:
        v = a.intersection(b, engine="manifold").volume
        good = v < 1.0   # mm3; the meshes are faceted, flush faces leave slivers
        print(("ok   " if good else "FAIL ") + f"{name}: {v:.2f} mm3")
        ok &= good
    return ok


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    part = mount()
    if a.check:
        ok = check_part("mount", part) & check_assembly(part)
        print("\nall green" if ok else "\nFAILED")
        sys.exit(0 if ok else 1)
    os.makedirs(OUT, exist_ok=True)
    export_stl(part, os.path.join(OUT, "dremel_mount.stl"), tolerance=0.01)
    export_step(part, os.path.join(OUT, "dremel_mount.step"))
    export_step(Compound([part, dremel_envelope()]), os.path.join(OUT, "assembly_preview.step"))
    print("wrote", sorted(os.listdir(OUT)))


if __name__ == "__main__":
    main()

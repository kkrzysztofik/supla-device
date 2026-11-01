# Copyright (C) Krzysztof Krzysztofik
"""Every number for the Dremel 3000 mount on the HeatSync insert press. Stdlib only.

The mount replaces the HeatSync soldering-iron collar ("Soldering Iron Mount:1_4") and bolts
to the same rack ("Soldering Iron Mount:1_1") with the same two M4x10 screws into the rack's
M4 inserts.

Frame (mm): X across the press, Y forward = toward the rack (the Dremel hangs at -Y), Z up.
y = 0 is the rack's front face, z = 0 is the bottom of the rack's lower block = mount bottom.
Prints upright as modelled, nose floor on the bed.

Sources, measured from the meshes (not traced from images):
  HEATSYNC   A1_HeatSync.3mf, object_74 = rack, object_77 = iron collar  (Thinkable, MakerWorld)
  DREMEL     Dremel+Drill+Press+Small+Printbed.3mf, object_5 = Dremel_3000_holder_V1 (3DEddy)
rack frame -> this frame: y = y_rack + 15.45, z = z_rack + 50.0 (see RACK_TO_MOUNT)
MEASURE = not in any file; check with calipers on the real Dremel/press, update, re-run checks.
"""

# --- Process: home FDM, PETG, 0.4 nozzle ----------------------------------
WALL_MIN = 1.2          # 3 perimeters at 0.4
BED = (180, 180, 180)   # A1 mini, smallest Bambu bed (HeatSync's own target)

# --- HeatSync rack interface (copied from the iron collar, object_77) ------
RACK_TO_MOUNT = (0.0, 15.45, 50.0)   # add to rack-mesh coords
RACK_W = 41.0           # rack lower block width, x +-20.5
RACK_BLOCK_H = 23.0     # rack lower block z -50..-27 -> 0..23; collar is the same height
SLOT_CLR = 0.1          # per side; original collar is 41.0 on 41.0, the screws pull the ears in
SLOT_W = RACK_W + 2 * SLOT_CLR
SLOT_DEPTH = 14.8       # collar y 9.7..24.5; open at the back for the rack's bottom flange
EAR_X = 25.65           # collar ears' outer face, kept so the ears never reach past the original
H_BASE = RACK_BLOCK_H   # base block height, flush with the rack block top
BOLT_Y = 6.0            # collar (15.7, -2.16) / rack insert (-9.45, -38.5)
BOLT_Z = 11.5
M4_CLEAR = 4.5          # as the collar
M4_HEAD_D = 9.0         # counterbore, as the collar
M4_HEAD_SEAT_X = 21.9   # counterbore floor: 1.4 mm under the head, as the collar -> M4x10 reaches the insert
M4_SCREW_L = 10.0
RACK_INSERT_L = 6.0     # M4x6x6 heat-set insert in the rack

# --- Dremel 3000 (from the drill-press holder, z from its floor bottom) -----
NOSE_HOLE_D = 19.0 + 0.4  # 3/4"-12 nose thread; the holder has 19.0, +0.4 for PETG
FLOOR_T = 3.0           # clamped between the housing shoulder and the nose cap. MEASURE if the cap won't bite
CONE_R0 = 14.0          # housing taper radius at the floor top
CONE_SLOPE = 0.36       # mm radius per mm height (holder: r14.0 @3 -> r20.8 @22)
CRADLE_R = 25.2         # body radius where the rear cradle grips
CRADLE_Z = (140.0, 160.0)  # cradle band above the floor bottom
BODY_R = 26.0           # MEASURE: max body radius between cup and cradle (switch facing front)
BODY_TOP = 190.0        # MEASURE: Dremel 3000 is ~190 long; only used for the preview envelope
FRONT_OPEN_W = 2 * CONE_R0  # front gap in the cup so the nose can tilt in (as the holder)
FRONT_OPEN_Z = 5.0

# --- Mount ------------------------------------------------------------------
W = 30.0                # half-width of base front, spine and cradle
SPINE_T = 7.5
SPINE_GAP = 0.5         # spine back to rack front face (they move together; just no rub)
BODY_CLR = 1.0          # body envelope to spine
AXIS_Y = -(SPINE_GAP + SPINE_T + BODY_CLR + BODY_R)   # Dremel axis
CUP_R = W               # round front of the base
STRAP_W = 3.5           # strap/zip-tie window through each cradle arm
STRAP_H = 14.0
STRAP_Y = AXIS_Y + 2.0  # window starts this far behind the axis line


def cone_r(z):
    """Bore radius of the nose cup at height z (z >= FLOOR_T)."""
    return CONE_R0 + CONE_SLOPE * (z - FLOOR_T)


def wedge_z0():
    """Bottom of the 45-degree undercut under the cradle, at the spine back."""
    return CRADLE_Z[0] - (-SPINE_GAP - AXIS_Y)

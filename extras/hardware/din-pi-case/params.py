# Copyright (C) Krzysztof Krzysztofik
"""Every number for the Display HAT Mini top plate. Stdlib only.

All positions are in the case STLs' world frame (mm, Z up, the DIN face is +Z).
Case = Printables 1257367 (CC-BY), STLs in ref/.
HAT  = Pimoroni PIM589, ref/DisplayHATMiniDimensionalDrawing.png.
MEASURE = not on any drawing; check with calipers, update, re-run checks.
"""

# --- Process: home FDM, PLA/PETG, 0.4 nozzle ------------------------------
WALL_MIN = 1.2          # 3 perimeters at 0.4
HOLE_COMP = 0.2         # FDM holes print undersize; added to diameters below
BED = (220, 220, 250)   # assumption: Prusa MK-class bed

# --- Case: measured by sectioning ref/*.stl (sec.py / inner.py, 2026-10-19) --
PLATE_X = (-32.5, 9.5)   # 42 wide; slot between shoulders is x -33..10 at z 62.5..64.5
PLATE_Y = (-37.0, 31.0)  # 68 = wall-to-wall slot y -37..31 (author's Top_blank is 68 too)
PLATE_Z = (63.0, 64.5)   # under the 1 mm lip at 64.5..65.5 (Top_blank z 63..64.5)
OPENING_X = (-29.5, 6.5) # visible window between shoulders, z 61.5..65.5
OPENING_Y = (-35.0, 29.0)# lip of the side walls
INNER_Y = (-37.0, 31.0)  # wall inner faces for x -31..8, z 54..64.5
RIB_X = ((-33.0, -31.0), (8.0, 10.0))  # guide ribs narrow Y to -35..29 here
LEDGE_X = ((-33.0, -29.5), (6.5, 10.0)) # plate support ledges, z 61.5..62.5

# --- Display HAT Mini, board frame: origin top-left, bx right, by down ------
# (header edge at top). Values from Pimoroni's dimensional drawing.
BOARD = (65.0, 34.6)
BOARD_R = 3.0
HOLES_B = [(3.5, 5.6), (61.5, 5.6), (3.5, 28.6), (61.5, 28.6)]  # M2.5
HOLE_KEEPOUT_R = 2.8
ACTIVE_B = ((13.6, 54.4), (1.9, 32.5))  # 40.8 x 30.6; x/y edges scaled off pixels (18.2 px/mm)
GLASS_BX = (8.4, 56.2)                  # 47.8 dimensioned; left edge scaled off pixels
BUTTONS_B = [(6.6, 11.2), (6.6, 23.0), (58.4, 11.2), (58.4, 23.0)]  # A, B, X, Y
LED_B = (58.4, 17.1)                    # 17.5 from bottom edge
HEADER_B = ((7.0, 58.0), (5.6 - 2.55, 5.6 + 2.55))  # 2x20 socket under the board. MEASURE: assumed in line with holes
PCB_T = 1.6
H_GLASS = 3.8    # MEASURE: PCB top -> glass top. Pimoroni: 17 total with Zero on 10 standoffs (17-10-1.6-1.6)
H_SWITCH = 2.5   # MEASURE: PCB top -> button actuator top (typical 3x6 SMD tact)
H_SOCKET = 8.5   # standard 2x20 female header under PCB
H_IDC = 9.0      # male end of a male-female GPIO ribbon, below the socket. MEASURE yours

Z_PI_PCB = 9.2     # case_bottom standoff tops
H_PI_TALL = 16.0   # Pi 5 Ethernet jack above its PCB (tallest part)
CABLE_BEND = 3.0   # room for the ribbon to turn

# HAT centred on the visible opening.
HAT_C = ((OPENING_X[0] + OPENING_X[1]) / 2, (OPENING_Y[0] + OPENING_Y[1]) / 2)


def b2w(bx, by):
    """Board -> world XY. Board long axis runs along case Y (along the DIN rail),
    seen from outside: bx -> +Y, by -> +X (a pure 90 deg rotation, not a mirror)."""
    return HAT_C[0] + (by - BOARD[1] / 2), HAT_C[1] + (bx - BOARD[0] / 2)


# --- Plate features ---------------------------------------------------------
GLASS_GAP = 0.2       # glass must not be squeezed by the plate. Calibration knob.
WINDOW_MARGIN = 0.5   # per side around the active area
# 45 deg on the outside face, keeps viewing angle. The chamfer widens the window
# on the face that prints against the bed, so it also eats into the wall to the
# X/Y button holes and the LED hole: 4.0 - WINDOW_MARGIN - WINDOW_CHAMFER -
# PLUNGER_HOLE/2 >= WALL_MIN. 0.8 leaves 1.25/1.35 mm there (check.py enforces it).
WINDOW_CHAMFER = 0.8
POST_D = 5.0          # inside the 5.6 keep-out
POST_HOLE = 2.2       # M2.5 self-tapping into plastic (no HOLE_COMP: wants grip)
POST_LEN = H_GLASS + GLASS_GAP   # plate underside -> HAT PCB top
LED_HOLE = 2.5 + HOLE_COMP

# Button plungers: shaft through the plate, flange inside keeps them captive.
PLUNGER_D = 2.5         # glass edge is only ~1.8 from the button centres
PLUNGER_HOLE = PLUNGER_D + 0.2 + HOLE_COMP
FLANGE_D = 5.0          # D-shaped: cut flat at the shaft on the glass side
FLANGE_T = 0.8
PLUNGER_SLACK = 0.3     # flange top -> plate underside at rest
PLUNGER_PROUD = 1.0     # sticks out of the face
SWITCH_TRAVEL = 0.25    # typical tact switch


def z_pcb_top():
    """HAT PCB top in world z: plate underside minus the post length."""
    return PLATE_Z[0] - POST_LEN


def plunger_len():
    """Switch top -> PLUNGER_PROUD above the face."""
    return (PLATE_Z[1] + PLUNGER_PROUD) - (z_pcb_top() + H_SWITCH)

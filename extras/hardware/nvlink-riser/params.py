# Copyright (C) Krzysztof Krzysztofik
"""Every number for the NVLink riser block. Stdlib only.

Two RTX 3090s hang by their PCI bracket flanges from one rail of an open mining frame, PCIe down.
The PNY XLR8 REVEL's NVLink finger sits ~25 mm lower than the Palit GameRock's, so the PNY flange
sits on this block instead of on the rail. That lifts the whole PNY by OFFSET, and a 3-slot bridge
fits. The bridge itself sets the final pitch and lengthwise position. The block slides in the rail
slot, and the flange screw slides in the block's top slot over a captive nut.

Frame (mm): X along the rail, Y across the rail, Z up. Origin = block centre on the rail top face.
Prints as modelled, bottom on the bed, no supports (the nut channel is a 6 mm bridge).
MEASURE = not checked on the real rig yet. Use calipers, update, re-run both checks.
"""
import os

# --- Process: Bambu P2S, ASA/ABS, 0.4 nozzle --------------------------------
BED = (256, 256, 256)
WALL_MIN = 1.6          # 4 lines, assumption
HOLE_COMP = 0.2         # vertical holes print undersize, assumption

# --- The fix ----------------------------------------------------------------
# PNY finger height below the GameRock finger, both cards bolted to the rail. Measure from the rail
# top to the top edge of each NVLink finger and take the difference. Fine-tune with washers under the flange.
OFFSET = float(os.environ.get("NVLINK_OFFSET", "25.0"))   # MEASURE: user estimate "around 25"
NVLINK_PITCH = 3 * 20.32   # 3-slot bridge, PCB to PCB (PCIe slot pitch 0.8")

# --- Rig --------------------------------------------------------------------
RAIL_D = 20.0           # MEASURE: rail width across Y (block depth matches it)
FLANGE_W = 38.7         # MEASURE: PNY bracket flange width along X; 2-slot flange = 38.74, 3-slot = 59.06
FLANGE_SCREW = 3.0      # MEASURE: M3 assumed (6-32 = 3.5 also fits the 3.6 slot)

# --- Block ------------------------------------------------------------------
BLOCK_W = 36.0          # along X; stays under the PNY flange so it never touches the GameRock flange
BLOCK_D = RAIL_D
H = OFFSET              # flange sits on the top, block bottom on the rail top

# rail: two M3 screws up through the rail slot into heat-set inserts
INSERT_D = 4.0          # M3x5.7 heat-set insert pilot (OD 4.6 knurl)
INSERT_L = 6.0          # pilot depth, 0.3 deeper than the insert
RAIL_SCREW_X = 12.0     # +-, both in the rail slot line y = 0

# flange: M3 screw down through an X slot into a nut that slides in a channel open at both X ends
SLOT_W = FLANGE_SCREW + 0.4 + HOLE_COMP
SLOT_END = 3.0          # top slot stops this far from each X end so the top stays one piece
NUT_AF = 5.5 + 0.4      # M3 nut across flats + clearance
NUT_T = 2.4 + 0.4
NUT_TOP = 5.0           # plastic between the top face and the nut channel ceiling
FLANGE_T = 1.0          # bracket flange sheet thickness
FLANGE_SCREW_L = 10.0   # M3x10

# Copyright (C) Krzysztof Krzysztofik
"""Every number for the XIAO ESP32-S3 thermostat wall box. Stdlib only.

World frame (mm): X right, Y up as hung on the wall, Z out of the wall.
z = 0 is the wall. The base prints back-down as modelled; the lid prints face-down.
Cavity = x 0..IN_W, y 0..IN_H, z BACK_T..Z_RIM.

Board data come from Seeed's own CAD files (downloaded 2026-10-20, parsed, not traced from images):
  shield  Grove_Shield_for_Seeeduino_XIAO_v1.2_KiCad_SCH_PCB.zip  (KiCad, origin top-left, Y down)
  relay   Grove-Relay_Eagle_Files.zip  "Grove - Relay v1.2.brd"   (Eagle, origin board centre, Y up)
  button  Grove-Red_LED_Button.zip     (the yellow one shares the board)
  oled    OLED 128x64.zip + LY190-128064.pdf (panel 26.7x19.26, active 21.74x11.175)
MEASURE = not in any file; check with calipers, update, re-run both checks.
"""

# --- Process: home FDM, PLA/PETG, 0.4 nozzle ------------------------------
WALL_MIN = 1.2          # 3 perimeters at 0.4
HOLE_COMP = 0.2         # FDM holes print undersize; added to clearance holes
CLR = 0.3               # per side, board in a pocket
BED = (220, 220, 250)   # assumption: Prusa MK-class bed
M2_PILOT = 1.6          # M2 machine screw threading into plastic, no HOLE_COMP. Calibration knob.
M3_CLEAR = 3.4 + HOLE_COMP
M3_CSK_D = 6.5          # countersunk head
WOOD_SCREW = 4.0        # wall screws, countersunk from inside
WOOD_CLEAR = WOOD_SCREW + 0.5 + HOLE_COMP
WOOD_HEAD_D = 8.0

# --- Box ------------------------------------------------------------------
WALL = 2.4
BACK_T = 3.0
LID_T = 2.4
CORNER_R = 3.0          # outside vertical edges
BOSS_D = 8.0            # lid insert columns in the four cavity corners
BOSS_IN = 2.5           # column centre inset from both inner walls

# --- Grove Shield for XIAO (KiCad, board frame bx 0..58 right, by 0..25 down) --
SH = (58.0, 25.0)
SH_PCB_T = 1.6                      # MEASURE (Seeed default)
XIAO_B = ((36.77, 57.77), (3.65, 21.45))  # U1 21 x 17.8 centred (47.27, 12.55)
XIAO_USB_BY = 12.55                 # USB-C on the bx = 58 end
GROVE_SH_B = [(bx, by) for bx in (6.23, 14.19, 24.03, 32.03) for by in (5.44, 19.41)]  # J1..J8, vertical
GROVE_SOCKET = (4.9, 10.0)          # bx x by footprint of a vertical HW4-2.0. MEASURE
HDR_H = 8.5             # female header the XIAO plugs into. MEASURE
XIAO_PCB_T = 1.2        # MEASURE
USB_H = 3.2             # USB-C receptacle on top of the XIAO
XIAO_USB_PROTRUDE = 1.0 # receptacle past the board edge. MEASURE
PIN_STUB = 2.5          # THT pins under the shield; the rails must leave this free

# --- Grove 1x2 modules (Eagle frame, origin centre, Y up) -------------------
GROVE_PCB_T = 1.6       # MEASURE
RELAY_X = (-20.0, 20.2)
RELAY_Y = (-10.2, 10.3)
RELAY_HOLES = [(-10.0, -10.0), (-10.0, 10.0), (20.0, 0.0)]  # d2.2, two are edge notches
RELAY_BODY = ((-3.4, 18.2), (-8.6, 8.7))   # HLS8L outline from the footprint
RELAY_BODY_H = 15.6     # MEASURE
RELAY_GROVE = ((-18.7, -12.3), (-5.0, 5.0))  # vertical DIP socket

BTN_X = (-19.57, 20.64)
BTN_Y = (-10.30, 10.20)
BTN_HOLES = [(-10.64, -10.0), (-10.63, 10.0), (19.36, 0.0)]
BTN_SW = 12.0           # 12x12 switch on the back (B) side, centred (0, 0)
BTN_SW_H = 10.0         # PCB face -> cap top ("12X12X10" footprint). MEASURE
BTN_CAP_D = 11.0        # MEASURE
BTN_CAP_H = 4.0         # part of BTN_SW_H that is the round cap. MEASURE
BTN_TRAVEL = 0.5        # MEASURE
BTN_GROVE = ((-20.0, -10.8), (-6.1, 6.1))   # right-angle socket on the front (T) side

OLED_X = (-20.0, 22.1)
OLED_Y = (-12.1, 12.1)
OLED_HOLES = [(-12.1, 10.0), (-12.1, -10.0), (17.9, 0.0)]   # full holes, but the first two
# sit ~1.1 from the glass corners: no post fits. Held like the others: lips + tab.
OLED_PAD_A = (16.0, 22.1)   # glass-free end strip, under the tab
OLED_PAD_B = (-20.0, -11.6) # glass-free end strip under the lips
OLED_LIP_Y = 7.0            # lips at |y| >= this, clear of the Grove socket (|y| <= 6.1)
LIP = 1.0                   # overhang onto the T side
LIP_T = 1.2
OLED_PANEL = ((-11.0, 15.7), (-9.14, 9.735))  # glass on the B side, FPC bends at the low-Y edge
OLED_PANEL_H = 1.5      # PCB face -> glass face incl. tape. MEASURE
OLED_AA = (21.74, 11.175)
OLED_AA_TOP = 2.5       # glass top edge (high Y, away from FPC) -> active area. MEASURE
OLED_GROVE = ((-21.5, -10.8), (-6.1, 6.1))  # right-angle socket on the T side

GROVE_CONN_H = 6.0      # right-angle socket + plug height above the PCB. MEASURE
GROVE_PLUG_UP = 15.0    # vertical socket + plug + cable bend above the PCB. MEASURE

# --- Layout in the cavity ---------------------------------------------------
# Shield: vertical, XIAO/USB end down. bx -> -y, by -> +x. The parts on it are near
# symmetric about by = 12.5, so mirror vs rotation is not checked (unlike din-pi-case).
USB_PLUG_METAL = 6.5    # USB-C plug shell length. Overmold must start inside the cavity
USB_OVERMOLD = (12.5, 7.0)  # MEASURE your cable
SH_X0 = 7.0             # clear of the corner columns
SH_Y0 = USB_PLUG_METAL - 0.5 + XIAO_USB_PROTRUDE   # receptacle face 0.5 below plug overmold
LEDGE_H = 3.0           # rails under the shield; > PIN_STUB
GAP_SCREW = 12.0        # strip between shield and relay for the wall screws
RL_X0 = SH_X0 + SH[1] + GAP_SCREW
RL_TOP = SH_Y0 + SH[0]  # relay top aligned with shield top
IN_W = RL_X0 + (RELAY_Y[1] - RELAY_Y[0]) + 7.0
IN_H = RL_TOP + 2.0
WALL_SCREWS = [(SH_X0 + SH[1] + GAP_SCREW / 2, y) for y in (16.0, 50.0)]
HOOK_Y = 24.0           # shield snap hooks: along the XIAO, the Grove sockets reach the board edge

# Front modules on the lid, seen from outside: B side faces out, so x mirrors.
WIN_CX = IN_W / 2
OLED_C = (WIN_CX + (OLED_PANEL[0][0] + OLED_PANEL[0][1]) / 2, IN_H - 3.0 - OLED_Y[1])
BTN_C = (WIN_CX, 18.0)

# Cables through the bottom wall (axis along Y). Teardrops: printed axis-horizontal.
PROBE_CABLE_D = 4.0     # DS18B20 SEN0002 cable. MEASURE
RELAY_CABLE_D = 5.5     # your 2-core thermostat cable. MEASURE
CABLE_HOLES = [  # (name, x, d)
    ("probe", RL_X0 + 4.0, PROBE_CABLE_D + 0.6 + HOLE_COMP),
    ("relay", RL_X0 + 16.0, RELAY_CABLE_D + 0.6 + HOLE_COMP),
]
CABLE_Z = BACK_T + 6.0
TIE = (8.0, 6.0, 5.0)   # zip-tie anchor block above the relay cable hole, x y z
TIE_SLOT = (4.0, 2.0)   # tunnel across x, w x h (FDM bridge)
TIE_Y = 8.0             # anchor centre above the inner bottom wall

# Probe strain relief: saddle on the floor + screwed clamp bar, cable centre on the hole axis.
CLAMP_Y = (2.0, 10.0)   # grip length along the cable
CLAMP_DX = 4.0          # M2 screws either side of the cable centre
CLAMP_W = 2 * (CLAMP_DX + 2.2)
CLAMP_BAR_T = 2.5
CLAMP_GROOVE = 1.5      # groove depth in the saddle and in the bar
CLAMP_SQUEEZE = 0.6     # cable diameter lost when tight. Calibration knob (with PROBE_CABLE_D)
CLAMP_GAP = PROBE_CABLE_D - 2 * CLAMP_GROOVE - CLAMP_SQUEEZE   # bar to saddle when tight
CLAMP_SCREW = 6.0       # M2x6, same as everywhere

# Antenna (DFRobot FIT0505, 170 mm, 15 cm U.FL pigtail) in a socket on the top wall.
ANT_BASE_D = 12.0       # MEASURE (fits a 13 mm deep package)
ANT_SOCKET_L = 10.0
ANT_CABLE_HOLE = 4.5    # passes the U.FL plug
ANT_X = WIN_CX
ANT_SOCKET_WALL = 2.0

# --- Lid features -----------------------------------------------------------
GLASS_GAP = 0.3         # glass must not be squeezed. Calibration knob.
WINDOW_MARGIN = 1.0     # per side; absorbs OLED_AA_TOP error
WINDOW_CHAMFER = 1.0
BTN_PROUD = 1.5         # cap above the lid face
BTN_HOLE = BTN_CAP_D + 0.6 + HOLE_COMP
POST_D = 4.5            # supports under the relay and button holes

OLED_POST = OLED_PANEL_H + GLASS_GAP
BTN_POST = BTN_SW_H - LID_T - BTN_PROUD

# --- Depth: worst case, any back stack under any front stack ---------------
# ponytail: assumes everything overlaps in plan; per-overlap stacking saves ~5 mm if it matters.
BACK_STACK = max(
    LEDGE_H + SH_PCB_T + max(HDR_H + XIAO_PCB_T + USB_H, GROVE_PLUG_UP),
    LEDGE_H + GROVE_PCB_T + max(RELAY_BODY_H, GROVE_PLUG_UP),
)
FRONT_STACK = max(OLED_POST, BTN_POST) + GROVE_PCB_T + GROVE_CONN_H
STACK_GAP = 2.0
IN_D = round(BACK_STACK + FRONT_STACK + STACK_GAP + 0.49)
Z_RIM = BACK_T + IN_D


def shield_xy(bx, by):
    return SH_X0 + by, SH_Y0 + (SH[0] - bx)


def relay_xy(x, y):
    """Relay vertical, Grove end up: board x -> world -y, board y -> world +x."""
    return RL_X0 - RELAY_Y[0] + y, RL_TOP + RELAY_X[0] - x


def oled_xy(x, y):
    return OLED_C[0] - x, OLED_C[1] + y


def btn_xy(x, y):
    return BTN_C[0] - x, BTN_C[1] + y


def rect(f, xs, ys):
    """Board (lo, hi) ranges through a mapping -> world ((x0, x1), (y0, y1))."""
    a, b = f(xs[0], ys[0]), f(xs[1], ys[1])
    return tuple(sorted((a[0], b[0]))), tuple(sorted((a[1], b[1])))


def oled_window():
    """Active area + margin, world ((x0, x1), (y0, y1)). AA centred across the glass."""
    top = OLED_PANEL[1][1] - OLED_AA_TOP
    cx = (OLED_PANEL[0][0] + OLED_PANEL[0][1]) / 2
    m = WINDOW_MARGIN
    return rect(oled_xy, (cx - OLED_AA[0] / 2 - m, cx + OLED_AA[0] / 2 + m), (top - OLED_AA[1] - m, top + m))


def usb_center():
    x, _ = shield_xy(0, XIAO_USB_BY)
    z = BACK_T + LEDGE_H + SH_PCB_T + HDR_H + XIAO_PCB_T + USB_H / 2
    return x, z


BOSSES = [(BOSS_IN, BOSS_IN), (IN_W - BOSS_IN, BOSS_IN), (BOSS_IN, IN_H - BOSS_IN), (IN_W - BOSS_IN, IN_H - BOSS_IN)]
OUTER = ((-WALL, IN_W + WALL), (-WALL, IN_H + WALL), (0.0, Z_RIM + LID_T))


# --- Fasteners ---------------------------------------------------------------
# Lid: heat-set inserts (Ruthex-type M3 x 5.7, OD ~4.6) + M3x8 countersunk (DIN 7991).
INSERT_D = 4.0          # hole for the insert (maker's figure for M3 x 5.7)
INSERT_L = 5.7
INSERT_HOLE = INSERT_L + 1.3   # melt room below the insert
LID_SCREW = 8.0
# Lid modules: slide the connector end under two lips, lock the far end with a printed tab
# and one M2x6 machine screw threaded straight into a plastic post. All M2 are M2x6.
TAB_SCREW = 6.0
TAB_T = 2.0
TAB_W = 6.0
TAB_OVER = 1.5          # tab overlap onto the board's T face
TAB_POST_D = 5.0
TAB_HOLE = 2.2 + HOLE_COMP
RAIL_T = 1.2            # side guide rails along the lid modules' long edges
RAIL_PROUD = 0.5        # rail top past the PCB T face
SCREW_SPARE = 0.5

RELAY_SCREW = 6.0       # M2x6 machine screws through the relay's holes/notches into the standoffs
# Lid modules: name -> mapping, board x/y ranges, lip start |y|. The connector end (-x) goes under the lips.
MODULES = {
    "oled": (oled_xy, OLED_X, OLED_Y, OLED_LIP_Y),
    "button": (btn_xy, BTN_X, BTN_Y, 7.3),         # Grove socket |y| <= 6.1, test pads |y| <= 6.95
}
TAB_CLEAR_OF = {  # nearest T-side part to the tab overlap, board x of its +x edge
    "oled": 16.32 + 0.5,                           # P1 marker pad
    "button": 17.78 + 0.6,                         # TP5..TP8
}


def module_z(name):
    """(z_mount, z_b, d): mounting surface, PCB face on the supports, direction to the T side."""
    return Z_RIM, Z_RIM - (OLED_POST if name == "oled" else BTN_POST), -1


def rails(name):
    """World rects of the two side rails of a lid module (full length, CLR off the long edges)."""
    f, xs, ys, _ = MODULES[name]
    x = (xs[0] - CLR - LIP_T, xs[1] + CLR)
    return [rect(f, x, (ys[1] + CLR, ys[1] + CLR + RAIL_T)), rect(f, x, (ys[0] - CLR - RAIL_T, ys[0] - CLR))]


def tab_post(name):
    f, xs, _, _ = MODULES[name]
    return f(xs[1] + CLR + TAB_POST_D / 2, 0.0)


def pilots():
    """name -> (z_lo, z_hi, engagement, skin). Skin = plastic left beyond the hole on the outside."""
    out = {}
    tip = Z_RIM + LID_T - LID_SCREW                          # head flush in the lid
    out["lid"] = (Z_RIM - INSERT_HOLE, Z_RIM + 1, min(Z_RIM - tip, INSERT_L), Z_RIM - INSERT_HOLE - BACK_T)
    zs, _ = clamp_z()
    top = zs + CLAMP_GAP + CLAMP_BAR_T
    tip = top - CLAMP_SCREW                                  # head on the bar, pointing -z
    out["clamp"] = (tip - SCREW_SPARE, zs + 0.1, zs - tip, tip - SCREW_SPARE)
    z_pcb = BACK_T + LEDGE_H
    tip = z_pcb + GROVE_PCB_T - RELAY_SCREW                  # head on the relay, pointing -z
    out["relay"] = (tip - SCREW_SPARE, z_pcb + 0.1, RELAY_SCREW - GROVE_PCB_T, tip - SCREW_SPARE)
    for n in MODULES:
        zm, zb, d = module_z(n)
        zt = zb + d * GROVE_PCB_T                            # T face = post top, tab sits on it
        tip = zt + d * TAB_T - d * TAB_SCREW
        lo, hi = sorted((zt + d * 0.1, tip - d * SCREW_SPARE))
        skin = lo if d > 0 else Z_RIM + LID_T - hi           # back face z=0, lid face z1
        out["tab_" + n] = (lo, hi, TAB_SCREW - TAB_T, skin)
    return out


def clamp_x():
    return next(x for n, x, _ in CABLE_HOLES if n == "probe")


def clamp_z():
    """(saddle top, cable centre). The cable centre sits on the hole axis."""
    return CABLE_Z - (PROBE_CABLE_D / 2 - CLAMP_GROOVE), CABLE_Z

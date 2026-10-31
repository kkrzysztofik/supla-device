# Copyright (C) Krzysztof Krzysztofik
"""Every number for the XIAO ESP32-S3 thermostat wall box. Stdlib only.

World frame (mm): X right, Y up as hung on the wall, Z out of the wall.
z = 0 is the wall. The base prints back-down as modelled; the lid prints face-down.
Cavity = x 0..IN_W, y 0..IN_H, z BACK_T..Z_RIM.

Board data come from Seeed's own CAD files (downloaded 2026-10-20, parsed, not traced from images):
  shield  Grove_Shield_for_Seeeduino_XIAO_v1.2_KiCad_SCH_PCB.zip  (KiCad, origin top-left, Y down)
  relay   Grove-Relay_Eagle_Files.zip  "Grove - Relay v1.2.brd"   (Eagle, origin board centre, Y up)
  oled, button: measured with calipers (the Seeed files describe older boards)
MEASURE = not in any file; check with calipers, update, re-run both checks.
"""
import os

# --- Process: home FDM, PLA/PETG, 0.4 nozzle ------------------------------
WALL_MIN = 1.2          # 3 perimeters at 0.4
HOLE_COMP = 0.2         # FDM holes print undersize; added to clearance holes
CLR = 0.3               # per side, board in a pocket
BED = (220, 220, 250)   # assumption: Prusa MK-class bed
TAB_POST_D = 5.0        # posts for the M2x6 locking-tab screws
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

# Lid modules: Grove OLED 0.96" and Grove LED Button share one 40.1 x 20 Grove 1x2 board.
# Calipers, 2026-10-30. Board frame seen from outside the lid (glass / cap towards you):
# bx 0..40.1 right, by 0..20 up, Grove socket on the back at the left edge. Front faces out: no mirror.
MOD = (40.1, 20.0)
MOD_PCB_T = 1.5
NOTCH_D = 4.5           # open half-holes centred on the board edge (Grove standard)
MOD_NOTCHES = [(10.0, 0.0), (10.0, MOD[1]), (MOD[0], MOD[1] / 2)]
MOD_GROVE = ((0.0, 9.0), (4.0, 16.0))   # socket on the back, 9 x 12 centred on the left edge
MOD_PLUG_OUT = 8.0      # right-angle plug + cable bend past the left edge. MEASURE

OLED_GLASS = ((11.6, 38.6), (0.0, 20.0))  # front; the left notches end where the glass starts
OLED_GLASS_H = 1.8      # PCB face -> glass face incl. tape
OLED_FPC = ((18.0, 30.1), (MOD[1], MOD[1] + 3.0))  # wraps the top edge, 3 past it, ends 10 from the right. x0 MEASURE

BTN_SW = 12.0           # switch body, centred on the board (its side notches don't matter here)
BTN_BODY_H = 4.3        # switch body height above the PCB, 12x12 tact. MEASURE
BTN_CAP_D = 7.0
BTN_TOP_H = 7.0         # PCB face -> cap top
BTN_TRAVEL = 1.0

GROVE_CONN_H = 6.0      # right-angle socket + plug height above the PCB. MEASURE
GROVE_PLUG_UP = 15.0    # vertical socket + plug + cable bend above the PCB. MEASURE

# --- Layout in the cavity ---------------------------------------------------
# Shield: vertical, XIAO/USB end down. bx -> -y, by -> +x. The parts on it are near
# symmetric about by = 12.5, so mirror vs rotation is not checked (unlike din-pi-case).
# USB-C port: receptacle right at the wall, shell-shaped slot through a thin bezel, the plug
# body nests in a shallow recess outside so the plug still seats.
USB_SHELL = (8.4, 2.6)  # plug shell, USB-C spec max 8.34 x 2.56
USB_RECEPT = (9.0, 3.2) # the XIAO's receptacle. MEASURE
USB_OVERMOLD = (12.5, 7.0)  # MEASURE your cable
PORT_WALL = 0.8         # bezel around the slot (2 perimeters)
USB_MATE_GAP_MAX = 1.2  # plug body may stop this far short of the receptacle (assumption)
SH_X0 = 7.0             # clear of the corner columns
SH_Y0 = CLR             # board end on the bottom wall; the receptacle sits in a pocket in it
LEDGE_H = 3.0           # rails under the shield; > PIN_STUB
GAP_SCREW = 12.0        # strip between shield and relay for the wall screws
RL_X0 = SH_X0 + SH[1] + GAP_SCREW
RL_TOP = SH_Y0 + SH[0]  # relay top aligned with shield top
IN_W = RL_X0 + (RELAY_Y[1] - RELAY_Y[0]) + 7.0
IN_H = max(RL_TOP + 2.0, SH_Y0 + SH[0] + 2 * CLR + TAB_POST_D + 0.5)  # room for the shield's end tab post
WALL_SCREWS = [(SH_X0 + SH[1] + GAP_SCREW / 2, y) for y in (28.0, 54.0)]

# Lid modules: world position of each board's bx = by = 0 corner. Glass and cap on the centre line.
WIN_CX = IN_W / 2
OLED_O = (WIN_CX - sum(OLED_GLASS[0]) / 2, IN_H - 4.0 - MOD[1])   # top: FPC (3) + CLR, posts in the cavity
BTN_O = (WIN_CX - MOD[0] / 2, 8.0)
BTN_C = (WIN_CX, BTN_O[1] + MOD[1] / 2)

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
WINDOW_MARGIN = 1.0     # outer opening inset from the glass edge, per side; the unlit glass is a black border
WINDOW_CHAMFER = 1.0
BTN_HOLE = BTN_CAP_D + 0.6 + HOLE_COMP
POST_D = 4.5            # standoffs under the relay holes
# Lid module mounts: a post at each edge notch. Under the board it is a shoulder (clipped off the
# glass); outside the board it rises past the PCB edge and locates it. M2x6 + printed washer from inside.
MOD_CLR = float(os.environ.get("MOD_CLR", "0.3"))   # board edge to locating post, per side. Calibration knob.
NOTCH_POST_D = 7.0
WASHER_D = 7.0          # the Ø4.5 notch swallows an M2 head (Ø3.8)
WASHER_T = 1.5
MOD_SCREW = 6.0
BTN_POCKET = LID_T - 1.0    # switch body sinks into the lid's inner face, 1.0 skin left

OLED_POST = OLED_GLASS_H + GLASS_GAP
BTN_POST = BTN_BODY_H + 0.3 - BTN_POCKET
BTN_PROUD = BTN_TOP_H - BTN_POST - LID_T    # cap above the lid face

# --- Depth: worst case, any back stack under any front stack ---------------
# ponytail: assumes everything overlaps in plan; per-overlap stacking saves ~5 mm if it matters.
BACK_STACK = max(
    LEDGE_H + SH_PCB_T + max(HDR_H + XIAO_PCB_T + USB_H, GROVE_PLUG_UP),
    LEDGE_H + GROVE_PCB_T + max(RELAY_BODY_H, GROVE_PLUG_UP),
)
FRONT_STACK = max(OLED_POST, BTN_POST) + MOD_PCB_T + max(GROVE_CONN_H, WASHER_T + 1.6)   # 1.6: M2 head
STACK_GAP = 2.0
IN_D = 36               # kept from the printed base (the new lid stack is 3 mm thinner); check.py asserts it fits
Z_RIM = BACK_T + IN_D


def shield_xy(bx, by):
    return SH_X0 + by, SH_Y0 + (SH[0] - bx)


def relay_xy(x, y):
    """Relay vertical, Grove end up: board x -> world -y, board y -> world +x."""
    return RL_X0 - RELAY_Y[0] + y, RL_TOP + RELAY_X[0] - x


def oled_xy(x, y):
    return OLED_O[0] + x, OLED_O[1] + y


def btn_xy(x, y):
    return BTN_O[0] + x, BTN_O[1] + y


def rect(f, xs, ys):
    """Board (lo, hi) ranges through a mapping -> world ((x0, x1), (y0, y1))."""
    a, b = f(xs[0], ys[0]), f(xs[1], ys[1])
    return tuple(sorted((a[0], b[0]))), tuple(sorted((a[1], b[1])))


def oled_window():
    """Through-opening, world ((x0, x1), (y0, y1)): the glass minus margin and chamfer per side."""
    m = WINDOW_MARGIN + WINDOW_CHAMFER
    (x0, x1), (y0, y1) = OLED_GLASS
    return rect(oled_xy, (x0 + m, x1 - m), (y0 + m, y1 - m))


def usb_port():
    """Y planes of the port, outside is -y: (receptacle face, pocket floor, bezel outer face)."""
    face = SH_Y0 - XIAO_USB_PROTRUDE
    pocket = face - 0.2
    return face, pocket, pocket - PORT_WALL


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
# Shield tabs: a printed tab and one M2x6 machine screw threaded straight into a plastic post.
# All M2 are M2x6.
TAB_SCREW = 6.0
TAB_T = 2.0
TAB_W = 6.0
TAB_OVER = 1.5          # tab overlap onto the board's T face
TAB_HOLE = 2.2 + HOLE_COMP
# Shield (no mounting holes in Seeed's KiCad): clamped by 3 tabs on its edges, in board frame
# (edge point, outward direction). Edge strips there are clear on the component side.
SHIELD_TABS = [
    ((0.0, 12.5), (-1, 0)),    # far end, between J4 and J8 (sockets start at bx 3.8)
    ((40.0, 0.0), (0, -1)),    # long edge beside the XIAO (J11 pads from bx 43.4)
    ((40.0, 25.0), (0, 1)),    # long edge beside the XIAO (J10 pads from bx 43.4)
]
SHIELD_KEEPOUT = [  # component-side parts near the tabs, board frame ((bx0, bx1), (by0, by1))
    *[((bx - GROVE_SOCKET[0] / 2, bx + GROVE_SOCKET[0] / 2), (by - GROVE_SOCKET[1] / 2, by + GROVE_SOCKET[1] / 2)) for bx, by in GROVE_SH_B],
    ((43.4, 51.1), (0.1, 2.7)), ((43.4, 51.1), (22.4, 25.0)),   # J11, J10 header pads
    ((35.1, 36.3), (19.3, 21.9)),                               # R18, R20, R21
    ((56.6, 57.6), (22.3, 23.3)), ((56.0, 57.9), (6.3, 9.0)),   # D16 charge LED, J9 battery pads
]
SCREW_SPARE = 0.5

RELAY_SCREW = 6.0       # M2x6 machine screws through the relay's holes/notches into the standoffs
MODULES = {"oled": (oled_xy, OLED_POST), "button": (btn_xy, BTN_POST)}   # name -> mapping, standoff
# Front-side parts the notch shoulders must stay off, board frame.
BTN_SW_B = tuple(((m - BTN_SW) / 2, (m + BTN_SW) / 2) for m in MOD)
MOD_FRONT_KEEPOUT = {"oled": OLED_GLASS, "button": BTN_SW_B}


def module_z(name):
    """(front face, back face) of a lid module's PCB. The front rests on the shoulders."""
    zf = Z_RIM - MODULES[name][1]
    return zf, zf - MOD_PCB_T


def shield_tab_sites():
    """[(post xy, board-edge xy)] in world coords."""
    out = []
    for (bx, by), (ox, oy) in SHIELD_TABS:
        k = CLR + TAB_POST_D / 2
        out.append((shield_xy(bx + ox * k, by + oy * k), shield_xy(bx, by)))
    return out


def shield_tab_overlap(i):
    """Board-frame rect the tab covers."""
    (bx, by), (ox, oy) = SHIELD_TABS[i]
    w = TAB_W / 2
    if ox:
        return (bx, bx + TAB_OVER) if ox < 0 else (bx - TAB_OVER, bx), (by - w, by + w)
    return (bx - w, bx + w), ((by, by + TAB_OVER) if oy < 0 else (by - TAB_OVER, by))


def pilots():
    """name -> (z_lo, z_hi, engagement, skin). Skin = plastic left beyond the hole on the outside."""
    out = {}
    tip = Z_RIM + LID_T - LID_SCREW                          # head flush in the lid
    out["lid"] = (Z_RIM - INSERT_HOLE, Z_RIM + 1, min(Z_RIM - tip, INSERT_L), Z_RIM - INSERT_HOLE - BACK_T)
    zt = BACK_T + LEDGE_H + SH_PCB_T                         # shield T face = post top
    tip = zt + TAB_T - TAB_SCREW
    out["tab_shield"] = (tip - SCREW_SPARE, zt + 0.1, TAB_SCREW - TAB_T, tip - SCREW_SPARE)
    zs, _ = clamp_z()
    top = zs + CLAMP_GAP + CLAMP_BAR_T
    tip = top - CLAMP_SCREW                                  # head on the bar, pointing -z
    out["clamp"] = (tip - SCREW_SPARE, zs + 0.1, zs - tip, tip - SCREW_SPARE)
    z_pcb = BACK_T + LEDGE_H
    tip = z_pcb + GROVE_PCB_T - RELAY_SCREW                  # head on the relay, pointing -z
    out["relay"] = (tip - SCREW_SPARE, z_pcb + 0.1, RELAY_SCREW - GROVE_PCB_T, tip - SCREW_SPARE)
    for n in MODULES:
        zf, zk = module_z(n)
        tip = zk - WASHER_T + MOD_SCREW                      # head on the washer, pointing +z
        out["mod_" + n] = (zf, tip + SCREW_SPARE, tip - zf, Z_RIM + LID_T - tip - SCREW_SPARE)
    return out


def clamp_x():
    return next(x for n, x, _ in CABLE_HOLES if n == "probe")


def clamp_z():
    """(saddle top, cable centre). The cable centre sits on the hole axis."""
    return CABLE_Z - (PROBE_CABLE_D / 2 - CLAMP_GROOVE), CABLE_Z

from pathlib import Path

import numpy as np
import trimesh
from manifold3d import Manifold

PCB_W = 85.49
PCB_H = 55.63
CLEARANCE = 0.5
WALL = 2.0
FRONT = 2.0
DEPTH = 30.0
RIB = 1.5
RIB_DEPTH = 10.0
RIB_TRIM = 0.5

CABLE_SLOT_W = 5.5
CABLE_SLOT_Y = -5.5
CABLE_SLOT_Z = 10.0

BACK_T = 2.0
SKIRT_H = 4.0
SKIRT_T = 1.2
BACK_FIT = 0.2
SNAP_R = 0.6
SNAP_FROM_TOP = 2.5

VENT_X = -6.5
VENT_Y = -4.5
VENT_D = 3.0
VENT_PITCH = 5.0
VENT_COLS = 5
VENT_ROWS = 4

WINDOW_W = 75.0
WINDOW_H = 50.5
WINDOW_OFFSET_X = -3.0

STAND_TILT = 15.0
STAND_LEN = 100.0
STAND_BASE = 3.0
STAND_LIP = 7.0
STAND_BACK = 20.0
SLOT_CLEARANCE = 0.6

OUT = Path(__file__).parent


def box(x, y, z, cx=0.0, cy=0.0, z0=0.0):
    return Manifold.cube((x, y, z)).translate((cx - x / 2, cy - y / 2, z0))


def frame():
    pocket_w = PCB_W + 2 * CLEARANCE
    pocket_h = PCB_H + 2 * CLEARANCE
    pocket_x = -WINDOW_OFFSET_X
    border = max(
        pocket_w / 2 + abs(pocket_x) + WALL - WINDOW_W / 2,
        pocket_h / 2 + WALL - WINDOW_H / 2,
    )
    outer_w = WINDOW_W + 2 * border
    outer_h = WINDOW_H + 2 * border
    body = box(outer_w, outer_h, DEPTH)
    cavity = box(outer_w - 2 * WALL, outer_h - 2 * WALL, DEPTH, z0=FRONT)
    window = box(WINDOW_W, WINDOW_H, FRONT + 2, z0=-1)
    shell = body - cavity - window

    in_w = outer_w / 2 - WALL
    in_h = outer_h / 2 - WALL
    px0 = pocket_x - pocket_w / 2 - RIB_TRIM
    px1 = pocket_x + pocket_w / 2 + RIB_TRIM
    py = pocket_h / 2 + RIB_TRIM
    rib_h = RIB_DEPTH - FRONT
    ribs = []
    for s in (-0.25, 0.25):
        if px0 > -in_w:
            ribs.append(box(px0 + in_w, RIB, rib_h, cx=(px0 - in_w) / 2, cy=s * pocket_h, z0=FRONT))
        if in_w > px1:
            ribs.append(box(in_w - px1, RIB, rib_h, cx=(px1 + in_w) / 2, cy=s * pocket_h, z0=FRONT))
        for sign in (-1, 1):
            if in_h > py:
                ribs.append(box(RIB, in_h - py, rib_h, cx=pocket_x + s * pocket_w, cy=sign * (py + in_h) / 2, z0=FRONT))
    for r in ribs:
        shell = shell + r

    groove_z = DEPTH - SNAP_FROM_TOP
    groove = box(outer_w - 2 * WALL + 2 * (SNAP_R + 0.1), outer_h - 2 * WALL + 2 * (SNAP_R + 0.1), 2 * SNAP_R + 0.4, z0=groove_z - SNAP_R - 0.2)
    shell = shell - (groove - box(outer_w - 2 * WALL, outer_h - 2 * WALL, DEPTH + 2, z0=-1))

    slot_x = outer_w / 2 - WALL / 2
    r = CABLE_SLOT_W / 2
    slot = box(WALL + 2, CABLE_SLOT_W, DEPTH, cx=slot_x, cy=CABLE_SLOT_Y, z0=CABLE_SLOT_Z + r)
    slot = slot + Manifold.cylinder(WALL + 2, r, r, 48).rotate((0, 90, 0)).translate((slot_x - WALL / 2 - 1, CABLE_SLOT_Y, CABLE_SLOT_Z + r))
    shell = shell - slot

    print(f"border {border:.2f} mm")
    return shell, outer_w, outer_h


def back(outer_w, outer_h):
    plate = box(outer_w, outer_h, BACK_T)
    sw = outer_w - 2 * WALL - 2 * BACK_FIT
    sh = outer_h - 2 * WALL - 2 * BACK_FIT
    skirt = box(sw, sh, SKIRT_H, z0=BACK_T) - box(sw - 2 * SKIRT_T, sh - 2 * SKIRT_T, SKIRT_H + 1, z0=BACK_T)
    part = plate + skirt

    z = BACK_T + SNAP_FROM_TOP
    for sign in (-1, 1):
        length = sw * 0.4
        bump = Manifold.cylinder(length, SNAP_R, SNAP_R, 24).rotate((0, 90, 0)).translate((-length / 2, sign * sh / 2, z))
        part = part + bump
    length = sh * 0.4
    bump = Manifold.cylinder(length, SNAP_R, SNAP_R, 24).rotate((90, 0, 0)).translate((-sw / 2, length / 2, z))
    part = part + bump

    for c in range(VENT_COLS):
        for r in range(VENT_ROWS):
            x = VENT_X + (c - (VENT_COLS - 1) / 2) * VENT_PITCH
            y = -VENT_Y + (r - (VENT_ROWS - 1) / 2) * VENT_PITCH
            part = part - Manifold.cylinder(BACK_T + 2, VENT_D / 2, VENT_D / 2, 24).translate((x, y, -1))

    notch_x = sw / 2 - SKIRT_T / 2
    part = part - box(SKIRT_T + 2, CABLE_SLOT_W, SKIRT_H + 1, cx=notch_x, cy=-CABLE_SLOT_Y, z0=BACK_T)
    return part


def stand(outer_w):
    length = max(STAND_LEN, outer_w + 10)
    slot_w = DEPTH + BACK_T + SLOT_CLEARANCE
    lip_d = 4.0
    back_d = 6.0
    stand_depth = lip_d + slot_w + back_d
    base = box(length, stand_depth, STAND_BASE, cy=0)
    lip = box(length, lip_d, STAND_LIP, cy=-stand_depth / 2 + lip_d / 2)
    rear = box(length, back_d, STAND_BACK, cy=stand_depth / 2 - back_d / 2)
    body = base + lip + rear
    slot_y = -stand_depth / 2 + lip_d + slot_w / 2
    slot = (
        box(length + 2, slot_w, 60, z0=0)
        .rotate((-STAND_TILT, 0, 0))
        .translate((0, slot_y, STAND_BASE))
    )
    return body - slot


def save(m, name):
    mesh = m.to_mesh()
    tm = trimesh.Trimesh(vertices=np.asarray(mesh.vert_properties)[:, :3], faces=np.asarray(mesh.tri_verts))
    assert tm.is_watertight, name
    tm.export(OUT / name)
    print(name, tm.bounds.round(2).tolist())


if __name__ == "__main__":
    f, w, h = frame()
    save(f, "timebuddy_frame.stl")
    save(back(w, h), "timebuddy_back.stl")
    save(stand(w), "timebuddy_stand.stl")

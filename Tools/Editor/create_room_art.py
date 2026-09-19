"""Art pass over the cell, corridor 1 and the guard station of L_M01_CellBlockD.

Turns Frank's cell and corridor 1 from lit grey boxes into a dark black-site prison:
procedural concrete, a steel cell door left open, fluorescent tubes (one with a bad
ballast), a red emergency lamp at the far end, pipes and panels to break up the walls,
a ceiling over the whole map so the sun stops flooding the interior, and a post process
that lets the dark stay dark. The guard station - the room the keycard door is in - gets
the same concrete and two steady fluorescents, because the player has to be able to see
the lock they are meant to solve.

Everything is cubes and cylinders from /Engine/BasicShapes plus the procedural materials
in _materials.py. No imported art, nothing downloaded.

Rules this script obeys:

  * every actor it creates is labelled ``Art_...`` so the pass can be found and undone
  * no gameplay actor is moved, retyped or deleted - guards, patrol points, triggers,
    the door, pickups, PlayerStart and the nav volume are read-only here
  * idempotent: a re-run fills gaps and leaves everything else alone
  * anything it places whose footprint comes within CLEARANCE cm of a patrol TargetPoint
    is logged, because that is a guard walking into the scenery

Run headless (after the other content scripts):

    UnrealEditor-Cmd.exe Castle.uproject -run=pythonscript ^
        -script="Tools\\Editor\\create_room_art.py" -unattended -nullrhi -nosplash -nop4 -stdout
"""

import math
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _common as c  # noqa: E402
import _materials as m  # noqa: E402

MAP_PATH = "/Game/Maps/L_M01_CellBlockD"
CUBE_PATH = "/Engine/BasicShapes/Cube.Cube"
CYLINDER_PATH = "/Engine/BasicShapes/Cylinder.Cylinder"
GREYBOX_PATH = "/Game/Kit/Materials/M_Greybox"

ART_PREFIX = "Art_"

# Any art actor whose footprint passes within this many cm of a patrol point is logged,
# unless it hangs entirely above a guard's head.
CLEARANCE = 60.0
HEAD_HEIGHT = 200.0

# --- bounds -----------------------------------------------------------------------------
# Mirrors create_sandbox_map.M01_WALLS. Interior extents, in cm:
#   cell        x    0..300   y -150..150   (doorway in the x = 300 wall is y -50..50)
#   corridor 1  x  300..2300  y -150..150   (station opening in the x = 2300 wall)
# Walls are 20 thick and 400 tall, so the ceiling sits at z = 400.
WALL_HEIGHT = 400.0
CEILING_THICK = 20.0
CEILING_Z = WALL_HEIGHT + CEILING_THICK / 2.0

CELL_X = (0.0, 300.0)
CORR1_X = (300.0, 2300.0)
STATION_X = (2300.0, 2900.0)
STATION_Y = (-300.0, 300.0)
ROOM_Y = (-150.0, 150.0)
DOORWAY_Y = (-50.0, 50.0)

# The two wall stubs either side of the cell doorway, as create_sandbox_map now places them.
# A map built before the doorway was widened still has the old 120-wide stubs, so this is the
# table the art pass moves them onto. (label, center, size)
CELL_DOOR_WALLS = (
    ("Wall_CellDoor_S", (300.0, -100.0, 200.0), (20.0, 100.0, 400.0)),
    ("Wall_CellDoor_N", (300.0, 100.0, 200.0), (20.0, 100.0, 400.0)),
)

# Wall actors create_sandbox_map placed that belong to the cell or corridor 1.
ART_WALL_LABELS = (
    "Wall_Cell_Back",
    "Wall_Cell_South",
    "Wall_Cell_North",
    "Wall_CellDoor_S",
    "Wall_CellDoor_N",
    "Wall_Corr1_South",
    "Wall_Corr1_North",
    "Wall_StationIn_S",
    "Wall_StationIn_N",
    # The guard station: the room the keycard door is in, so it gets the same treatment.
    "Wall_Station_South",
    "Wall_Station_North",
    "Wall_SecurityDoor_S",
    "Wall_SecurityDoor_N",
)

# (label, cx, cy, sx, sy, material key). The first three are the art rooms; the rest just
# stop the sun, so they stay on the greybox material.
CEILINGS = (
    ("Art_Ceiling_Cell", 150.0, 0.0, 340.0, 340.0, "concrete"),
    ("Art_Ceiling_Corr1", 1300.0, 0.0, 2000.0, 340.0, "concrete"),
    ("Art_Ceiling_Station", 2600.0, 0.0, 640.0, 640.0, "concrete"),
    ("Art_Ceiling_Corr2", 3900.0, 0.0, 2000.0, 340.0, "greybox"),
    ("Art_Ceiling_Exit", 5200.0, 0.0, 640.0, 640.0, "greybox"),
)

# A 4 cm skim of finished floor over the art rooms only - the map's single Floor slab
# runs the whole level and the other rooms stay grey. Top face lands at z = +2.
FLOOR_SKIMS = (
    ("Art_Floor_Cell", 150.0, 0.0, 340.0, 320.0),
    ("Art_Floor_Corr1", 1300.0, 0.0, 2000.0, 320.0),
    ("Art_Floor_Station", 2600.0, 0.0, 640.0, 640.0),
)

TUBE_SIZE = (120.0, 10.0, 5.0)
TUBE_Z = 392.0
TUBE_LIGHT_Z = 380.0

# (suffix, x, y, material key, light intensity in candelas). A 2500 lm tube is roughly
# 200 cd; these run a little under that. Corridor 1 is 20 m long, so it needs five of them -
# two lamps left 15 m of corridor in the dark.
FLUORESCENTS = (
    ("Cell", 150.0, 0.0, "tube", 250.0),
    ("Corr1_A", 800.0, 0.0, "tube", 100.0),
    ("Corr1_B", 1800.0, 0.0, "flicker", 60.0),
    ("Corr1_C", 400.0, 0.0, "tube", 100.0),
    ("Corr1_D", 1200.0, 0.0, "tube", 100.0),
    ("Corr1_E", 2200.0, 0.0, "tube", 100.0),
    # The guard station is the room the player has to solve, so it is the brightest thing
    # in the level. Station_A hangs a metre short of the keycard door to point at it.
    ("Station_A", 2800.0, 0.0, "tube", 100.0),
    ("Station_B", 2450.0, 0.0, "tube", 100.0),
)

# (label, center, size, material key, yaw)
CELL_DRESSING = (
    ("Art_Bunk", (140.0, -100.0, 45.0), (200.0, 80.0, 20.0), "steel", 0.0),
    ("Art_Toilet", (45.0, 115.0, 20.0), (40.0, 40.0, 40.0), "keycard", 0.0),
    ("Art_Drain", (150.0, 40.0, 3.0), (60.0, 60.0, 4.0), "steel", 0.0),
    ("Art_CellDoor_Jamb_S", (300.0, -55.0, 110.0), (30.0, 10.0, 220.0), "steel", 0.0),
    ("Art_CellDoor_Jamb_N", (300.0, 55.0, 110.0), (30.0, 10.0, 220.0), "steel", 0.0),
    ("Art_CellDoor_Lintel", (300.0, 0.0, 225.0), (30.0, 120.0, 10.0), "steel", 0.0),
    # Above the lintel the wall simply stopped, so from inside the cell the doorway read as a
    # black slot running up to the ceiling. This fills z 230..400 across the 100 cm opening.
    ("Art_CellDoor_Header", (300.0, 0.0, 315.0), (20.0, 100.0, 170.0), "concrete", 0.0),
    # Hinged on the north jamb at (300, 50) and swung 70 degrees into the corridor, so the
    # doorway and the leave_cell trigger stay clear. Half of the 100 cm leaf, 70 degrees off
    # the closed line, puts its centre 47.0 forward of the hinge and 17.1 south of it.
    ("Art_CellDoor_Slab", (347.0, 32.9, 110.0), (8.0, 100.0, 210.0), "steel", 70.0),
)

CORRIDOR_DRESSING = (
    ("Art_Panel_A", (700.0, -134.0, 200.0), (60.0, 12.0, 80.0), "steel", 0.0),
    ("Art_Panel_B", (1600.0, -134.0, 210.0), (50.0, 12.0, 60.0), "steel", 0.0),
    ("Art_Camera_Body", (2250.0, -128.0, 340.0), (24.0, 16.0, 16.0), "steel", 0.0),
)

# (label, center, radius, length, material key) - laid along X against the north wall.
CORRIDOR_PIPES = (
    ("Art_Pipe_A", (1300.0, 130.0, 356.0), 7.0, 2000.0, "steel"),
    ("Art_Pipe_B", (1300.0, 130.0, 338.0), 7.0, 2000.0, "steel"),
    ("Art_Pipe_C", (1300.0, 131.0, 320.0), 8.0, 2000.0, "steel"),
)

# The guard station, x 2300..2900, y -300..300. BP_Door_Keycard sits at (2900, 0) with its
# own frame mesh 140 wide and 260 tall, so the steel frame here wraps outside that: jambs at
# y +-80, a lintel at z 270, and a concrete header closing the slot up to the ceiling.
# The spare pistol (2700, -200) and keycard (2700, 200) pickups are left alone.
# (label, center, size, material key, yaw)
STATION_DRESSING = (
    ("Art_Station_Desk", (2450.0, -250.0, 37.5), (160.0, 70.0, 75.0), "steel", 0.0),
    ("Art_Station_Monitor", (2450.0, -262.0, 95.0), (50.0, 8.0, 30.0), "monitor", 0.0),
    ("Art_Station_Locker_A", (2400.0, 265.0, 90.0), (40.0, 50.0, 180.0), "steel", 0.0),
    ("Art_Station_Locker_B", (2445.0, 265.0, 90.0), (40.0, 50.0, 180.0), "steel", 0.0),
    ("Art_Station_Locker_C", (2490.0, 265.0, 90.0), (40.0, 50.0, 180.0), "steel", 0.0),
    ("Art_Station_CardReader", (2882.0, -75.0, 120.0), (16.0, 16.0, 24.0), "steel", 0.0),
    # Red because a material instance cannot switch to green from Python; the door Blueprint
    # owns that state and can swap the material when it unlocks.
    ("Art_Station_ReaderLed", (2872.0, -75.0, 126.0), (4.0, 6.0, 4.0), "red", 0.0),
    ("Art_Station_DoorJamb_S", (2900.0, -80.0, 130.0), (30.0, 20.0, 260.0), "steel", 0.0),
    ("Art_Station_DoorJamb_N", (2900.0, 80.0, 130.0), (30.0, 20.0, 260.0), "steel", 0.0),
    # 60 tall and centred at 250, not 20 at 270: the door leaf stops at z 220 and the header
    # starts at 280, and the slot between them was an open hole through into corridor 2.
    ("Art_Station_DoorLintel", (2900.0, 0.0, 250.0), (30.0, 180.0, 60.0), "steel", 0.0),
    ("Art_Station_DoorHeader", (2900.0, 0.0, 340.0), (20.0, 160.0, 120.0), "concrete", 0.0),
)

# --- baseline lighting ------------------------------------------------------------------
# Every room past the keycard door is still greybox - Cameron dresses one room at a time - but
# a greybox room the player cannot see is not a greybox room, it is a black screen. Ceilings
# went over the whole map, which is what took the sun out of corridor 2 and the exit room and
# left them pitch black. These are plain steady tubes at the same 100 cd as corridor 1, labelled
# Art_Base_ so the real art pass for each room can replace them without hunting.
#
# (suffix, x, y). Corridors get one every 500 cm, rooms one in the middle.
BASELINE_FLUORESCENTS = (
    ("Corr2_A", 3150.0, 0.0),
    ("Corr2_B", 3650.0, 0.0),
    ("Corr2_C", 4150.0, 0.0),
    ("Corr2_D", 4650.0, 0.0),
    ("Exit", 5200.0, 0.0),
)

BASELINE_INTENSITY = 100.0

# A red lamp at the mouth of the exit room, so the last room reads as the way out rather than
# as more corridor. (lamp label, lamp centre, size), (light label, light centre).
BASELINE_RED_LAMP = ("Art_Base_RedEmergency_Exit", (4960.0, 278.0, 300.0), (16.0, 20.0, 26.0))
BASELINE_RED_LIGHT = ("Art_Base_Light_RedExit", (4960.0, 262.0, 295.0))

# Green exit sign hanging over the stairwell trigger at (5200, 0).
BASELINE_EXIT_SIGN = ("Art_Base_ExitSign_Exit", (5200.0, 0.0, 330.0), (10.0, 60.0, 16.0))

# Floor-centre samples every room has to have a light near, checked by verify_room_art.py.
# (name, x, y). Nothing here may be further than LIGHT_REACH cm from a light actor.
ROOM_LIGHT_SAMPLES = (
    ("cell", 150.0, 0.0),
    ("corridor1_west", 400.0, 0.0),
    ("corridor1_mid", 1300.0, 0.0),
    ("corridor1_east", 2200.0, 0.0),
    ("station", 2600.0, 0.0),
    ("corridor2_west", 3150.0, 0.0),
    ("corridor2_mid", 3900.0, 0.0),
    ("corridor2_east", 4650.0, 0.0),
    ("exit_room", 5200.0, 0.0),
)

LIGHT_REACH = 600.0

RED_LAMP = ("Art_RedEmergency", (2240.0, 130.0, 300.0), (16.0, 20.0, 26.0))
RED_LIGHT = ("Art_Light_RedEmergency", (2225.0, 118.0, 295.0))
EXIT_SIGN = ("Art_ExitSign_Station", (2290.0, 0.0, 330.0), (10.0, 60.0, 16.0))

# --- room brightness ----------------------------------------------------------------
# Config/DefaultEngine.ini turns auto exposure off project-wide, so the scene renders at
# a fixed exposure that would blow a fluorescent-lit interior out to white without a
# stop-down. Two presets, one env var, one number to change to revert:
#
#   default (CASTLE_BRIGHT unset or "0")  -> ROOM_EXPOSURE_EV_NORMAL, dark but seeable,
#                                             the shipped look
#   CASTLE_BRIGHT=1                       -> ROOM_EXPOSURE_EV_TESTING, brighter, for
#                                             playtesting layout and AI
#
# Tools/create-content.ps1 -Bright sets CASTLE_BRIGHT=1 for the content-script process.
# The numbers themselves live in _materials, because create_world_blueprints.py asks for the
# same lamp instances and the two have to agree or each run undoes the other's tuning.
ROOM_EXPOSURE_EV_NORMAL = m.ROOM_EXPOSURE_EV_NORMAL   # the shipped look: dark but seeable
ROOM_EXPOSURE_EV_TESTING = m.ROOM_EXPOSURE_EV_TESTING  # bright, for playtesting layout and AI
BRIGHT = m.BRIGHT
ROOM_EXPOSURE_EV = m.ROOM_EXPOSURE_EV
EMISSIVE_INTENSITY_FACTOR = m.EMISSIVE_INTENSITY_FACTOR

COOL_WHITE = (200, 220, 255)
EMERGENCY_RED = (255, 25, 10)
DARK_BLUE = (40, 60, 110)

_STATE = {"changed": 0, "materials": {}}


# --------------------------------------------------------------------------------------
# level / actor plumbing
# --------------------------------------------------------------------------------------


def load_level(package_path):
    subsystem = c.level_editor_subsystem()
    if subsystem is not None:
        return bool(subsystem.load_level(package_path))
    if hasattr(unreal, "EditorLevelLibrary"):
        return bool(unreal.EditorLevelLibrary.load_level(package_path))
    return False


def save_level():
    subsystem = c.level_editor_subsystem()
    if subsystem is not None:
        return bool(subsystem.save_current_level())
    if hasattr(unreal, "EditorLevelLibrary"):
        return bool(unreal.EditorLevelLibrary.save_current_level())
    return False


def label_of(actor):
    try:
        return actor.get_actor_label()
    except Exception:  # noqa: BLE001
        return "<unlabelled>"


def find_actor_by_label(label):
    for actor in c.all_level_actors():
        if label_of(actor) == label:
            return actor
    return None


def touched(count=1):
    _STATE["changed"] += count


def color(rgb):
    """FColor from a 0-255 (r, g, b) tuple."""
    return unreal.Color(r=int(rgb[0]), g=int(rgb[1]), b=int(rgb[2]), a=255)


def mesh(path):
    return c.load_or_none(path) or unreal.load_object(None, path)


def material(key):
    return _STATE["materials"].get(key)


def set_props_if_changed(obj, values, context=""):
    """set_editor_property only where the value actually differs. Returns names written.

    This is what keeps a second run silent: the lighting, fog and post-process steps all
    write the same numbers every time, and writing them dirties the map.
    """
    applied = []
    for prop, value in values:
        try:
            current = obj.get_editor_property(prop)
        except Exception:  # noqa: BLE001 - property may not exist in this build
            current = None
        if m.same_value(current, value):
            continue
        try:
            obj.set_editor_property(prop, value)
            applied.append(prop)
        except Exception as exc:  # noqa: BLE001
            unreal.log_warning("[Castle] skipped   {0}.{1}  ({2}: {3})".format(
                context or c.safe_name(obj), prop, type(exc).__name__, exc))
    return applied


def set_first_prop_if_changed(obj, names, value, context=""):
    """First spelling of a property that exists, written only when it differs."""
    for prop in names:
        try:
            current = obj.get_editor_property(prop)
        except Exception:  # noqa: BLE001
            continue
        if m.same_value(current, value):
            return None
        try:
            obj.set_editor_property(prop, value)
            return prop
        except Exception:  # noqa: BLE001
            continue
    return None


def apply_material(actor, mat):
    """Assign slot 0 if it isn't already that material. Returns True when it changed."""
    if mat is None:
        return False
    try:
        component = actor.get_editor_property("static_mesh_component")
        overrides = list(component.get_editor_property("override_materials") or [])
        if overrides and overrides[0] == mat:
            return False
        component.set_material(0, mat)
        return True
    except Exception as exc:  # noqa: BLE001
        c.log_error("apply_material " + label_of(actor), exc)
        return False


# --------------------------------------------------------------------------------------
# patrol clearance
# --------------------------------------------------------------------------------------


def patrol_points():
    points = []
    for actor in c.all_level_actors():
        if isinstance(actor, unreal.TargetPoint):
            try:
                loc = actor.get_actor_location()
                points.append((label_of(actor), loc.x, loc.y))
            except Exception:  # noqa: BLE001
                continue
    return points


def check_clearance(label, center, size, yaw=0.0):
    """Log any patrol TargetPoint within CLEARANCE cm of this actor's XY footprint.

    A rotated box is approximated by its bounding circle, which is the pessimistic read -
    it will warn slightly early rather than let a guard walk into a prop. Anything whose
    underside clears head height is skipped: a ceiling lamp over a patrol point is fine.
    """
    try:
        if center[2] - size[2] / 2.0 >= HEAD_HEIGHT:
            return
        half_x, half_y = size[0] / 2.0, size[1] / 2.0
        if yaw:
            radius = math.hypot(half_x, half_y)
            half_x = half_y = radius
        for name, px, py in _STATE.get("patrol", []):
            dx = max(abs(px - center[0]) - half_x, 0.0)
            dy = max(abs(py - center[1]) - half_y, 0.0)
            distance = math.hypot(dx, dy)
            if distance < CLEARANCE:
                c.log(
                    "warn",
                    label,
                    "{0:.0f} cm from patrol point {1}".format(distance, name),
                )
    except Exception as exc:  # noqa: BLE001
        c.log_error("check_clearance " + label, exc)


# --------------------------------------------------------------------------------------
# primitives
# --------------------------------------------------------------------------------------


def near(a, b, tol=0.05):
    return abs(a - b) <= tol


def ensure_transform(actor, label, center, size, yaw=0.0):
    """Move an existing actor onto the placement its table now asks for. Returns True if moved.

    The tables above are the source of truth, so a layout change (widening the doorway, say)
    corrects what is already in the map instead of needing the level rebuilt. Every component
    is compared before it is written, so a second run writes nothing and the map stays clean.
    """
    changed = []
    try:
        location = actor.get_actor_location()
        if not all(near(getattr(location, axis), center[i]) for i, axis in enumerate("xyz")):
            actor.set_actor_location(unreal.Vector(*center), False, False)
            changed.append("location")

        rotation = actor.get_actor_rotation()
        if not (near(rotation.roll, 0.0) and near(rotation.pitch, 0.0) and near(rotation.yaw, yaw)):
            actor.set_actor_rotation(unreal.Rotator(0.0, 0.0, yaw), False)
            changed.append("rotation")

        wanted_scale = [value / 100.0 for value in size]
        scale = actor.get_actor_scale3d()
        if not all(near(getattr(scale, axis), wanted_scale[i], 0.0005) for i, axis in enumerate("xyz")):
            actor.set_actor_scale3d(unreal.Vector(*wanted_scale))
            changed.append("scale")
    except Exception as exc:  # noqa: BLE001
        c.log_error("ensure_transform " + label, exc)
        return False

    if not changed:
        return False
    touched()
    c.log("updated", label, ", ".join(changed))
    return True


def ensure_box(label, center, size, material_key, yaw=0.0, static=True):
    """Idempotent cube StaticMeshActor. Returns (actor, created)."""
    try:
        existing = find_actor_by_label(label)
        if existing is not None:
            moved = ensure_transform(existing, label, center, size, yaw)
            if apply_material(existing, material(material_key)):
                touched()
                c.log("updated", label, "material -> " + material_key)
            elif not moved:
                c.log("exists", label)
            return existing, False

        rotation = unreal.Rotator(0.0, 0.0, yaw)
        actor = c.spawn_actor(
            unreal.StaticMeshActor, unreal.Vector(*center), rotation, label=label)
        if actor is None:
            c.log("FAILED", label, "spawn_actor returned None")
            return None, False

        component = actor.get_editor_property("static_mesh_component")
        cube = mesh(CUBE_PATH)
        if cube is not None:
            component.set_static_mesh(cube)
        actor.set_actor_scale3d(
            unreal.Vector(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
        actor.set_mobility(
            unreal.ComponentMobility.STATIC if static else unreal.ComponentMobility.MOVABLE)
        apply_material(actor, material(material_key))
        touched()
        c.log("created", label, "{0:.0f}x{1:.0f}x{2:.0f} {3}".format(
            size[0], size[1], size[2], material_key))
        check_clearance(label, center, size, yaw)
        return actor, True
    except Exception as exc:  # noqa: BLE001
        c.log_error("ensure_box " + label, exc)
        return None, False


def ensure_pipe(label, center, radius, length, material_key):
    """Idempotent cylinder laid along X. /Engine/BasicShapes/Cylinder is r50 x h100."""
    try:
        existing = find_actor_by_label(label)
        if existing is not None:
            if apply_material(existing, material(material_key)):
                touched()
                c.log("updated", label, "material -> " + material_key)
            else:
                c.log("exists", label)
            return existing, False

        actor = c.spawn_actor(
            unreal.StaticMeshActor,
            unreal.Vector(*center),
            unreal.Rotator(0.0, 90.0, 0.0),
            label=label,
        )
        if actor is None:
            c.log("FAILED", label, "spawn_actor returned None")
            return None, False

        component = actor.get_editor_property("static_mesh_component")
        cylinder = mesh(CYLINDER_PATH)
        if cylinder is not None:
            component.set_static_mesh(cylinder)
        actor.set_actor_scale3d(
            unreal.Vector(radius / 50.0, radius / 50.0, length / 100.0))
        actor.set_mobility(unreal.ComponentMobility.STATIC)
        apply_material(actor, material(material_key))
        touched()
        c.log("created", label, "pipe r{0:.0f} x {1:.0f}".format(radius, length))
        check_clearance(label, center, (length, radius * 2.0))
        return actor, True
    except Exception as exc:  # noqa: BLE001
        c.log_error("ensure_pipe " + label, exc)
        return None, False


def light_component(actor, component_prop):
    """A light actor's light component, falling back to its root component."""
    if component_prop:
        try:
            component = actor.get_editor_property(component_prop)
            if component is not None:
                return component
        except Exception:  # noqa: BLE001
            pass
    try:
        return actor.get_editor_property("root_component")
    except Exception:  # noqa: BLE001
        return None


def ensure_light(actor_class, label, location, rotation=None, props=None, component_prop=None):
    """Idempotent light actor, Movable, with ``props`` applied to its light component."""
    try:
        existing = find_actor_by_label(label)
        if existing is not None:
            # Re-apply the tuning: intensity and colour are values we iterate on.
            component = light_component(existing, component_prop)
            applied = set_props_if_changed(component, props or [], label) if component else []
            if applied:
                touched()
                c.log("updated", label, ", ".join(applied))
            else:
                c.log("exists", label)
            return existing, False
        if actor_class is None:
            c.log("skipped", label, "light class unavailable")
            return None, False

        actor = c.spawn_actor(actor_class, unreal.Vector(*location), rotation, label=label)
        if actor is None:
            c.log("FAILED", label, "spawn_actor returned None")
            return None, False

        c.set_actor_mobility_movable(actor)
        component = light_component(actor, component_prop)
        if component is not None and props:
            c.set_props(component, props, label)
        touched()
        c.log("created", label, c.class_name(actor_class))
        return actor, True
    except Exception as exc:  # noqa: BLE001
        c.log_error("ensure_light " + label, exc)
        return None, False


# --------------------------------------------------------------------------------------
# steps
# --------------------------------------------------------------------------------------


def step_wall_materials():
    """Concrete on the cell and corridor-1 walls. Other rooms keep M_Greybox."""
    concrete = material("concrete")
    if concrete is None:
        c.log("skipped", "wall materials", "M_Concrete unavailable")
        return
    for actor in c.all_level_actors():
        if not isinstance(actor, unreal.StaticMeshActor):
            continue
        label = label_of(actor)
        if label not in ART_WALL_LABELS:
            continue
        try:
            if apply_material(actor, concrete):
                touched()
                c.log("updated", label, "material -> M_Concrete")
            else:
                c.log("exists", label, "already M_Concrete")
        except Exception as exc:  # noqa: BLE001
            c.log_error("step_wall_materials " + label, exc)


def step_doorway_walls():
    """Widen the cell doorway to 100 cm on a map that was built with the old 80 cm one."""
    for label, center, size in CELL_DOOR_WALLS:
        actor = find_actor_by_label(label)
        if actor is None:
            c.log("skipped", label, "not in the map")
            continue
        if not ensure_transform(actor, label, center, size):
            c.log("exists", label, "doorway already 100 cm")


def step_floor_skim():
    for label, cx, cy, sx, sy in FLOOR_SKIMS:
        ensure_box(label, (cx, cy, 0.0), (sx, sy, 4.0), "concrete_floor")


def step_ceilings():
    for label, cx, cy, sx, sy, key in CEILINGS:
        ensure_box(label, (cx, cy, CEILING_Z), (sx, sy, CEILING_THICK), key)


def step_kill_daylight():
    """The map is indoors. The sun goes to a cold sliver and the sky light nearly off."""
    for actor in c.all_level_actors():
        try:
            if isinstance(actor, unreal.DirectionalLight):
                component = actor.get_editor_property("directional_light_component")
                applied = set_props_if_changed(
                    component,
                    [
                        ("intensity", 0.2),
                        ("light_color", color(DARK_BLUE)),
                        ("atmosphere_sun_light", False),
                    ],
                    label_of(actor),
                )
                if applied:
                    touched()
                    c.log("updated", label_of(actor), "sun -> 0.2 lux, cold tint")
                else:
                    c.log("exists", label_of(actor), "sun already dimmed")
            elif isinstance(actor, unreal.SkyLight):
                component = actor.get_editor_property("light_component")
                applied = set_props_if_changed(
                    component, [("intensity", 0.15)], label_of(actor))
                if applied:
                    touched()
                    c.log("updated", label_of(actor), "sky light -> 0.15")
                else:
                    c.log("exists", label_of(actor), "sky light already dimmed")
        except Exception as exc:  # noqa: BLE001
            c.log_error("step_kill_daylight " + label_of(actor), exc)


def step_fog():
    for actor in c.all_level_actors():
        if not isinstance(actor, unreal.ExponentialHeightFog):
            continue
        try:
            component = None
            for prop in ("component", "exponential_height_fog_component"):
                try:
                    component = actor.get_editor_property(prop)
                    if component is not None:
                        break
                except Exception:  # noqa: BLE001
                    continue
            if component is None:
                c.log("skipped", label_of(actor), "no fog component property")
                continue
            applied = set_props_if_changed(
                component,
                [
                    ("fog_density", 0.05),
                    ("fog_height_falloff", 0.12),
                    ("start_distance", 0.0),
                ],
                label_of(actor),
            )
            tint = set_first_prop_if_changed(
                component,
                ["fog_inscattering_luminance", "fog_inscattering_color"],
                unreal.LinearColor(0.10, 0.13, 0.18, 1.0),
                label_of(actor),
            )
            if applied or tint:
                touched()
                c.log("updated", label_of(actor), "density 0.05, cool inscatter")
            else:
                c.log("exists", label_of(actor), "fog already set")
        except Exception as exc:  # noqa: BLE001
            c.log_error("step_fog " + label_of(actor), exc)


def step_post_process():
    """Desaturate a touch, add contrast and vignette, and let dark rooms stay dark."""
    for actor in c.all_level_actors():
        if not isinstance(actor, unreal.PostProcessVolume):
            continue
        try:
            settings = actor.get_editor_property("settings")
            applied = set_props_if_changed(
                settings,
                [
                    ("override_color_saturation", True),
                    ("color_saturation", unreal.Vector4(0.85, 0.85, 0.85, 1.0)),
                    ("override_color_contrast", True),
                    ("color_contrast", unreal.Vector4(1.12, 1.12, 1.12, 1.0)),
                    ("override_vignette_intensity", True),
                    ("vignette_intensity", 0.25),
                    ("override_bloom_intensity", True),
                    ("bloom_intensity", 0.6),
                    ("override_auto_exposure_bias", True),
                    ("auto_exposure_bias", ROOM_EXPOSURE_EV),
                    ("override_auto_exposure_min_brightness", True),
                    ("auto_exposure_min_brightness", 0.6),
                    ("override_auto_exposure_max_brightness", True),
                    ("auto_exposure_max_brightness", 1.2),
                ],
                label_of(actor),
            )
            grain_override = set_first_prop_if_changed(
                settings,
                ["override_film_grain_intensity", "override_grain_intensity"],
                True,
                label_of(actor),
            )
            grain = set_first_prop_if_changed(
                settings,
                ["film_grain_intensity", "grain_intensity"],
                0.2,
                label_of(actor),
            )
            if not (applied or grain_override or grain):
                c.log("exists", label_of(actor), "post process already graded")
                continue
            actor.set_editor_property("settings", settings)
            touched()
            c.log("updated", label_of(actor), "saturation 0.85, vignette, stopped down")
        except Exception as exc:  # noqa: BLE001
            c.log_error("step_post_process " + label_of(actor), exc)


def ensure_fluorescent(prefix, suffix, x, y, key, intensity):
    """One fixture: a tube mesh at the ceiling and a rect light pointed down out of it."""
    ensure_box(prefix + "Tube_" + suffix, (x, y, TUBE_Z), TUBE_SIZE, key)

    props = [
        ("intensity", intensity),
        ("light_color", color(COOL_WHITE)),
        ("source_width", TUBE_SIZE[0]),
        ("source_height", TUBE_SIZE[1]),
        ("attenuation_radius", 1400.0),
        ("cast_shadows", True),
    ]
    units = getattr(unreal, "LightUnits", None)
    if units is not None and getattr(units, "CANDELAS", None) is not None:
        props.insert(0, ("intensity_units", units.CANDELAS))
    ensure_light(
        c.find_class("RectLight", "/Script/Engine.RectLight"),
        prefix + "Light_" + suffix,
        (x, y, TUBE_LIGHT_Z),
        # unreal.Rotator is (roll, pitch, yaw): a -90 pitch aims the panel at the floor.
        unreal.Rotator(0.0, -90.0, 0.0),
        props,
        "rect_light_component",
    )


def step_fluorescents():
    """A tube mesh at the ceiling plus a rect light under it, per fixture."""
    for suffix, x, y, key, intensity in FLUORESCENTS:
        ensure_fluorescent("Art_", suffix, x, y, key, intensity)
    c.log(
        "note",
        "Art_Tube_Corr1_B",
        "flicker is material-only (M_FluorescentFlicker); the rect light stays steady "
        "until UFlickerLightComponent exists",
    )


def step_emergency_light():
    label, center, size = RED_LAMP
    ensure_box(label, center, size, "red")

    light_label, light_center = RED_LIGHT
    ensure_light(
        unreal.PointLight,
        light_label,
        light_center,
        None,
        [
            ("intensity", 800.0),
            ("light_color", color(EMERGENCY_RED)),
            ("attenuation_radius", 600.0),
            ("cast_shadows", True),
        ],
        "point_light_component",
    )

    sign_label, sign_center, sign_size = EXIT_SIGN
    ensure_box(sign_label, sign_center, sign_size, "stripe")


def step_baseline_lighting():
    """Make every undressed room visible: steady tubes, a red lamp and a green exit sign.

    Nothing here is art. It is the floor under the art: a room whose only light source was the
    sun went black the moment the ceilings went on, and a black room cannot be playtested.
    """
    for suffix, x, y in BASELINE_FLUORESCENTS:
        ensure_fluorescent("Art_Base_", suffix, x, y, "tube", BASELINE_INTENSITY)

    lamp_label, lamp_center, lamp_size = BASELINE_RED_LAMP
    ensure_box(lamp_label, lamp_center, lamp_size, "red")

    light_label, light_center = BASELINE_RED_LIGHT
    ensure_light(
        unreal.PointLight,
        light_label,
        light_center,
        None,
        [
            ("intensity", 800.0),
            ("light_color", color(EMERGENCY_RED)),
            ("attenuation_radius", 600.0),
            ("cast_shadows", True),
        ],
        "point_light_component",
    )

    sign_label, sign_center, sign_size = BASELINE_EXIT_SIGN
    ensure_box(sign_label, sign_center, sign_size, "stripe")


def step_cell_dressing():
    for label, center, size, key, yaw in CELL_DRESSING:
        ensure_box(label, center, size, key, yaw)


def step_station_dressing():
    """The guard station: desk, monitor, lockers, card reader and the keycard door frame."""
    for label, center, size, key, yaw in STATION_DRESSING:
        ensure_box(label, center, size, key, yaw)


def step_corridor_dressing():
    for label, center, size, key, yaw in CORRIDOR_DRESSING:
        ensure_box(label, center, size, key, yaw)
    for label, center, radius, length, key in CORRIDOR_PIPES:
        ensure_pipe(label, center, radius, length, key)
    # Short stub holding the camera body off the wall.
    ensure_pipe("Art_Camera_Mount", (2250.0, -136.0, 348.0), 3.0, 22.0, "steel")


STEPS = (
    ("wall materials", step_wall_materials),
    ("doorway walls", step_doorway_walls),
    ("floor skim", step_floor_skim),
    ("ceilings", step_ceilings),
    ("daylight", step_kill_daylight),
    ("fog", step_fog),
    ("post process", step_post_process),
    ("fluorescents", step_fluorescents),
    ("baseline lighting", step_baseline_lighting),
    ("emergency light", step_emergency_light),
    ("cell dressing", step_cell_dressing),
    ("corridor dressing", step_corridor_dressing),
    ("station dressing", step_station_dressing),
)


def run():
    _STATE["changed"] = 0
    c.log(
        "note",
        "exposure preset",
        "{0} (CASTLE_BRIGHT={1}) EV {2:.1f}, emissive x{3:.3f}".format(
            "testing" if BRIGHT else "normal",
            os.environ.get("CASTLE_BRIGHT", "0"),
            ROOM_EXPOSURE_EV,
            EMISSIVE_INTENSITY_FACTOR,
        ),
    )
    materials = m.run(EMISSIVE_INTENSITY_FACTOR)
    greybox = c.load_or_none(GREYBOX_PATH)
    if greybox is None:
        unreal.log_warning("[Castle] M_Greybox missing; the spare ceilings stay untextured")
    materials["greybox"] = greybox
    _STATE["materials"] = materials

    if not load_level(MAP_PATH):
        c.log("FAILED", MAP_PATH, "could not open the level for the art pass")
        return False

    _STATE["patrol"] = patrol_points()
    c.log(
        "note",
        MAP_PATH,
        "cell x {0:.0f}..{1:.0f}, corridor 1 x {2:.0f}..{3:.0f} (y {4:.0f}..{5:.0f}), "
        "guard station x {6:.0f}..{7:.0f} (y {8:.0f}..{9:.0f})".format(
            CELL_X[0], CELL_X[1], CORR1_X[0], CORR1_X[1], ROOM_Y[0], ROOM_Y[1],
            STATION_X[0], STATION_X[1], STATION_Y[0], STATION_Y[1]),
    )
    c.log("note", MAP_PATH, "{0} patrol point(s) checked for clearance".format(
        len(_STATE["patrol"])))

    for title, fn in STEPS:
        unreal.log("[Castle] ---- {0} ----".format(title))
        try:
            fn()
        except Exception as exc:  # noqa: BLE001
            c.log_error("room art step '" + title + "'", exc)

    if _STATE["changed"]:
        save_level()
        c.log("updated", MAP_PATH, "{0} art change(s)".format(_STATE["changed"]))
        return True
    c.log("exists", MAP_PATH, "room art already in place")
    return False


if __name__ == "__main__":
    run()
    c.print_summary("room art")

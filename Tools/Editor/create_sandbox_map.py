"""Create the two stage-1/stage-2 maps.

    /Game/Maps/L_Sandbox          flat 4000x4000 box room for tuning movement
    /Game/Maps/L_M01_CellBlockD   rough greybox of Cell Block D per docs/plans/02-prototype.md

Both get lighting, a PlayerStart and a World Settings GameMode override pointing at
BP_CastleGameMode. The mission map also gets four objective trigger volumes
(AObjectiveTriggerVolume when it is spawnable from Python, plain ATriggerBox otherwise).

Geometry is cubes: /Engine/BasicShapes/Cube is 100 cm, so scale = size_cm / 100.
Walls are 20 cm thick and 400 cm tall, matching claude-docs/asset-conventions.md.
"""

import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _common as c  # noqa: E402

MAPS_PATH = "/Game/Maps"
PLAYER_PATH = "/Game/Blueprints/Player"
CUBE_PATH = "/Engine/BasicShapes/Cube.Cube"

WALL_HEIGHT = 400.0
WALL_THICK = 20.0

# --- L_M01_CellBlockD layout (all in cm, +X is "forward, towards the exit") ------------
#   cell          x    0..300    y -150..150
#   corridor 1    x  300..2300   y -150..150
#   guard station x 2300..2900   y -300..300
#   corridor 2    x 2900..4900   y -150..150
#   exit room     x 4900..5500   y -300..300
#
# (label, center x, center y, size x, size y)
M01_WALLS = [
    ("Wall_Cell_Back", 0.0, 0.0, WALL_THICK, 320.0),
    ("Wall_Cell_South", 150.0, -150.0, 300.0, WALL_THICK),
    ("Wall_Cell_North", 150.0, 150.0, 300.0, WALL_THICK),
    ("Wall_CellDoor_S", 300.0, -100.0, WALL_THICK, 120.0),
    ("Wall_CellDoor_N", 300.0, 100.0, WALL_THICK, 120.0),
    ("Wall_Corr1_South", 1300.0, -150.0, 2000.0, WALL_THICK),
    ("Wall_Corr1_North", 1300.0, 150.0, 2000.0, WALL_THICK),
    ("Wall_Station_South", 2600.0, -300.0, 600.0, WALL_THICK),
    ("Wall_Station_North", 2600.0, 300.0, 600.0, WALL_THICK),
    ("Wall_StationIn_S", 2300.0, -225.0, WALL_THICK, 150.0),
    ("Wall_StationIn_N", 2300.0, 225.0, WALL_THICK, 150.0),
    ("Wall_SecurityDoor_S", 2900.0, -175.0, WALL_THICK, 250.0),
    ("Wall_SecurityDoor_N", 2900.0, 175.0, WALL_THICK, 250.0),
    ("Wall_Corr2_South", 3900.0, -150.0, 2000.0, WALL_THICK),
    ("Wall_Corr2_North", 3900.0, 150.0, 2000.0, WALL_THICK),
    ("Wall_Exit_South", 5200.0, -300.0, 600.0, WALL_THICK),
    ("Wall_Exit_North", 5200.0, 300.0, 600.0, WALL_THICK),
    ("Wall_ExitIn_S", 4900.0, -225.0, WALL_THICK, 150.0),
    ("Wall_ExitIn_N", 4900.0, 225.0, WALL_THICK, 150.0),
    ("Wall_Exit_Back", 5500.0, 0.0, WALL_THICK, 620.0),
]

# (label, ObjectiveId, x, y)
M01_TRIGGERS = [
    ("OBJ_leave_cell", "leave_cell", 350.0, 0.0),
    ("OBJ_find_weapon", "find_weapon", 2600.0, 0.0),
    ("OBJ_security_door", "security_door", 2950.0, 0.0),
    ("OBJ_reach_stairwell", "reach_stairwell", 5200.0, 0.0),
]


def cube_mesh():
    return c.load_or_none(CUBE_PATH) or unreal.load_object(None, CUBE_PATH)


def add_box(mesh, label, center, size):
    """Cube StaticMeshActor. ``center`` and ``size`` are (x, y, z) in cm."""
    actor = c.spawn_actor(unreal.StaticMeshActor, unreal.Vector(*center), label=label)
    if actor is None:
        return None
    try:
        component = actor.get_editor_property("static_mesh_component")
        if mesh is not None:
            component.set_static_mesh(mesh)
        actor.set_actor_scale3d(unreal.Vector(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
        actor.set_mobility(unreal.ComponentMobility.STATIC)
    except Exception as exc:  # noqa: BLE001
        c.log_error("add_box " + label, exc)
    return actor


def add_wall(mesh, label, cx, cy, sx, sy):
    return add_box(mesh, label, (cx, cy, WALL_HEIGHT / 2.0), (sx, sy, WALL_HEIGHT))


def add_lighting():
    c.spawn_actor(
        unreal.DirectionalLight,
        unreal.Vector(0.0, 0.0, 2000.0),
        unreal.Rotator(0.0, -45.0, 0.0),
        label="Sun",
    )
    c.spawn_actor(unreal.SkyLight, unreal.Vector(0.0, 0.0, 1000.0), label="SkyLight")
    c.spawn_actor(
        unreal.ExponentialHeightFog, unreal.Vector(0.0, 0.0, 0.0), label="HeightFog"
    )
    atmosphere = c.find_class("SkyAtmosphere", "/Script/Engine.SkyAtmosphere")
    if atmosphere is None:
        atmosphere = c.find_class("AtmosphericFog", "/Script/Engine.AtmosphericFog")
    if atmosphere is not None:
        c.spawn_actor(atmosphere, unreal.Vector(0.0, 0.0, 0.0), label="SkyAtmosphere")
    else:
        unreal.log_warning("[Castle] neither SkyAtmosphere nor AtmosphericFog is available")


def apply_game_mode(level_label):
    game_mode = c.load_generated_class(PLAYER_PATH, "BP_CastleGameMode")
    if game_mode is None:
        c.log("skipped", level_label, "BP_CastleGameMode_C not found; GameMode override unset")
        return False
    return c.set_level_game_mode(game_mode, level_label)


def load_level(package_path):
    subsystem = c.level_editor_subsystem()
    if subsystem is not None:
        return bool(subsystem.load_level(package_path))
    if hasattr(unreal, "EditorLevelLibrary"):
        return bool(unreal.EditorLevelLibrary.load_level(package_path))
    return False


def ensure_game_mode(package_path):
    """For a map that already exists: set the GameMode override if it isn't set yet.

    The first run creates the maps before the Blueprints exist, so the override has to be
    fixable on a later pass instead of only at creation time.
    """
    game_mode = c.load_generated_class(PLAYER_PATH, "BP_CastleGameMode")
    if game_mode is None:
        c.log("exists", package_path, "BP_CastleGameMode_C not found; override left unset")
        return False
    if not load_level(package_path):
        c.log("exists", package_path, "could not open level to check GameMode override")
        return False

    settings = c.world_settings()
    if settings is not None:
        try:
            if settings.get_editor_property("default_game_mode") == game_mode:
                c.log("exists", package_path, "GameMode override already set")
                return False
        except Exception:  # noqa: BLE001
            pass

    if c.set_level_game_mode(game_mode, package_path):
        save_level()
        c.log("updated", package_path, "GameMode override = BP_CastleGameMode_C")
        return True
    c.log("exists", package_path, "GameMode override could not be set")
    return False


def new_level(package_path):
    """True when a fresh empty level was created at package_path."""
    subsystem = c.level_editor_subsystem()
    if subsystem is not None:
        return bool(subsystem.new_level(package_path))
    if hasattr(unreal, "EditorLevelLibrary"):
        return bool(unreal.EditorLevelLibrary.new_level(package_path))
    c.log("FAILED", package_path, "no LevelEditorSubsystem / EditorLevelLibrary")
    return False


def save_level():
    subsystem = c.level_editor_subsystem()
    if subsystem is not None:
        return bool(subsystem.save_current_level())
    if hasattr(unreal, "EditorLevelLibrary"):
        return bool(unreal.EditorLevelLibrary.save_current_level())
    return False


def build_sandbox():
    full = c.asset_path(MAPS_PATH, "L_Sandbox")
    if c.exists(full):
        ensure_game_mode(full)
        return False
    if not new_level(full):
        c.log("FAILED", full, "new_level returned false")
        return False

    mesh = cube_mesh()
    add_box(mesh, "Floor", (0.0, 0.0, -5.0), (4000.0, 4000.0, 10.0))
    add_wall(mesh, "Wall_East", 2000.0, 0.0, WALL_THICK, 4000.0)
    add_wall(mesh, "Wall_West", -2000.0, 0.0, WALL_THICK, 4000.0)
    add_wall(mesh, "Wall_North", 0.0, 2000.0, 4000.0, WALL_THICK)
    add_wall(mesh, "Wall_South", 0.0, -2000.0, 4000.0, WALL_THICK)

    c.spawn_actor(unreal.PlayerStart, unreal.Vector(0.0, 0.0, 100.0), label="PlayerStart")
    add_lighting()
    apply_game_mode(full)

    save_level()
    c.log("created", full, "floor, 4 walls, PlayerStart, lighting")
    return True


def trigger_class():
    cls = c.find_class("ObjectiveTriggerVolume", "/Script/Castle.ObjectiveTriggerVolume")
    if cls is not None:
        return cls, True
    return unreal.TriggerBox, False


def build_cell_block_d():
    full = c.asset_path(MAPS_PATH, "L_M01_CellBlockD")
    if c.exists(full):
        ensure_game_mode(full)
        return False
    if not new_level(full):
        c.log("FAILED", full, "new_level returned false")
        return False

    mesh = cube_mesh()
    # One slab under the whole run: x -100..5700, y -400..400.
    add_box(mesh, "Floor", (2800.0, 0.0, -5.0), (5800.0, 800.0, 10.0))
    for label, cx, cy, sx, sy in M01_WALLS:
        add_wall(mesh, label, cx, cy, sx, sy)

    c.spawn_actor(unreal.PlayerStart, unreal.Vector(150.0, 0.0, 100.0), label="PlayerStart")
    add_lighting()

    cls, is_objective_volume = trigger_class()
    for label, tag, x, y in M01_TRIGGERS:
        actor = c.spawn_actor(cls, unreal.Vector(x, y, 100.0), label=label)
        if actor is None:
            c.log("FAILED", full, "could not spawn trigger " + label)
            continue
        if is_objective_volume:
            # The C++ field has been called both ObjectiveId and ObjectiveTag; try both.
            c.set_first_prop(actor, ["objective_id", "objective_tag"], tag, label)
    if not is_objective_volume:
        c.log(
            "skipped",
            full,
            "AObjectiveTriggerVolume unavailable; placed plain TriggerBoxes to replace by hand",
        )

    apply_game_mode(full)

    save_level()
    c.log(
        "created",
        full,
        "{0} walls, 4 objective triggers, PlayerStart, lighting".format(len(M01_WALLS)),
    )
    return True


def run():
    c.ensure_directory(MAPS_PATH)
    build_sandbox()
    build_cell_block_d()


if __name__ == "__main__":
    run()
    c.print_summary("maps")

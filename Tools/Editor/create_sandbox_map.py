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

MATERIALS_PATH = "/Game/Kit/Materials"

WALL_HEIGHT = 400.0
WALL_THICK = 20.0

# Label prefixes the mesh-material fixup recognises, in priority order. "Floor" is more
# specific than "Wall_" would ever collide with, but keep the order anyway.
MESH_LABEL_MATERIAL_KIND = (
    ("Floor", "floor"),
    ("Wall_", "greybox"),
)

# Filled in by ensure_greybox_materials(); read by add_box() and fix_existing_materials().
_GREYBOX_MATERIALS = {}

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
# security_door is completed by BP_Door_Keycard itself and find_weapon by the pistol pickup, so
# neither gets a trigger volume. leave_cell sits just outside the cell doorway at x = 300.
M01_TRIGGERS = [
    ("OBJ_leave_cell", "leave_cell", 400.0, 0.0),
    ("OBJ_reach_stairwell", "reach_stairwell", 5200.0, 0.0),
]

WORLD_BP_PATH = "/Game/Blueprints/World"
AI_BP_PATH = "/Game/Blueprints/AI"

# (label, spawn x, spawn y, [(patrol point label, x, y), ...], carries the pistol + keycard)
M01_GUARDS = [
    ("Guard_Corr1_A", 900.0, -60.0,
     [("PP_Corr1_A1", 600.0, -60.0), ("PP_Corr1_A2", 1500.0, -60.0)], True),
    ("Guard_Corr1_B", 1900.0, 60.0,
     [("PP_Corr1_B1", 2150.0, 60.0), ("PP_Corr1_B2", 1400.0, 60.0)], False),
    ("Guard_Hall_A", 3300.0, -60.0,
     [("PP_Hall_A1", 3100.0, -60.0), ("PP_Hall_A2", 3900.0, -60.0)], False),
    ("Guard_Hall_B", 4100.0, 60.0,
     [("PP_Hall_B1", 4400.0, 60.0), ("PP_Hall_B2", 3700.0, 60.0)], False),
    ("Guard_Hall_C", 4700.0, 0.0,
     [("PP_Hall_C1", 4750.0, -80.0), ("PP_Hall_C2", 4750.0, 80.0)], False),
]

# A spare pistol and keycard in the guard station, so the level stays finishable even if the
# first guard's body lands somewhere silly.
M01_PICKUPS = [
    ("Pickup_Pistol", "BP_Pickup_Pistol", 2700.0, -200.0, 40.0),
    ("Pickup_Keycard", "BP_Pickup_Keycard", 2700.0, 200.0, 40.0),
]

# Fills the 100-wide gap in the wall at x = 2900 between the station and corridor 2.
M01_DOOR = ("Door_Security", 2900.0, 0.0)

# Covers the whole playable slab with headroom. Guards cannot move without it.
NAV_VOLUME = ("NavMeshBounds", 2800.0, 0.0, 200.0, 6200.0, 1000.0, 1200.0)


def cube_mesh():
    return c.load_or_none(CUBE_PATH) or unreal.load_object(None, CUBE_PATH)


def ensure_greybox_materials():
    """Create (once) the two simple greybox materials and cache them for this run."""
    _GREYBOX_MATERIALS["greybox"] = c.ensure_constant_color_material(
        "M_Greybox", MATERIALS_PATH, (0.5, 0.5, 0.5), 0.9
    )
    _GREYBOX_MATERIALS["floor"] = c.ensure_constant_color_material(
        "M_Greybox_Floor", MATERIALS_PATH, (0.35, 0.35, 0.35), 0.9
    )
    return _GREYBOX_MATERIALS


def material_kind_for_label(label):
    for prefix, kind in MESH_LABEL_MATERIAL_KIND:
        if label.startswith(prefix):
            return kind
    return None


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

    kind = material_kind_for_label(label) or "greybox"
    material = _GREYBOX_MATERIALS.get(kind)
    if material is not None:
        c.assign_mesh_material(actor, material)
    return actor


def add_wall(mesh, label, cx, cy, sx, sy):
    return add_box(mesh, label, (cx, cy, WALL_HEIGHT / 2.0), (sx, sy, WALL_HEIGHT))


def configure_directional_light(actor):
    c.set_actor_mobility_movable(actor)
    try:
        light_component = actor.get_editor_property("directional_light_component")
        c.set_props(
            light_component,
            [("intensity", 5.0), ("atmosphere_sun_light", True)],
            "Sun",
        )
    except Exception as exc:  # noqa: BLE001
        c.log_error("configure_directional_light " + c.safe_name(actor), exc)


def configure_sky_light(actor):
    c.set_actor_mobility_movable(actor)
    try:
        light_component = actor.get_editor_property("light_component")
        c.set_props(
            light_component,
            [
                ("real_time_capture", True),
                ("source_type", unreal.SkyLightSourceType.SLS_CAPTURED_SCENE),
            ],
            "SkyLight",
        )
    except Exception as exc:  # noqa: BLE001
        c.log_error("configure_sky_light " + c.safe_name(actor), exc)


def configure_post_process_volume(actor):
    try:
        actor.set_editor_property("unbound", True)
        settings = actor.get_editor_property("settings")
        settings.set_editor_property("override_auto_exposure_min_brightness", True)
        settings.set_editor_property("override_auto_exposure_max_brightness", True)
        settings.set_editor_property("auto_exposure_min_brightness", 1.0)
        settings.set_editor_property("auto_exposure_max_brightness", 1.0)
        actor.set_editor_property("settings", settings)
    except Exception as exc:  # noqa: BLE001
        c.log_error("configure_post_process_volume " + c.safe_name(actor), exc)


def atmosphere_class():
    atmosphere = c.find_class("SkyAtmosphere", "/Script/Engine.SkyAtmosphere")
    if atmosphere is None:
        atmosphere = c.find_class("AtmosphericFog", "/Script/Engine.AtmosphericFog")
    return atmosphere


def add_lighting():
    sun = c.spawn_actor(
        unreal.DirectionalLight,
        unreal.Vector(0.0, 0.0, 2000.0),
        unreal.Rotator(0.0, -50.0, 0.0),
        label="Sun",
    )
    if sun is not None:
        configure_directional_light(sun)

    sky = c.spawn_actor(unreal.SkyLight, unreal.Vector(0.0, 0.0, 1000.0), label="SkyLight")
    if sky is not None:
        configure_sky_light(sky)

    c.spawn_actor(
        unreal.ExponentialHeightFog, unreal.Vector(0.0, 0.0, 0.0), label="HeightFog"
    )

    atmosphere = atmosphere_class()
    if atmosphere is not None:
        c.spawn_actor(atmosphere, unreal.Vector(0.0, 0.0, 0.0), label="SkyAtmosphere")
    else:
        unreal.log_warning("[Castle] neither SkyAtmosphere nor AtmosphericFog is available")

    pp = c.spawn_actor(unreal.PostProcessVolume, unreal.Vector(0.0, 0.0, 0.0), label="PP_Global")
    if pp is not None:
        configure_post_process_volume(pp)


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


LIGHT_ACTOR_CLASSES = (unreal.DirectionalLight, unreal.SkyLight, unreal.PointLight)


def fix_existing_lights():
    """Movable mobility + tuning for every light actor already in the open level."""
    changed = 0
    for actor in c.all_level_actors():
        if not isinstance(actor, LIGHT_ACTOR_CLASSES):
            continue
        try:
            was_movable = c.actor_mobility(actor) == unreal.ComponentMobility.MOVABLE
        except Exception:  # noqa: BLE001
            was_movable = False
        if isinstance(actor, unreal.DirectionalLight):
            configure_directional_light(actor)
        elif isinstance(actor, unreal.SkyLight):
            configure_sky_light(actor)
        else:
            c.set_actor_mobility_movable(actor)
        if not was_movable:
            changed += 1
            c.log("updated", actor.get_actor_label(), "mobility -> Movable")
    return changed


def ensure_scene_actor(actor_class, label, location):
    """Spawn one instance of ``actor_class`` if the level has none of that type yet."""
    if actor_class is None:
        return 0
    for actor in c.all_level_actors():
        if isinstance(actor, actor_class):
            return 0
    actor = c.spawn_actor(actor_class, location, label=label)
    if actor is None:
        return 0
    c.log("created", label, "was missing from an existing map")
    return 1


def fix_missing_scene_actors():
    """Add SkyAtmosphere / fog / post-process if an already-existing map lacks them."""
    added = 0
    added += ensure_scene_actor(atmosphere_class(), "SkyAtmosphere", unreal.Vector(0.0, 0.0, 0.0))
    added += ensure_scene_actor(
        unreal.ExponentialHeightFog, "HeightFog", unreal.Vector(0.0, 0.0, 0.0)
    )
    pp_before = [a for a in c.all_level_actors() if isinstance(a, unreal.PostProcessVolume)]
    added += ensure_scene_actor(
        unreal.PostProcessVolume, "PP_Global", unreal.Vector(0.0, 0.0, 0.0)
    )
    if not pp_before:
        for actor in c.all_level_actors():
            if isinstance(actor, unreal.PostProcessVolume):
                configure_post_process_volume(actor)
                break
    return added


def fix_existing_materials():
    """Assign the greybox materials to already-placed meshes still on the mesh default."""
    changed = 0
    for actor in c.all_level_actors():
        if not isinstance(actor, unreal.StaticMeshActor):
            continue
        label = actor.get_actor_label()
        kind = material_kind_for_label(label)
        if kind is None:
            continue
        try:
            component = actor.get_editor_property("static_mesh_component")
        except Exception as exc:  # noqa: BLE001
            c.log_error("fix_existing_materials " + label, exc)
            continue
        if c.has_material_override(component):
            continue
        material = _GREYBOX_MATERIALS.get(kind)
        if material is not None and c.assign_mesh_material(actor, material):
            changed += 1
            c.log("updated", label, "material -> " + material.get_name())
    return changed


def ensure_game_mode(package_path):
    """For a map that already exists: fix up lighting, materials and the GameMode override.

    The first run creates the maps before the Blueprints and greybox materials exist, so
    all of this has to be fixable on a later pass instead of only at creation time.
    """
    if not load_level(package_path):
        c.log("exists", package_path, "could not open level to check for fixups")
        return False

    changed = fix_existing_lights() + fix_missing_scene_actors() + fix_existing_materials()

    game_mode = c.load_generated_class(PLAYER_PATH, "BP_CastleGameMode")
    if game_mode is None:
        c.log("exists", package_path, "BP_CastleGameMode_C not found; override left unset")
    else:
        settings = c.world_settings()
        already_set = False
        if settings is not None:
            try:
                already_set = settings.get_editor_property("default_game_mode") == game_mode
            except Exception:  # noqa: BLE001
                pass
        if not already_set and c.set_level_game_mode(game_mode, package_path):
            changed += 1
            c.log("updated", package_path, "GameMode override = BP_CastleGameMode_C")

    if changed:
        save_level()
        c.log("updated", package_path, "{0} fixup(s) applied".format(changed))
        return True
    c.log("exists", package_path, "lighting, materials and GameMode override already correct")
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


def find_actor_by_label(label):
    for actor in c.all_level_actors():
        try:
            if actor.get_actor_label() == label:
                return actor
        except Exception:  # noqa: BLE001
            continue
    return None


def ensure_actor(actor_class, label, location, rotation=None):
    """Spawn an actor only when the open level has no actor with that label yet.

    Returns (actor, created). This is what makes a re-run safe on a map that already exists:
    missing actors are filled in and existing ones are left exactly as the designer left them.
    """
    existing = find_actor_by_label(label)
    if existing is not None:
        return existing, False
    if actor_class is None:
        c.log("skipped", label, "class not found")
        return None, False
    actor = c.spawn_actor(actor_class, location, rotation, label=label)
    if actor is None:
        c.log("FAILED", label, "spawn_actor returned None")
        return None, False
    return actor, True


def add_nav_volume():
    nav_class = c.find_class("NavMeshBoundsVolume", "/Script/NavigationSystem.NavMeshBoundsVolume")
    label, cx, cy, cz, sx, sy, sz = NAV_VOLUME
    nav, created = ensure_actor(nav_class, label, unreal.Vector(cx, cy, cz))
    if not created or nav is None:
        return 0
    # A brush volume is 200 units per side at scale 1.
    nav.set_actor_scale3d(unreal.Vector(sx / 200.0, sy / 200.0, sz / 200.0))
    c.log("created", label, "nav bounds {0}x{1}x{2}".format(int(sx), int(sy), int(sz)))
    return 1


def add_door():
    door_class = c.load_generated_class(WORLD_BP_PATH, "BP_Door_Keycard")
    label, dx, dy = M01_DOOR
    _door, created = ensure_actor(door_class, label, unreal.Vector(dx, dy, 0.0))
    if created:
        c.log("created", label, "locked on keycard 'cellblock'")

    # The door completes security_door now, so the old trigger volume would race it.
    stale = find_actor_by_label("OBJ_security_door")
    if stale is not None:
        try:
            stale.destroy_actor()
            c.log("updated", "OBJ_security_door", "removed; BP_Door_Keycard completes it now")
        except Exception as exc:  # noqa: BLE001
            c.log_error("destroy OBJ_security_door", exc)

    # find_weapon is completed by the pistol pickup for the same reason.
    stale_weapon = find_actor_by_label("OBJ_find_weapon")
    if stale_weapon is not None:
        try:
            stale_weapon.destroy_actor()
            c.log("updated", "OBJ_find_weapon", "removed; the pistol pickup completes it now")
        except Exception as exc:  # noqa: BLE001
            c.log_error("destroy OBJ_find_weapon", exc)

    return 1 if created else 0


def add_pickups():
    created = 0
    for label, bp_name, px, py, pz in M01_PICKUPS:
        pickup_class = c.load_generated_class(WORLD_BP_PATH, bp_name)
        _actor, was_created = ensure_actor(pickup_class, label, unreal.Vector(px, py, pz))
        if was_created:
            created += 1
            c.log("created", label, bp_name)
    return created


def add_guards():
    guard_class = c.load_generated_class(AI_BP_PATH, "BP_Guard")
    pistol_class = c.load_generated_class(WORLD_BP_PATH, "BP_Pickup_Pistol")
    keycard_class = c.load_generated_class(WORLD_BP_PATH, "BP_Pickup_Keycard")
    target_point_class = c.find_class("TargetPoint", "/Script/Engine.TargetPoint")

    created = 0
    for label, gx, gy, patrol, carries_loot in M01_GUARDS:
        points = []
        for point_label, ppx, ppy in patrol:
            point, point_created = ensure_actor(
                target_point_class, point_label, unreal.Vector(ppx, ppy, 20.0)
            )
            if point_created:
                created += 1
            if point is not None:
                points.append(point)

        guard, was_created = ensure_actor(guard_class, label, unreal.Vector(gx, gy, 100.0))
        if guard is None or not was_created:
            continue

        created += 1
        values = [("patrol_points", points)]
        if carries_loot and pistol_class is not None and keycard_class is not None:
            # Per instance, not on the class: only the first guard is worth killing quietly.
            values.append(("drop_on_death", [pistol_class, keycard_class]))
        c.set_props(guard, values, label)
        c.log(
            "created",
            label,
            "{0} patrol points{1}".format(
                len(points), ", drops pistol + keycard" if carries_loot else ""
            ),
        )
    return created


def ensure_m01_gameplay(package_path):
    """Open L_M01 and fill in whatever gameplay actor it is missing, then save if anything changed."""
    if not load_level(package_path):
        c.log("skipped", package_path, "could not open the level to add gameplay actors")
        return False

    created = add_nav_volume() + add_door() + add_pickups() + add_guards()
    if created:
        save_level()
        c.log("updated", package_path, "{0} gameplay actor(s) added".format(created))
    else:
        c.log("exists", package_path, "every gameplay actor already placed")
    return bool(created)


def run():
    c.ensure_directory(MAPS_PATH)
    ensure_greybox_materials()
    build_sandbox()
    build_cell_block_d()
    ensure_m01_gameplay(c.asset_path(MAPS_PATH, "L_M01_CellBlockD"))


if __name__ == "__main__":
    run()
    c.print_summary("maps")

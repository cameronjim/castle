"""Create the narrative plumbing's data: the phone's texts, the dialogue sequences, the placeholder
flashback and the small playable scene it ends in (claude-docs/gameplay-semantics.md).

    /Game/Data/DT_Messages                          FHawkeyePhoneMessage rows: four placeholder texts
                                                    tied to DA_CH01_Rooftops' objectives
    /Game/Data/DT_DialogueSequences                 FHawkeyeDialogueSequenceRow rows: seq_ch01_open,
                                                    three placeholder lines (DT_Dialogue's
                                                    seq_ch01_open_0N rows, made by create_partner.py)
    /Game/Blueprints/Player/BP_GameMode_ScenePlaceholder  child of BP_HawkeyeGameMode starting
                                                    DA_Scene_Placeholder (create_mission_data.py)
    /Game/Maps/L_Scene_Placeholder                  a 20 x 20 m greybox room: floor, four walls, a
                                                    ceiling, two lights, a PlayerStart at one end and
                                                    Scene_Obj_reach_marker (AObjectiveTriggerVolume)
                                                    under a purple marker at the other
    /Game/Flashbacks/Definitions/DA_FB00_Placeholder three solid-colour slides captioned "[Slide N]",
                                                    PlayableScene = L_Scene_Placeholder, back to
                                                    City_SceneReturn_FB00 (generate_city.py places it)

Then DA_CH01_Rooftops.FlashbackToPlay = DA_FB00_Placeholder.

Every string here is a bracketed placeholder: the story is written separately and dropped into these
rows. The tables and the flashback carry a build tag and are rewritten only when their rows change;
the map is saved only when an actor changed. Runs after create_mission_data and create_partner.
"""

import hashlib
import json
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _common as c  # noqa: E402
import _materials as m  # noqa: E402

DATA_PATH = "/Game/Data"
PLAYER_PATH = "/Game/Blueprints/Player"
MISSION_PATH = "/Game/Missions"
FLASHBACK_PATH = "/Game/Flashbacks/Definitions"
SCENE_MAP = "/Game/Maps/L_Scene_Placeholder"
SCENE_GAME_MODE = "BP_GameMode_ScenePlaceholder"
SCENE_MISSION = MISSION_PATH + "/DA_Scene_Placeholder"
CH01_MISSION = MISSION_PATH + "/DA_CH01_Rooftops"
FLASHBACK_NAME = "DA_FB00_Placeholder"
RETURN_POINT = "City_SceneReturn_FB00"
BUILD_TAG = "HawkeyeBuild"
CH01_ID = "DA_CH01_Rooftops"

# DT_Messages: (row, sender, text, trigger, objective or event, delay seconds, arrives read)
MESSAGES = [
    ("msg_ch01_01", "[Grills]", "[Grills text 1]", "ObjectiveStarted", "reach_roof", 6.0, False),
    ("msg_ch01_02", "[Grills]", "[Grills text 2]", "ObjectiveCompleted", "reach_roof", 2.0, False),
    ("msg_ch01_03", "[Contact 2]", "[Contact 2 text 1]", "ObjectiveCompleted", "clear_roof", 4.0, False),
    ("msg_ch01_04", "[Contact 2]", "[Contact 2 text 2]", "ObjectiveStarted", "find_arrow", 8.0, False),
]

# DT_DialogueSequences: (row, sequence, order, DT_Dialogue line, gap seconds). The first gap lets
# the 4.5 s title card go first.
SEQUENCE_STEPS = [
    ("seq_ch01_open_01", "seq_ch01_open", 1, "seq_ch01_open_01", 5.0),
    ("seq_ch01_open_02", "seq_ch01_open", 2, "seq_ch01_open_02", 1.5),
    ("seq_ch01_open_03", "seq_ch01_open", 3, "seq_ch01_open_03", 1.5),
]

# DA_FB00_Placeholder's slides: (caption, tint). Solid colours until there are images.
SLIDES = [
    ("[Slide 1]", (0.16, 0.05, 0.28)),
    ("[Slide 2]", (0.03, 0.12, 0.16)),
    ("[Slide 3]", (0.22, 0.05, 0.05)),
]
SLIDE_HOLD = 3.0
SLIDE_CROSSFADE = 0.75

# The room, cm. Origin at the floor's centre; the player starts at -X, the marker is at +X.
ROOM = 2000.0
WALL_HEIGHT = 400.0
WALL_THICK = 20.0
CUBE = "/Engine/BasicShapes/Cube"
CYLINDER = "/Engine/BasicShapes/Cylinder"
MI_BEACON = m.MATERIALS_PATH + "/MI_ObjectiveBeacon"
START = (-700.0, 0.0, 100.0)
MARKER = (600.0, 0.0)
OBJECTIVE_EXTENT = (120.0, 120.0, 150.0)
LIGHT_LUMENS = 4000.0
LIGHT_RADIUS = 1800.0


def _hash(rows):
    return hashlib.md5(json.dumps(rows, sort_keys=True).encode("utf-8")).hexdigest()[:12]


# --------------------------------------------------------------------------------------
# tables
# --------------------------------------------------------------------------------------

def ensure_table(name, struct_name, rows):
    """A data table of struct_name filled from rows (dicts with "Name"), rewritten when rows change."""
    full = c.asset_path(DATA_PATH, name)
    row_struct = getattr(unreal, struct_name, None)
    if row_struct is None:
        c.log("FAILED", full, "F{0} not exposed; build the module".format(struct_name))
        return None
    factory = c.new_factory("DataTableFactory")
    if factory is not None:
        c.set_props(factory, [("struct", row_struct.static_struct())], "DataTableFactory")
    table, created = c.create_asset(name, DATA_PATH, unreal.DataTable, factory, quiet=True)
    if table is None:
        return None
    build = "rows-" + _hash(rows)
    if not created and unreal.EditorAssetLibrary.get_metadata_tag(table, BUILD_TAG) == build:
        c.log("exists", full, "{0} rows".format(len(rows)))
        return table
    if not unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(rows, indent=1)):
        c.log("FAILED", full, "fill_data_table_from_json_string refused the rows")
        return table
    unreal.EditorAssetLibrary.set_metadata_tag(table, BUILD_TAG, build)
    c.save(table)
    c.log("created" if created else "updated", full, "{0} rows".format(len(rows)))
    return table


def message_rows():
    rows = []
    for name, sender, text, trigger, key, delay, read in MESSAGES:
        rows.append({
            "Name": name, "Sender": sender, "Text": text, "Trigger": trigger,
            "TriggerObjectiveId": key if trigger != "Event" else "None",
            "TriggerEvent": key if trigger == "Event" else "None",
            "ChapterId": CH01_ID, "DelaySeconds": delay, "bRead": read,
        })
    return rows


def sequence_rows():
    return [{"Name": name, "Sequence": seq, "Order": order, "Line": line, "GapSeconds": gap}
            for name, seq, order, line, gap in SEQUENCE_STEPS]


# --------------------------------------------------------------------------------------
# the placeholder scene
# --------------------------------------------------------------------------------------

def ensure_scene_game_mode():
    """BP_GameMode_ScenePlaceholder starting DA_Scene_Placeholder, no navigation build."""
    parent = c.load_generated_class(PLAYER_PATH, "BP_HawkeyeGameMode")
    mission = c.load_or_none(SCENE_MISSION)
    full = c.asset_path(PLAYER_PATH, SCENE_GAME_MODE)
    if parent is None or mission is None:
        c.log("skipped", full, "run create_blueprints and create_mission_data first")
        return None
    bp = c.load_or_none(full)
    changed = []
    if bp is None:
        factory = c.new_factory("BlueprintFactory")
        c.set_props(factory, [("parent_class", parent)], SCENE_GAME_MODE + " factory")
        bp, _created = c.create_asset(SCENE_GAME_MODE, PLAYER_PATH, unreal.Blueprint, factory, quiet=True)
        if bp is None:
            return None
        changed.append("created")
        c.compile_blueprint(bp)
    cdo = c.blueprint_cdo(bp)
    if cdo is not None and cdo.get_editor_property("starting_mission") != mission:
        c.set_props(cdo, [("starting_mission", mission)], SCENE_GAME_MODE)
        changed.append("starting_mission")
    # A closed room with nothing that walks: no navmesh to build.
    if cdo is not None and cdo.get_editor_property("build_navigation_at_start"):
        c.set_props(cdo, [("build_navigation_at_start", False)], SCENE_GAME_MODE)
        changed.append("no navigation")
    if changed:
        c.compile_blueprint(bp)
        c.save(bp)
        c.log("updated" if "created" not in changed else "created", full, ", ".join(changed))
    else:
        c.log("exists", full)
    return c.load_generated_class(PLAYER_PATH, SCENE_GAME_MODE)


def open_or_create_scene_map():
    subsystem = c.level_editor_subsystem()
    if c.exists(SCENE_MAP):
        return bool(subsystem.load_level(SCENE_MAP)), False
    c.ensure_directory("/Game/Maps")
    try:
        ok = subsystem.new_level(SCENE_MAP, False)
    except TypeError:
        ok = subsystem.new_level(SCENE_MAP)
    return bool(ok), bool(ok)


def _same(a, b, tol=0.5):
    return abs(a.x - b.x) <= tol and abs(a.y - b.y) <= tol and abs(a.z - b.z) <= tol


def _ensure_actor(existing, label, cls, loc, rot=None):
    """(actor, changes): the actor labelled label, of cls, at loc."""
    rot = rot or unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0)
    changes = 0
    actor = existing.get(label)
    if actor is not None and not isinstance(actor, cls):
        actor.destroy_actor()
        actor = None
    if actor is None:
        actor = c.spawn_actor(cls, loc, rot, label=label)
        if actor is None:
            c.log("FAILED", label, "spawn_actor returned None")
            return None, 0
        existing[label] = actor
        changes += 1
    if not _same(actor.get_actor_location(), loc):
        actor.set_actor_location(loc, False, True)
        changes += 1
    if abs(((actor.get_actor_rotation().yaw - rot.yaw) + 180.0) % 360.0 - 180.0) > 0.1:
        actor.set_actor_rotation(rot, False)
        changes += 1
    return actor, changes


def _ensure_box(existing, label, mesh, material, centre, size):
    """A StaticMeshActor of the engine cube, size cm, centred on centre."""
    actor, n = _ensure_actor(existing, label, unreal.StaticMeshActor, unreal.Vector(*centre))
    if actor is None:
        return 0
    comp = actor.get_editor_property("static_mesh_component")
    if comp.get_editor_property("static_mesh") != mesh:
        comp.set_static_mesh(mesh)
        n += 1
    scale = unreal.Vector(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0)
    if not _same(actor.get_actor_scale3d(), scale, 1e-4):
        actor.set_actor_scale3d(scale)
        n += 1
    overrides = comp.get_editor_property("override_materials")
    if material is not None and (len(overrides) < 1 or overrides[0] != material):
        comp.set_material(0, material)
        n += 1
    return n


def _ensure_light(existing, label, loc):
    actor, n = _ensure_actor(existing, label, unreal.PointLight, unreal.Vector(*loc))
    if actor is None:
        return 0
    comp = actor.get_editor_property("point_light_component")
    for prop, value in (("mobility", unreal.ComponentMobility.MOVABLE), ("intensity_units", unreal.LightUnits.LUMENS),
                        ("intensity", LIGHT_LUMENS), ("attenuation_radius", LIGHT_RADIUS)):
        current = comp.get_editor_property(prop)
        if (abs(current - value) > 0.5) if isinstance(value, float) else current != value:
            comp.set_editor_property(prop, value)
            n += 1
    return n


def build_room(existing):
    cube = c.load_or_none(CUBE)
    cylinder = c.load_or_none(CYLINDER)
    wall_mat = c.load_or_none(m.M_CONCRETE)
    floor_mat = c.load_or_none(m.M_CONCRETE_FLOOR) or wall_mat
    beacon = c.load_or_none(MI_BEACON)
    half, h, t = ROOM * 0.5, WALL_HEIGHT, WALL_THICK
    n = _ensure_box(existing, "Scene_Floor", cube, floor_mat, (0.0, 0.0, -t * 0.5), (ROOM, ROOM, t))
    n += _ensure_box(existing, "Scene_Ceiling", cube, wall_mat, (0.0, 0.0, h + t * 0.5), (ROOM, ROOM, t))
    walls = [("Scene_Wall_N", (0.0, half + t * 0.5, h * 0.5), (ROOM + 2 * t, t, h)),
             ("Scene_Wall_S", (0.0, -half - t * 0.5, h * 0.5), (ROOM + 2 * t, t, h)),
             ("Scene_Wall_E", (half + t * 0.5, 0.0, h * 0.5), (t, ROOM, h)),
             ("Scene_Wall_W", (-half - t * 0.5, 0.0, h * 0.5), (t, ROOM, h))]
    for label, centre, size in walls:
        n += _ensure_box(existing, label, cube, wall_mat, centre, size)
    n += _ensure_light(existing, "Scene_Light_0", (-450.0, 0.0, h - 60.0))
    n += _ensure_light(existing, "Scene_Light_1", (450.0, 0.0, h - 60.0))

    start, k = _ensure_actor(existing, "Scene_PlayerStart", unreal.PlayerStart, unreal.Vector(*START))
    n += k
    volume_class = c.find_class("ObjectiveTriggerVolume", "/Script/Hawkeye.ObjectiveTriggerVolume")
    if volume_class is None:
        c.log("FAILED", "Scene_Obj_reach_marker", "AObjectiveTriggerVolume not exposed; build the module")
        return n
    volume, k = _ensure_actor(existing, "Scene_Obj_reach_marker", volume_class,
                              unreal.Vector(MARKER[0], MARKER[1], OBJECTIVE_EXTENT[2]))
    n += k
    if volume is not None:
        if str(volume.get_editor_property("objective_id")) != "reach_marker":
            volume.set_editor_property("objective_id", unreal.Name("reach_marker"))
            n += 1
        box = volume.get_editor_property("collision_component")
        extent = unreal.Vector(*OBJECTIVE_EXTENT)
        if box is not None and not _same(box.get_editor_property("box_extent"), extent, 1.0):
            box.set_editor_property("box_extent", extent)
            n += 1
    # A purple post over the objective so the room has something to walk to.
    n += _ensure_box(existing, "Scene_Marker", cylinder, beacon, (MARKER[0], MARKER[1], 60.0), (30.0, 30.0, 120.0))
    marker = existing.get("Scene_Marker")
    if marker is not None:
        comp = marker.get_editor_property("static_mesh_component")
        if str(comp.get_collision_profile_name()) != "NoCollision":
            comp.set_collision_profile_name("NoCollision")
            n += 1
    return n


def ensure_scene_map():
    game_mode = ensure_scene_game_mode()
    ok, created = open_or_create_scene_map()
    if not ok:
        c.log("FAILED", SCENE_MAP, "could not open or create the map")
        return None
    existing = {}
    for actor in c.all_level_actors():
        try:
            existing[actor.get_actor_label()] = actor
        except Exception:  # noqa: BLE001
            continue
    changes = build_room(existing)
    ws = c.world_settings()
    if ws is not None and game_mode is not None and ws.get_editor_property("default_game_mode") != game_mode:
        ws.set_editor_property("default_game_mode", game_mode)
        changes += 1
    if created or changes:
        c.level_editor_subsystem().save_current_level()
        c.log("created" if created else "updated", SCENE_MAP, "{0} change(s)".format(changes))
    else:
        c.log("exists", SCENE_MAP, "nothing changed; level not saved")
    return c.load_or_none(SCENE_MAP)


# --------------------------------------------------------------------------------------
# the placeholder flashback, and chapter 1 pointing at it
# --------------------------------------------------------------------------------------

def _path_of(value):
    if value is None:
        return ""
    for getter in ("get_path_name", "export_text"):
        method = getattr(value, getter, None)
        if method is not None:
            try:
                return str(method()).split(".")[0]
            except Exception:  # noqa: BLE001
                pass
    return str(value).split(".")[0]


def ensure_flashback(scene_world):
    full = c.asset_path(FLASHBACK_PATH, FLASHBACK_NAME)
    cls = c.find_class("FlashbackDefinition", "/Script/Hawkeye.FlashbackDefinition")
    if cls is None:
        c.log("FAILED", full, "UFlashbackDefinition not exposed; build the module")
        return None
    factory = c.new_factory("DataAssetFactory")
    c.set_props(factory, [("data_asset_class", cls)], "DataAssetFactory")
    asset, created = c.create_asset(FLASHBACK_NAME, FLASHBACK_PATH, cls, factory, quiet=True)
    if asset is None:
        return None
    build = "fb-" + _hash([SLIDES, SLIDE_HOLD, SLIDE_CROSSFADE, RETURN_POINT, _path_of(scene_world)])
    current_scene = _path_of(asset.get_editor_property("playable_scene"))
    if not created and unreal.EditorAssetLibrary.get_metadata_tag(asset, BUILD_TAG) == build \
            and current_scene == _path_of(scene_world):
        c.log("exists", full, "{0} slides, scene {1}".format(len(SLIDES), current_scene))
        return asset
    slides = []
    for caption, rgb in SLIDES:
        slide = unreal.FlashbackSlide()
        slide.set_editor_property("caption", caption)
        slide.set_editor_property("hold_seconds", SLIDE_HOLD)
        slide.set_editor_property("crossfade_seconds", SLIDE_CROSSFADE)
        slide.set_editor_property("tint", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
        slides.append(slide)
    c.set_props(asset, [("title", "[FB00 title]"), ("slides", slides), ("skippable", True),
                        ("return_point_label", unreal.Name(RETURN_POINT))], FLASHBACK_NAME)
    if scene_world is not None:
        c.set_props(asset, [("playable_scene", scene_world)], FLASHBACK_NAME)
    unreal.EditorAssetLibrary.set_metadata_tag(asset, BUILD_TAG, build)
    c.save(asset)
    c.log("created" if created else "updated", full, "{0} slides, scene {1}".format(len(SLIDES), _path_of(scene_world)))
    return asset


def point_chapter_one_at(flashback):
    mission = c.load_or_none(CH01_MISSION)
    if mission is None or flashback is None:
        c.log("skipped", CH01_MISSION, "run create_mission_data first")
        return
    if _path_of(mission.get_editor_property("flashback_to_play")) == _path_of(flashback):
        c.log("exists", CH01_MISSION + ".FlashbackToPlay", FLASHBACK_NAME)
        return
    c.set_props(mission, [("flashback_to_play", flashback)], "DA_CH01_Rooftops")
    c.save(mission)
    c.log("updated", CH01_MISSION + ".FlashbackToPlay", FLASHBACK_NAME)


def run():
    c.ensure_directory(DATA_PATH)
    c.ensure_directory(FLASHBACK_PATH)
    ensure_table("DT_Messages", "HawkeyePhoneMessage", message_rows())
    ensure_table("DT_DialogueSequences", "HawkeyeDialogueSequenceRow", sequence_rows())
    scene = ensure_scene_map()
    flashback = ensure_flashback(scene)
    point_chapter_one_at(flashback)
    return flashback


if __name__ == "__main__":
    run()
    c.print_summary("narrative")

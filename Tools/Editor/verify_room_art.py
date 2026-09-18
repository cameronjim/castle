"""Check the L_M01_CellBlockD room-art pass landed. Read-only, prints PASS or FAIL.

Counterpart to create_room_art.py. It does not touch verify_content.py, which covers the
gameplay content; this one only looks at the art: the procedural materials, the ``Art_``
actors, the concrete on the cell and corridor-1 walls, and the dead daylight.

    UnrealEditor-Cmd.exe Castle.uproject -run=pythonscript ^
        -script="Tools\\Editor\\verify_room_art.py" -unattended -nullrhi -nosplash -nop4 -stdout
"""

import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _common as c  # noqa: E402
import _materials as m  # noqa: E402
import create_room_art as art  # noqa: E402

EXPECTED_MATERIALS = (
    m.M_CONCRETE,
    m.M_CONCRETE_FLOOR,
    m.M_STEEL_PAINTED,
    m.M_EMISSIVE,
    m.M_FLUORESCENT_FLICKER,
    m.MI_FLUORESCENT_TUBE,
    m.MI_RED_EMERGENCY,
    m.MI_KEYCARD_STRIPE,
    m.M_PISTOL,
    m.M_KEYCARD_BODY,
)

# Art actors that have to be there for the room to read as a prison at all.
EXPECTED_ACTORS = (
    "Art_Ceiling_Cell",
    "Art_Ceiling_Corr1",
    "Art_Ceiling_Station",
    "Art_Ceiling_Corr2",
    "Art_Ceiling_Exit",
    "Art_Floor_Cell",
    "Art_Floor_Corr1",
    "Art_Tube_Cell",
    "Art_Tube_Corr1_A",
    "Art_Tube_Corr1_B",
    "Art_Light_Cell",
    "Art_Light_Corr1_A",
    "Art_Light_Corr1_B",
    "Art_RedEmergency",
    "Art_Light_RedEmergency",
    "Art_ExitSign_Station",
    "Art_Bunk",
    "Art_Toilet",
    "Art_CellDoor_Jamb_S",
    "Art_CellDoor_Jamb_N",
    "Art_CellDoor_Lintel",
    "Art_CellDoor_Slab",
    "Art_Pipe_A",
    "Art_Pipe_B",
    "Art_Pipe_C",
    "Art_Panel_A",
    "Art_Panel_B",
    "Art_Camera_Body",
)

# Gameplay actors the art pass must not have disturbed.
GAMEPLAY_LABELS = (
    "PlayerStart",
    "OBJ_leave_cell",
    "OBJ_reach_stairwell",
    "Guard_Corr1_A",
    "Guard_Corr1_B",
    "Door_Security",
    "NavMeshBounds",
)

MAX_SUN_INTENSITY = 0.5
PROBLEMS = []


def say(line):
    unreal.log("[VerifyArt] " + line)


def fail(line):
    PROBLEMS.append(line)
    unreal.log_warning("[VerifyArt] FAIL " + line)


def material_name(actor):
    try:
        component = actor.get_editor_property("static_mesh_component")
        overrides = list(component.get_editor_property("override_materials") or [])
        if not overrides or overrides[0] is None:
            return None
        return overrides[0].get_name()
    except Exception:  # noqa: BLE001
        return None


def check_materials():
    say("---- materials ----")
    for path in EXPECTED_MATERIALS:
        if c.exists(path):
            say("  ok      " + path)
        else:
            fail("missing material " + path)


def check_actors(by_label):
    say("---- art actors ----")
    art_actors = [label for label in by_label if label.startswith(art.ART_PREFIX)]
    say("  {0} actor(s) with the {1} prefix".format(len(art_actors), art.ART_PREFIX))
    for label in EXPECTED_ACTORS:
        if label in by_label:
            say("  ok      " + label)
        else:
            fail("missing art actor " + label)


def check_gameplay_intact(by_label):
    say("---- gameplay actors still present ----")
    for label in GAMEPLAY_LABELS:
        if label in by_label:
            say("  ok      " + label)
        else:
            fail("gameplay actor {0} is gone".format(label))

    points = [a for a in by_label.values() if isinstance(a, unreal.TargetPoint)]
    say("  {0} patrol TargetPoint(s)".format(len(points)))
    if len(points) < 10:
        fail("only {0} TargetPoints left, expected at least 10".format(len(points)))


def check_wall_materials(by_label):
    say("---- cell and corridor 1 surfaces ----")
    for label in art.ART_WALL_LABELS:
        actor = by_label.get(label)
        if actor is None:
            fail("wall actor {0} is missing".format(label))
            continue
        name = material_name(actor)
        say("  {0:<22} {1}".format(label, name or "<mesh default>"))
        if name != "M_Concrete":
            fail("{0} is on {1}, expected M_Concrete".format(label, name))

    for label in ("Art_Floor_Cell", "Art_Floor_Corr1"):
        actor = by_label.get(label)
        if actor is None:
            continue
        name = material_name(actor)
        say("  {0:<22} {1}".format(label, name or "<mesh default>"))
        if name != "M_ConcreteFloor":
            fail("{0} is on {1}, expected M_ConcreteFloor".format(label, name))


def check_lighting(actors):
    say("---- lighting ----")
    suns = [a for a in actors if isinstance(a, unreal.DirectionalLight)]
    if not suns:
        say("  no DirectionalLight in the level (fine - the map is indoors)")
    for sun in suns:
        hidden = False
        intensity = None
        try:
            hidden = bool(sun.get_editor_property("hidden"))
        except Exception:  # noqa: BLE001
            hidden = False
        try:
            component = sun.get_editor_property("directional_light_component")
            intensity = component.get_editor_property("intensity")
        except Exception:  # noqa: BLE001
            intensity = None
        say("  {0:<22} hidden={1} intensity={2}".format(
            art.label_of(sun), hidden, intensity))
        if not hidden and (intensity is None or intensity > MAX_SUN_INTENSITY):
            fail("{0} still floods the interior (intensity {1})".format(
                art.label_of(sun), intensity))

    for sky in [a for a in actors if isinstance(a, unreal.SkyLight)]:
        try:
            intensity = sky.get_editor_property("light_component").get_editor_property(
                "intensity")
        except Exception:  # noqa: BLE001
            intensity = None
        say("  {0:<22} intensity={1}".format(art.label_of(sky), intensity))
        if intensity is not None and intensity > 0.5:
            fail("{0} sky light is {1}, expected 0.15".format(art.label_of(sky), intensity))

    rect_class = c.find_class("RectLight", "/Script/Engine.RectLight")
    rects = [a for a in actors if rect_class is not None and isinstance(a, rect_class)]
    points = [a for a in actors if isinstance(a, unreal.PointLight)]
    say("  rect lights: {0}, point lights: {1}".format(len(rects), len(points)))
    if len(rects) < 3:
        fail("{0} rect light(s), expected 3 fluorescents".format(len(rects)))
    if not points:
        fail("no PointLight; the red emergency lamp is missing")


def main():
    say("==== verifying the L_M01_CellBlockD room art ====")
    check_materials()

    if not art.load_level(art.MAP_PATH):
        fail("could not open " + art.MAP_PATH)
        actors = []
    else:
        actors = c.all_level_actors()

    by_label = {}
    for actor in actors:
        by_label[art.label_of(actor)] = actor

    check_actors(by_label)
    check_gameplay_intact(by_label)
    check_wall_materials(by_label)
    check_lighting(actors)

    if PROBLEMS:
        unreal.log_error("[VerifyArt] FAIL: {0} problem(s)".format(len(PROBLEMS)))
        for problem in PROBLEMS:
            unreal.log_error("[VerifyArt]   " + problem)
    else:
        say("==== PASS: the room art is in place and the level is still playable ====")


main()

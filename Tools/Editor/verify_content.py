"""Load every asset create_all.py is supposed to produce and print its key properties.

Read-only: nothing is created, changed or saved. Exits by printing a PASS/FAIL line so the
output can be eyeballed or grepped after a content run.

    UnrealEditor-Cmd.exe Castle.uproject -run=pythonscript ^
        -script="Tools\\Editor\\verify_content.py" -unattended -nullrhi -nosplash -nop4 -stdout
"""

import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _common as c  # noqa: E402

INPUT_PATH = "/Game/Input"
PLAYER_PATH = "/Game/Blueprints/Player"
UI_PATH = "/Game/Blueprints/UI"
WORLD_PATH = "/Game/Blueprints/World"
AI_PATH = "/Game/Blueprints/AI"
IMAGE_PATH = "/Game/Flashbacks/Images"
PISTOL_PATH = "/Game/Weapons/Pistol"
GUARD_MATERIAL_PATH = "/Game/Characters/Guard"

IA_NAMES = [
    "IA_Move", "IA_Look", "IA_Jump", "IA_Sprint", "IA_Crouch", "IA_Fire",
    "IA_Aim", "IA_Reload", "IA_Takedown", "IA_Interact", "IA_Pause", "IA_Skip",
]

CHARACTER_INPUT_PROPS = [
    "default_mapping_context", "move_action", "look_action", "jump_action", "sprint_action",
    "crouch_action", "fire_action", "aim_action", "reload_action", "takedown_action",
    "interact_action",
]

# Pause is bound on the controller so it survives the pawn being locked out or dead.
CONTROLLER_PROPS = [
    "flashback_widget_class", "hud_widget_class", "pause_widget_class", "pause_action",
    "pause_mapping_context", "end_card_widget_class",
]

EXPECTED = (
    [c.asset_path(INPUT_PATH, n) for n in IA_NAMES]
    + [
        c.asset_path(INPUT_PATH, "IMC_Default"),
        c.asset_path(PLAYER_PATH, "BP_CastleCharacter"),
        c.asset_path(PLAYER_PATH, "BP_CastlePlayerController"),
        c.asset_path(PLAYER_PATH, "BP_CastleGameMode"),
        c.asset_path(UI_PATH, "WBP_Flashback"),
        c.asset_path(UI_PATH, "WBP_Hud"),
        c.asset_path(UI_PATH, "WBP_Pause"),
        c.asset_path(UI_PATH, "WBP_EndCard"),
        c.asset_path(WORLD_PATH, "BP_Pickup_Pistol"),
        c.asset_path(WORLD_PATH, "BP_Pickup_Keycard"),
        c.asset_path(WORLD_PATH, "BP_Door_Keycard"),
        c.asset_path(AI_PATH, "BP_Guard"),
    ]
    + [c.asset_path(IMAGE_PATH, "T_FB01_0{0}".format(i)) for i in range(1, 7)]
    + [
        # First-person weapon art, copied out of the engine's template resources.
        c.asset_path(PISTOL_PATH + "/Meshes", "SM_Pistol"),
        c.asset_path(PISTOL_PATH + "/Materials", "MI_Weapon_Pistol"),
        "/Game/Weapons/Rifle/Materials/M_Weapon",
        c.asset_path(GUARD_MATERIAL_PATH, "M_GuardBody"),
        c.asset_path(GUARD_MATERIAL_PATH, "M_GuardVisor"),
    ]
    + [
        "/Game/Missions/DA_M01_CellBlockD",
        "/Game/Flashbacks/Definitions/DA_FB01_Sunday",
        "/Game/Maps/L_Sandbox",
        "/Game/Maps/L_M01_CellBlockD",
    ]
)

PROBLEMS = []


def say(line):
    unreal.log("[Verify] " + line)


def fail(line):
    PROBLEMS.append(line)
    unreal.log_warning("[Verify] MISSING " + line)


def prop(obj, name):
    try:
        return obj.get_editor_property(name)
    except Exception:  # noqa: BLE001
        return None


def name_of(value):
    if value is None:
        return "None"
    try:
        return value.get_name()
    except Exception:  # noqa: BLE001
        return c.class_name(value)


def check_existence():
    say("---- asset existence ----")
    missing = [p for p in EXPECTED if not c.exists(p)]
    for p in missing:
        fail(p)
    say("{0}/{1} expected assets present".format(len(EXPECTED) - len(missing), len(EXPECTED)))


def check_input():
    say("---- input ----")
    for name in IA_NAMES:
        action = c.load_or_none(c.asset_path(INPUT_PATH, name))
        if action is None:
            continue
        say("  {0:<12} value_type={1}".format(name, prop(action, "value_type")))

    imc = c.load_or_none(c.asset_path(INPUT_PATH, "IMC_Default"))
    if imc is None:
        fail("IMC_Default could not be loaded")
        return
    container = prop(imc, "default_key_mappings")
    mappings = list(prop(container, "mappings") or []) if container else []
    say("  IMC_Default: {0} mappings".format(len(mappings)))
    for mapping in mappings:
        action = prop(mapping, "action")
        key = prop(mapping, "key")
        key_name = prop(key, "key_name") if key is not None else "?"
        modifiers = list(prop(mapping, "modifiers") or [])
        mod_names = ", ".join(c.class_name(type(m)) for m in modifiers) or "-"
        say(
            "    {0:<18} {1:<12} modifiers: {2}".format(
                name_of(action), str(key_name), mod_names
            )
        )


def check_blueprints():
    say("---- blueprints ----")
    char_class = c.load_generated_class(PLAYER_PATH, "BP_CastleCharacter")
    if char_class is None:
        fail("BP_CastleCharacter_C")
    else:
        cdo = unreal.get_default_object(char_class)
        for name in CHARACTER_INPUT_PROPS:
            value = prop(cdo, name)
            say("  BP_CastleCharacter.{0:<24} = {1}".format(name, name_of(value)))
            if value is None:
                fail("BP_CastleCharacter." + name + " is unset")

    gm_class = c.load_generated_class(PLAYER_PATH, "BP_CastleGameMode")
    if gm_class is None:
        fail("BP_CastleGameMode_C")
    else:
        cdo = unreal.get_default_object(gm_class)
        for name in ("default_pawn_class", "player_controller_class", "starting_mission"):
            say("  BP_CastleGameMode.{0:<24} = {1}".format(name, name_of(prop(cdo, name))))

    pc_class = c.load_generated_class(PLAYER_PATH, "BP_CastlePlayerController")
    if pc_class is None:
        fail("BP_CastlePlayerController_C")
    else:
        cdo = unreal.get_default_object(pc_class)
        for name in CONTROLLER_PROPS:
            value = prop(cdo, name)
            say("  BP_CastlePlayerController.{0:<24} = {1}".format(name, name_of(value)))
            if value is None:
                fail("BP_CastlePlayerController." + name + " is unset")


def value_text(value):
    """Comparable text for a property value: 'WEAPON' for an enum, str() for everything else."""
    name = getattr(value, "name", None)
    if name is not None:
        return str(name)
    return str(value)


def check_world_blueprints():
    say("---- world blueprints ----")

    for name, expected in (
        ("BP_Pickup_Pistol", [("pickup_type", "WEAPON"), ("magazine_amount", 12), ("ammo_amount", 24)]),
        ("BP_Pickup_Keycard", [("pickup_type", "KEYCARD"), ("keycard_id", "cellblock")]),
    ):
        cls = c.load_generated_class(WORLD_PATH, name)
        if cls is None:
            fail(name + "_C")
            continue
        cdo = unreal.get_default_object(cls)
        for field, want in expected:
            got = prop(cdo, field)
            say("  {0}.{1:<20} = {2}".format(name, field, got))
            if value_text(got) != str(want):
                fail("{0}.{1} is {2}, expected {3}".format(name, field, got, want))

    door_class = c.load_generated_class(WORLD_PATH, "BP_Door_Keycard")
    if door_class is None:
        fail("BP_Door_Keycard_C")
    else:
        cdo = unreal.get_default_object(door_class)
        for field, want in (
            ("locked", True),
            ("required_keycard_id", "cellblock"),
            ("completes_objective_id", "security_door"),
        ):
            got = prop(cdo, field)
            say("  BP_Door_Keycard.{0:<22} = {1}".format(field, got))
            if value_text(got) != str(want):
                fail("BP_Door_Keycard.{0} is {1}, expected {2}".format(field, got, want))

    guard_class = c.load_generated_class(AI_PATH, "BP_Guard")
    if guard_class is None:
        fail("BP_Guard_C")
    else:
        cdo = unreal.get_default_object(guard_class)
        controller = prop(cdo, "ai_controller_class")
        say("  BP_Guard.ai_controller_class      = {0}".format(name_of(controller)))
        if controller is None or "GuardAIController" not in c.class_name(controller):
            fail("BP_Guard.ai_controller_class is not AGuardAIController")
        say("  BP_Guard.auto_possess_ai          = {0}".format(prop(cdo, "auto_possess_ai")))

    for name in ("WBP_Hud", "WBP_Pause", "WBP_EndCard"):
        if c.load_generated_class(UI_PATH, name) is None:
            fail(name + "_C")
        else:
            say("  {0}_C loads".format(name))


def check_pickup_parts():
    """Both pickups are built from Part components with real materials, not one grey cube."""
    say("---- pickup parts ----")

    for name, minimum in (("BP_Pickup_Pistol", 4), ("BP_Pickup_Keycard", 2)):
        cls = c.load_generated_class(WORLD_PATH, name)
        if cls is None:
            fail(name + "_C")
            continue

        cdo = unreal.get_default_object(cls)
        shaped = 0
        for index in range(1, 5):
            part = prop(cdo, "part{0}".format(index))
            if part is None:
                continue
            mesh_asset = prop(part, "static_mesh")
            material = None
            try:
                material = part.get_material(0)
            except Exception:  # noqa: BLE001 - an empty slot reads back as None
                material = None
            if mesh_asset is None:
                continue
            say("  {0}.Part{1} = {2} / {3}".format(
                name, index, name_of(mesh_asset), name_of(material)))
            if material is not None:
                shaped += 1

        if shaped < minimum:
            fail("{0} has {1} shaped part(s) with a material, expected {2}".format(
                name, shaped, minimum))


def check_guard_presentation():
    """Riot-cop materials, a flashlight and the two locomotion sequences."""
    say("---- guard look ----")

    cls = c.load_generated_class(AI_PATH, "BP_Guard")
    if cls is None:
        fail("BP_Guard_C")
        return

    cdo = unreal.get_default_object(cls)

    for field in ("idle_anim", "walk_anim"):
        value = prop(cdo, field)
        say("  BP_Guard.{0:<10} = {1}".format(field, name_of(value)))
        if value is None:
            fail("BP_Guard." + field + " is unset; the guard would T-pose")

    flashlight = prop(cdo, "flashlight")
    say("  BP_Guard.flashlight  = {0}".format(name_of(flashlight)))
    if flashlight is None:
        fail("BP_Guard has no flashlight component")

    component = prop(cdo, "mesh")
    if component is None:
        fail("BP_Guard has no mesh component")
        return

    say("  BP_Guard.Mesh.animation_mode = {0}".format(prop(component, "animation_mode")))
    for slot in (0, 1):
        material = None
        try:
            material = component.get_material(slot)
        except Exception:  # noqa: BLE001
            material = None
        say("  BP_Guard.Mesh slot {0} = {1}".format(slot, name_of(material)))
        if material is None:
            fail("BP_Guard.Mesh slot {0} has no material".format(slot))


def check_view_model():
    """Frank's view model: the pistol, on the camera. There are deliberately no arms."""
    say("---- first-person view model ----")

    cls = c.load_generated_class(PLAYER_PATH, "BP_CastleCharacter")
    if cls is None:
        fail("BP_CastleCharacter_C")
        return

    cdo = unreal.get_default_object(cls)

    weapon = prop(cdo, "weapon_mesh")
    weapon_asset = prop(weapon, "static_mesh") if weapon is not None else None
    say("  WeaponMesh.static_mesh       = {0}".format(name_of(weapon_asset)))
    if weapon_asset is None:
        fail("BP_CastleCharacter.WeaponMesh has no pistol mesh")

    say("  WeaponRelativeLocation       = {0}".format(prop(cdo, "weapon_relative_location")))
    say("  WeaponAimLocation            = {0}".format(prop(cdo, "weapon_aim_location")))

    # The only skeletal mesh available is the full body mannequin, which wraps the camera in
    # its own torso. Arms stay off until there is an arms-only mesh to use.
    if prop(cdo, "use_arms_mesh"):
        fail("BP_CastleCharacter.bUseArmsMesh is on; the mannequin arms fill the screen")
    arms = prop(cdo, "arms_mesh")
    arms_asset = prop(arms, "skeletal_mesh_asset") if arms is not None else None
    say("  ArmsMesh.skeletal_mesh_asset = {0}".format(name_of(arms_asset)))
    if arms_asset is not None:
        fail("BP_CastleCharacter.ArmsMesh has a mesh; run create_blueprints.py to clear it")


def check_data_assets():
    say("---- data assets ----")
    mission = c.load_or_none("/Game/Missions/DA_M01_CellBlockD")
    if mission is None:
        fail("DA_M01_CellBlockD")
    else:
        objectives = list(prop(mission, "objectives") or [])
        say(
            "  DA_M01_CellBlockD: name='{0}' number={1} objectives={2} enforce_order={3}".format(
                prop(mission, "mission_name"),
                prop(mission, "mission_number"),
                len(objectives),
                prop(mission, "enforce_order"),
            )
        )
        for obj in objectives:
            say(
                "    id={0:<18} title='{1}'".format(
                    str(prop(obj, "objective_id")), prop(obj, "title")
                )
            )
        if len(objectives) != 4:
            fail("DA_M01_CellBlockD has {0} objectives, expected 4".format(len(objectives)))
        say("  DA_M01_CellBlockD.flashback_to_play = {0}".format(prop(mission, "flashback_to_play")))
        end_card_line = prop(mission, "end_card_line")
        say("  DA_M01_CellBlockD.end_card_line     = '{0}'".format(end_card_line))
        if not str(end_card_line or ""):
            fail("DA_M01_CellBlockD.end_card_line is empty")

    flashback = c.load_or_none("/Game/Flashbacks/Definitions/DA_FB01_Sunday")
    if flashback is None:
        fail("DA_FB01_Sunday")
    else:
        slides = list(prop(flashback, "slides") or [])
        say(
            "  DA_FB01_Sunday: title='{0}' slides={1} skippable={2}".format(
                prop(flashback, "title"), len(slides), prop(flashback, "skippable")
            )
        )
        for index, slide in enumerate(slides):
            say(
                "    slide {0}: image={1} hold={2} fade={3} caption='{4}'".format(
                    index + 1,
                    prop(slide, "image"),
                    prop(slide, "hold_seconds"),
                    prop(slide, "crossfade_seconds"),
                    prop(slide, "caption"),
                )
            )
        if len(slides) != 6:
            fail("DA_FB01_Sunday has {0} slides, expected 6".format(len(slides)))


LIGHT_ACTOR_CLASSES = (unreal.DirectionalLight, unreal.SkyLight, unreal.PointLight)


def check_lighting_and_materials(map_path, actors):
    """Every light must be Movable; every mesh actor must have a real material in slot 0."""
    unbuilt_lights = []
    for actor in actors:
        if not isinstance(actor, LIGHT_ACTOR_CLASSES):
            continue
        mobility = c.actor_mobility(actor) if hasattr(c, "actor_mobility") else None
        if mobility != unreal.ComponentMobility.MOVABLE:
            unbuilt_lights.append(label_of(actor))
    say("    lights not Movable: {0}".format(len(unbuilt_lights)))
    if unbuilt_lights:
        fail(
            "{0}: {1} light(s) not Movable ({2})".format(
                map_path, len(unbuilt_lights), ", ".join(unbuilt_lights)
            )
        )

    unmaterialed_meshes = []
    for actor in actors:
        if not isinstance(actor, unreal.StaticMeshActor):
            continue
        try:
            component = actor.get_editor_property("static_mesh_component")
        except Exception:  # noqa: BLE001
            continue
        has_material = hasattr(c, "has_material_override") and c.has_material_override(component)
        if not has_material:
            unmaterialed_meshes.append(label_of(actor))
    say("    mesh actors with no material: {0}".format(len(unmaterialed_meshes)))
    if unmaterialed_meshes:
        fail(
            "{0}: {1} mesh actor(s) with no material ({2})".format(
                map_path, len(unmaterialed_meshes), ", ".join(unmaterialed_meshes)
            )
        )


def check_maps():
    say("---- maps ----")
    subsystem = c.level_editor_subsystem()
    for map_path in ("/Game/Maps/L_Sandbox", "/Game/Maps/L_M01_CellBlockD"):
        if not c.exists(map_path):
            fail(map_path)
            continue
        if subsystem is not None and not subsystem.load_level(map_path):
            fail(map_path + " would not open")
            continue
        actors = c.all_level_actors()
        settings = c.world_settings()
        game_mode = prop(settings, "default_game_mode") if settings else None
        say(
            "  {0}: {1} actors, GameMode override = {2}".format(
                map_path, len(actors), c.class_name(game_mode)
            )
        )
        if game_mode is None:
            fail(map_path + " has no GameMode override")

        check_lighting_and_materials(map_path, actors)

        triggers = [
            a
            for a in actors
            if isinstance(a, unreal.TriggerBox) or "ObjectiveTriggerVolume" in c.class_name(type(a))
        ]
        for trigger in triggers:
            say(
                "    {0:<22} objective_id={1}".format(
                    trigger.get_actor_label(), prop(trigger, "objective_id")
                )
            )
        starts = [a for a in actors if isinstance(a, unreal.PlayerStart)]
        say("    PlayerStarts: {0}, trigger volumes: {1}".format(len(starts), len(triggers)))

        if map_path.endswith("L_M01_CellBlockD"):
            check_m01_gameplay(actors)


def label_of(actor):
    try:
        return actor.get_actor_label()
    except Exception:  # noqa: BLE001
        return "<unlabelled>"


def check_m01_gameplay(actors):
    """Mission 1 needs guards with patrol points, a door, the pickups and a nav volume."""
    guards = [a for a in actors if "BP_Guard" in c.class_name(type(a)) or "GuardCharacter" in c.class_name(type(a))]
    doors = [a for a in actors if "Door" in c.class_name(type(a)) and "Frame" not in c.class_name(type(a))]
    pickups = [a for a in actors if "Pickup" in c.class_name(type(a))]
    points = [a for a in actors if isinstance(a, unreal.TargetPoint)]
    nav = [a for a in actors if isinstance(a, unreal.NavMeshBoundsVolume)]

    say("    guards: {0}, patrol points: {1}, pickups: {2}, doors: {3}, nav volumes: {4}".format(
        len(guards), len(points), len(pickups), len(doors), len(nav)))

    total_patrol = 0
    for guard in guards:
        assigned = list(prop(guard, "patrol_points") or [])
        loot = list(prop(guard, "drop_on_death") or [])
        total_patrol += len(assigned)
        say("      {0:<16} patrol={1} drops={2}".format(
            label_of(guard), len(assigned), ", ".join(c.class_name(x) for x in loot) or "-"))
        if len(assigned) < 2:
            fail("{0} has {1} patrol point(s), expected 2".format(label_of(guard), len(assigned)))

    if len(guards) != 5:
        fail("L_M01_CellBlockD has {0} guards, expected 5".format(len(guards)))
    if len(points) < 10:
        fail("L_M01_CellBlockD has {0} ATargetPoints, expected at least 10".format(len(points)))
    if not doors:
        fail("L_M01_CellBlockD has no BP_Door_Keycard")
    if len(pickups) < 2:
        fail("L_M01_CellBlockD has {0} pickups, expected at least 2".format(len(pickups)))
    if not nav:
        fail("L_M01_CellBlockD has no NavMeshBoundsVolume; guards cannot move")

    looters = [g for g in guards if list(prop(g, "drop_on_death") or [])]
    if len(looters) != 1:
        fail("{0} guard(s) carry loot, expected exactly 1".format(len(looters)))


def main():
    say("==== verifying stage 1-2 starter content ====")
    check_existence()
    check_input()
    check_blueprints()
    check_world_blueprints()
    check_pickup_parts()
    check_guard_presentation()
    check_view_model()
    check_data_assets()
    check_maps()
    if PROBLEMS:
        unreal.log_error("[Verify] FAIL: {0} problem(s)".format(len(PROBLEMS)))
        for problem in PROBLEMS:
            unreal.log_error("[Verify]   " + problem)
    else:
        say("==== PASS: every expected asset is present and populated ====")


main()

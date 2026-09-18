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
IMAGE_PATH = "/Game/Flashbacks/Images"

IA_NAMES = [
    "IA_Move", "IA_Look", "IA_Jump", "IA_Sprint", "IA_Crouch", "IA_Fire",
    "IA_Aim", "IA_Reload", "IA_Takedown", "IA_Interact", "IA_Pause", "IA_Skip",
]

CHARACTER_INPUT_PROPS = [
    "default_mapping_context", "move_action", "look_action", "jump_action", "sprint_action",
    "crouch_action", "fire_action", "reload_action", "takedown_action", "interact_action",
]

EXPECTED = (
    [c.asset_path(INPUT_PATH, n) for n in IA_NAMES]
    + [
        c.asset_path(INPUT_PATH, "IMC_Default"),
        c.asset_path(PLAYER_PATH, "BP_CastleCharacter"),
        c.asset_path(PLAYER_PATH, "BP_CastlePlayerController"),
        c.asset_path(PLAYER_PATH, "BP_CastleGameMode"),
        c.asset_path(UI_PATH, "WBP_Flashback"),
    ]
    + [c.asset_path(IMAGE_PATH, "T_FB01_0{0}".format(i)) for i in range(1, 7)]
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
        say(
            "  BP_CastlePlayerController.flashback_widget_class = {0}".format(
                name_of(prop(cdo, "flashback_widget_class"))
            )
        )


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


def main():
    say("==== verifying stage 1-2 starter content ====")
    check_existence()
    check_input()
    check_blueprints()
    check_data_assets()
    check_maps()
    if PROBLEMS:
        unreal.log_error("[Verify] FAIL: {0} problem(s)".format(len(PROBLEMS)))
        for problem in PROBLEMS:
            unreal.log_error("[Verify]   " + problem)
    else:
        say("==== PASS: every expected asset is present and populated ====")


main()

"""Create the player-framework Blueprints and wire their class defaults.

    /Game/Blueprints/Player/BP_CastleCharacter        parent ACastleCharacter
    /Game/Blueprints/Player/BP_CastlePlayerController parent ACastlePlayerController
    /Game/Blueprints/Player/BP_CastleGameMode         parent ACastleGameMode
    /Game/Blueprints/UI/WBP_Flashback                 parent UFlashbackWidget
    /Game/Blueprints/UI/WBP_Pause                     parent UCastlePauseWidget

Then, on the class default objects:

    BP_CastleCharacter       DefaultMappingContext = IMC_Default, every IA_* property
                             that exists on ACastleCharacter
    BP_CastleGameMode        DefaultPawnClass, PlayerControllerClass
    BP_CastlePlayerController FlashbackWidgetClass = WBP_Flashback_C,
                             PauseWidgetClass = WBP_Pause_C, PauseAction = IA_Pause,
                             PauseMappingContext = IMC_Default

Property names come from Source/Castle/Player/CastleCharacter.h and
Source/Castle/CastlePlayerController.h. Anything not found on the class is reported and
skipped rather than aborting.
"""

import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _common as c  # noqa: E402

PLAYER_PATH = "/Game/Blueprints/Player"
UI_PATH = "/Game/Blueprints/UI"
INPUT_PATH = "/Game/Input"

# ACastleCharacter input property name -> IA asset name.
# Pause lives on ACastlePlayerController, not the pawn, so that Escape still works when the
# pawn is locked out or dead; IA_Skip is consumed by the flashback widget's key handler and
# has no property to bind to. Both are reported as skipped here, which is expected.
CHARACTER_INPUT_PROPERTIES = [
    ("move_action", "IA_Move"),
    ("look_action", "IA_Look"),
    ("jump_action", "IA_Jump"),
    ("sprint_action", "IA_Sprint"),
    ("crouch_action", "IA_Crouch"),
    ("fire_action", "IA_Fire"),
    ("aim_action", "IA_Aim"),
    ("reload_action", "IA_Reload"),
    ("takedown_action", "IA_Takedown"),
    ("interact_action", "IA_Interact"),
    ("skip_action", "IA_Skip"),
]


def make_blueprint(name, path, parent_class, factory_names, quiet=False):
    """Create a Blueprint with the given parent, or return the existing one.

    Never raises: a failure here must not stop the other Blueprints being made.
    """
    try:
        return _make_blueprint(name, path, parent_class, factory_names, quiet)
    except Exception as exc:  # noqa: BLE001
        c.log_error(c.asset_path(path, name), exc)
        return None, False


def _make_blueprint(name, path, parent_class, factory_names, quiet=False):
    full = c.asset_path(path, name)
    if parent_class is None:
        c.log("FAILED", full, "parent class not found")
        return None, False

    existing = c.load_or_none(full)
    if existing is not None:
        c.log("exists", full)
        return existing, False

    factory = c.new_factory(*factory_names)
    if factory is None:
        c.log("FAILED", full, "no factory class among " + ", ".join(factory_names))
        return None, False

    c.set_props(factory, [("parent_class", parent_class)], name + " factory")

    asset_class = unreal.Blueprint
    if "WidgetBlueprintFactory" in factory_names and hasattr(unreal, "WidgetBlueprint"):
        asset_class = unreal.WidgetBlueprint

    bp, created = c.create_asset(name, path, asset_class, factory, quiet=True)
    if bp is None:
        return None, False
    # parent_class is a Python type object, so get_name() on it is unbound - use class_name.
    c.log("created", full, "parent " + c.class_name(parent_class))
    return bp, created


def apply_defaults(bp, name, path, values):
    """set_editor_property on the Blueprint CDO, then compile + save. Returns applied names."""
    full = c.asset_path(path, name)
    cdo = c.blueprint_cdo(bp)
    if cdo is None:
        cdo = unreal.get_default_object(c.load_generated_class(path, name))
    if cdo is None:
        c.log("FAILED", full, "no class default object")
        return []

    wanted = []
    already = []
    for prop, value in values:
        if value is None:
            continue
        try:
            if cdo.get_editor_property(prop) == value:
                already.append(prop)
                continue
        except Exception:  # noqa: BLE001 - property missing; set_props will report it
            pass
        wanted.append((prop, value))

    missing_values = [prop for prop, value in values if value is None]
    applied = c.set_props(cdo, wanted, name)
    skipped = [prop for prop, _v in wanted if prop not in applied] + missing_values

    # Only compile and save when something actually changed, so a re-run is a true no-op.
    if applied:
        c.compile_blueprint(bp)
        c.save(bp)
        c.log("updated", full, "set " + ", ".join(applied))
    elif already and not skipped:
        c.log("exists", full, "{0} defaults already set".format(len(already)))
    if skipped:
        c.log("skipped", full, "no such property / missing asset: " + ", ".join(skipped))
    return applied


def run():
    c.ensure_directory(PLAYER_PATH)
    c.ensure_directory(UI_PATH)

    character_parent = c.find_class("CastleCharacter", "/Script/Castle.CastleCharacter")
    controller_parent = c.find_class(
        "CastlePlayerController", "/Script/Castle.CastlePlayerController"
    )
    game_mode_parent = c.find_class("CastleGameMode", "/Script/Castle.CastleGameMode")
    widget_parent = c.find_class("FlashbackWidget", "/Script/Castle.FlashbackWidget")

    bp_factories = ("BlueprintFactory",)
    wbp_factories = ("WidgetBlueprintFactory",)

    pause_parent = c.find_class("CastlePauseWidget", "/Script/Castle.CastlePauseWidget")

    wbp_flashback, _ = make_blueprint("WBP_Flashback", UI_PATH, widget_parent, wbp_factories)
    wbp_pause, _ = make_blueprint("WBP_Pause", UI_PATH, pause_parent, wbp_factories)
    bp_character, _ = make_blueprint(
        "BP_CastleCharacter", PLAYER_PATH, character_parent, bp_factories
    )
    bp_controller, _ = make_blueprint(
        "BP_CastlePlayerController", PLAYER_PATH, controller_parent, bp_factories
    )
    bp_game_mode, _ = make_blueprint(
        "BP_CastleGameMode", PLAYER_PATH, game_mode_parent, bp_factories
    )

    # Newly created Blueprints need to exist on disk before load_class can find the _C.
    for bp in (wbp_flashback, wbp_pause, bp_character, bp_controller, bp_game_mode):
        if bp is not None:
            c.compile_blueprint(bp)
            c.save(bp, only_if_dirty=True)

    # --- BP_CastleCharacter -------------------------------------------------------------
    if bp_character is not None:
        values = [("default_mapping_context", c.load_or_none(c.asset_path(INPUT_PATH, "IMC_Default")))]
        for prop, asset_name in CHARACTER_INPUT_PROPERTIES:
            values.append((prop, c.load_or_none(c.asset_path(INPUT_PATH, asset_name))))
        apply_defaults(bp_character, "BP_CastleCharacter", PLAYER_PATH, values)

    # --- BP_CastlePlayerController ------------------------------------------------------
    if bp_controller is not None:
        apply_defaults(
            bp_controller,
            "BP_CastlePlayerController",
            PLAYER_PATH,
            [
                ("flashback_widget_class", c.load_generated_class(UI_PATH, "WBP_Flashback")),
                ("pause_widget_class", c.load_generated_class(UI_PATH, "WBP_Pause")),
                ("pause_action", c.load_or_none(c.asset_path(INPUT_PATH, "IA_Pause"))),
                (
                    "pause_mapping_context",
                    c.load_or_none(c.asset_path(INPUT_PATH, "IMC_Default")),
                ),
            ],
        )

    # --- BP_CastleGameMode --------------------------------------------------------------
    if bp_game_mode is not None:
        apply_defaults(
            bp_game_mode,
            "BP_CastleGameMode",
            PLAYER_PATH,
            [
                ("default_pawn_class", c.load_generated_class(PLAYER_PATH, "BP_CastleCharacter")),
                (
                    "player_controller_class",
                    c.load_generated_class(PLAYER_PATH, "BP_CastlePlayerController"),
                ),
            ],
        )

    return {
        "character": bp_character,
        "controller": bp_controller,
        "game_mode": bp_game_mode,
        "flashback_widget": wbp_flashback,
        "pause_widget": wbp_pause,
    }


if __name__ == "__main__":
    run()
    c.print_summary("blueprints")

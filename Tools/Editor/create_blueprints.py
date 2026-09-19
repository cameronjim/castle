"""Create the player-framework Blueprints and wire their class defaults.

    /Game/Blueprints/Player/BP_CastleCharacter        parent ACastleCharacter
    /Game/Blueprints/Player/BP_CastlePlayerController parent ACastlePlayerController
    /Game/Blueprints/Player/BP_CastleGameMode         parent ACastleGameMode
    /Game/Blueprints/UI/WBP_Flashback                 parent UFlashbackWidget
    /Game/Blueprints/UI/WBP_Pause                     parent UCastlePauseWidget
    /Game/Blueprints/UI/WBP_Settings                  parent UCastleSettingsWidget
    /Game/Blueprints/UI/WBP_EndCard                   parent UMissionEndCardWidget
    /Game/Blueprints/UI/WBP_Hotbar                    parent UCastleHotbarWidget
    /Game/Blueprints/UI/WBP_Inventory                 parent UCastleInventoryWidget

Then, on the class default objects:

    BP_CastleCharacter       DefaultMappingContext = IMC_Default, every IA_* property
                             that exists on ACastleCharacter
    BP_CastleGameMode        DefaultPawnClass, PlayerControllerClass
    BP_CastlePlayerController FlashbackWidgetClass = WBP_Flashback_C,
                             PauseWidgetClass = WBP_Pause_C,
                             SettingsWidgetClass = WBP_Settings_C, PauseAction = IA_Pause,
                             PauseMappingContext = IMC_Default,
                             EndCardWidgetClass = WBP_EndCard_C

Property names come from Source/Castle/Player/CastleCharacter.h and
Source/Castle/CastlePlayerController.h. Anything not found on the class is reported and
skipped rather than aborting.
"""

import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _common as c  # noqa: E402
import _materials as m  # noqa: E402

PLAYER_PATH = "/Game/Blueprints/Player"
UI_PATH = "/Game/Blueprints/UI"
INPUT_PATH = "/Game/Input"

# True first person. UE 5.8 ships no arms-only mesh, so SK_Mannequin is used twice: once as
# Frank's actual body (ACharacter's own Mesh, animated, shadow-casting, head hidden) and once
# as the poseable arms in front of the camera. See ACastleCharacter and
# UFirstPersonArmsComponent.
MANNEQUIN_MESH = "/Game/Mannequin/Character/Mesh/SK_Mannequin"
MANNEQUIN_IDLE = "/Game/Mannequin/Animations/ThirdPersonIdle"
MANNEQUIN_WALK = "/Game/Mannequin/Animations/ThirdPersonWalk"

# Copied out of Templates/TemplateResources/Standard/Weapons. Its internal references are
# absolute (/Game/Weapons/...), so Content/Weapons is where these have to live.
PISTOL_MESH = "/Game/Weapons/Pistol/Meshes/SM_Pistol"

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
    ("slot1_action", "IA_Slot1"),
    ("slot2_action", "IA_Slot2"),
    ("slot3_action", "IA_Slot3"),
    ("slot_scroll_action", "IA_SlotScroll"),
    ("inventory_action", "IA_Inventory"),
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


def component_asset(component, prop_name):
    """The asset a component already has, by property or by its getter. None when unknown."""
    try:
        return component.get_editor_property(prop_name)
    except Exception:  # noqa: BLE001 - private UPROPERTY; try the getter instead
        pass

    getter = getattr(component, "get_" + prop_name, None)
    if getter is None:
        return None
    try:
        return getter()
    except Exception:  # noqa: BLE001
        return None


def set_component_asset(bp, component_name, setter_name, prop_name, asset, context):
    """Assign a mesh on an inherited component through the Blueprint CDO. Returns True if changed.

    The CDO's component instance is the template every spawned actor copies, which is what a
    designer edits in the Components panel.
    """
    if asset is None:
        c.log("skipped", context, "asset not found")
        return False

    cdo = c.blueprint_cdo(bp)
    component = None
    if cdo is not None:
        try:
            component = cdo.get_editor_property(component_name)
        except Exception:  # noqa: BLE001
            component = None
    if component is None:
        c.log("skipped", context, "no component called " + component_name)
        return False

    if component_asset(component, prop_name) == asset:
        return False

    try:
        getattr(component, setter_name)(asset)
        c.log("updated", context, asset.get_name())
        return True
    except Exception as exc:  # noqa: BLE001
        c.log_error(context, exc)
        return False


def mesh_slot_names(mesh):
    """The material slot names of a skeletal mesh, in order. Empty list when it has none."""
    names = []
    if mesh is None:
        return names
    try:
        slots = mesh.get_editor_property("materials") or []
    except Exception:  # noqa: BLE001
        return names
    for slot in slots:
        try:
            names.append(str(slot.get_editor_property("material_slot_name")))
        except Exception:  # noqa: BLE001
            names.append("")
    return names


def set_component_materials(bp, component_name, slot_names, materials, context):
    """Put Frank's fatigues on every slot of a mesh component. Returns True if anything changed.

    The mannequin is one material over the whole body, so a "hands" slot is something only a
    future arms pack would have; if one shows up it gets the gloves instead of the sleeves.
    """
    arms = materials.get("arms")
    gloves = materials.get("gloves") or arms
    if arms is None:
        c.log("skipped", context, "M_FrankArms was not created")
        return False

    cdo = c.blueprint_cdo(bp)
    component = None
    if cdo is not None:
        try:
            component = cdo.get_editor_property(component_name)
        except Exception:  # noqa: BLE001
            component = None
    if component is None:
        c.log("skipped", context, "no component called " + component_name)
        return False

    changed = False
    for index, slot in enumerate(slot_names or [""]):
        wanted = gloves if ("hand" in slot.lower() or "glove" in slot.lower()) else arms
        try:
            if component.get_material(index) == wanted:
                continue
            component.set_material(index, wanted)
            c.log("updated", context, "slot {0} -> {1}".format(index, wanted.get_name()))
            changed = True
        except Exception as exc:  # noqa: BLE001
            c.log_error("{0} slot {1}".format(context, index), exc)
    return changed


def configure_view_model(bp):
    """Give BP_CastleCharacter its body, its hands and its pistol.

    Three assignments, all SK_Mannequin or its sequences:
      Mesh      Frank's real body. Animated by IdleAnim/WalkAnim, head hidden in C++.
      ArmsMesh  the poseable hands in front of the camera, posed in C++, never animated.
      WeaponMesh the pistol, parented to the arms' hand_r.
    """
    if bp is None:
        return

    pistol = c.load_or_none(PISTOL_MESH)
    mannequin = c.load_or_none(MANNEQUIN_MESH)

    changed = set_component_asset(
        bp, "mesh", "set_skeletal_mesh_asset", "skeletal_mesh_asset", mannequin,
        "BP_CastleCharacter.Mesh")
    # A poseable mesh is a USkinnedMeshComponent, so it takes the skinned-asset setter rather
    # than the skeletal-mesh one a USkeletalMeshComponent has.
    changed = set_component_asset(
        bp, "arms_mesh", "set_skinned_asset_and_update", "skinned_asset", mannequin,
        "BP_CastleCharacter.ArmsMesh") or changed
    changed = set_component_asset(
        bp, "weapon_mesh", "set_static_mesh", "static_mesh", pistol,
        "BP_CastleCharacter.WeaponMesh") or changed

    # Shiny white plastic is what the mannequin ships as, and it is the first thing a player
    # sees. Both meshes get the same fatigues so the legs match the forearms.
    c.ensure_directory(m.MATERIALS_PATH)
    character_materials = m.ensure_character_materials()
    slots = mesh_slot_names(mannequin)
    changed = set_component_materials(
        bp, "arms_mesh", slots, character_materials, "BP_CastleCharacter.ArmsMesh") or changed
    changed = set_component_materials(
        bp, "mesh", slots, character_materials, "BP_CastleCharacter.Mesh") or changed

    if changed:
        c.compile_blueprint(bp)
        c.save(bp)


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
    settings_parent = c.find_class(
        "CastleSettingsWidget", "/Script/Castle.CastleSettingsWidget"
    )
    end_card_parent = c.find_class(
        "MissionEndCardWidget", "/Script/Castle.MissionEndCardWidget"
    )

    wbp_flashback, _ = make_blueprint("WBP_Flashback", UI_PATH, widget_parent, wbp_factories)
    wbp_pause, _ = make_blueprint("WBP_Pause", UI_PATH, pause_parent, wbp_factories)
    wbp_settings, _ = make_blueprint("WBP_Settings", UI_PATH, settings_parent, wbp_factories)
    wbp_end_card, _ = make_blueprint("WBP_EndCard", UI_PATH, end_card_parent, wbp_factories)

    hotbar_parent = c.find_class("CastleHotbarWidget", "/Script/Castle.CastleHotbarWidget")
    inventory_parent = c.find_class("CastleInventoryWidget", "/Script/Castle.CastleInventoryWidget")
    wbp_hotbar, _ = make_blueprint("WBP_Hotbar", UI_PATH, hotbar_parent, wbp_factories)
    wbp_inventory, _ = make_blueprint("WBP_Inventory", UI_PATH, inventory_parent, wbp_factories)
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
    for bp in (
        wbp_flashback, wbp_pause, wbp_settings, wbp_end_card, wbp_hotbar, wbp_inventory,
        bp_character, bp_controller, bp_game_mode,
    ):
        if bp is not None:
            c.compile_blueprint(bp)
            c.save(bp, only_if_dirty=True)

    # --- BP_CastleCharacter -------------------------------------------------------------
    if bp_character is not None:
        values = [("default_mapping_context", c.load_or_none(c.asset_path(INPUT_PATH, "IMC_Default")))]
        for prop, asset_name in CHARACTER_INPUT_PROPERTIES:
            values.append((prop, c.load_or_none(c.asset_path(INPUT_PATH, asset_name))))
        # The body plays the same two sequences the guards do; there is no AnimBP.
        values.append(("idle_anim", c.load_or_none(MANNEQUIN_IDLE)))
        values.append(("walk_anim", c.load_or_none(MANNEQUIN_WALK)))
        apply_defaults(bp_character, "BP_CastleCharacter", PLAYER_PATH, values)
        configure_view_model(bp_character)

    # --- BP_CastlePlayerController ------------------------------------------------------
    if bp_controller is not None:
        apply_defaults(
            bp_controller,
            "BP_CastlePlayerController",
            PLAYER_PATH,
            [
                ("flashback_widget_class", c.load_generated_class(UI_PATH, "WBP_Flashback")),
                ("pause_widget_class", c.load_generated_class(UI_PATH, "WBP_Pause")),
                ("settings_widget_class", c.load_generated_class(UI_PATH, "WBP_Settings")),
                ("pause_action", c.load_or_none(c.asset_path(INPUT_PATH, "IA_Pause"))),
                (
                    "pause_mapping_context",
                    c.load_or_none(c.asset_path(INPUT_PATH, "IMC_Default")),
                ),
                ("end_card_widget_class", c.load_generated_class(UI_PATH, "WBP_EndCard")),
                ("inventory_widget_class", c.load_generated_class(UI_PATH, "WBP_Inventory")),
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
        "settings_widget": wbp_settings,
        "end_card_widget": wbp_end_card,
        "hotbar_widget": wbp_hotbar,
        "inventory_widget": wbp_inventory,
    }


if __name__ == "__main__":
    run()
    c.print_summary("blueprints")

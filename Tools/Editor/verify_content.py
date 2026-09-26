"""Load every asset create_all.py is supposed to produce and print its key properties.

Read-only: nothing is created, changed or saved. Exits by printing a PASS/FAIL line so the
output can be eyeballed or grepped after a content run.

    UnrealEditor-Cmd.exe Hawkeye.uproject -run=pythonscript ^
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
THUG_MATERIAL_PATH = "/Game/Characters/Thug"
WEAPON_PATH = "/Game/Blueprints/Weapons"
KATE_MATERIAL_PATH = "/Game/Characters/Kate"

IA_NAMES = [
    "IA_Move", "IA_Look", "IA_LookStick", "IA_Jump", "IA_Sprint", "IA_Crouch", "IA_Fire",
    "IA_Aim", "IA_Reload", "IA_Takedown", "IA_Interact", "IA_Pause", "IA_Skip",
    "IA_Slot1", "IA_Slot2", "IA_Slot3", "IA_Slot4", "IA_Slot5", "IA_Slot6", "IA_SlotScroll",
    "IA_Inventory", "IA_Grapple", "IA_Melee",
]

CHARACTER_INPUT_PROPS = [
    "default_mapping_context", "move_action", "look_action", "look_stick_action", "jump_action",
    "sprint_action", "crouch_action", "fire_action", "aim_action", "reload_action",
    "takedown_action", "interact_action", "slot1_action", "slot2_action", "slot3_action",
    "slot4_action", "slot5_action", "slot6_action", "slot_scroll_action", "inventory_action",
    "grapple_action", "melee_action",
]

# (action, key) pairs the gamepad pass must have added to IMC_Default. Xbox layout; a
# PlayStation pad reports the same Gamepad_* keys, so nothing PS-specific is checked here.
GAMEPAD_MAPPINGS = [
    ("IA_Move", "Gamepad_Left2D"),
    ("IA_LookStick", "Gamepad_Right2D"),
    ("IA_Jump", "Gamepad_FaceButton_Bottom"),
    ("IA_Sprint", "Gamepad_LeftThumbstick"),
    ("IA_Crouch", "Gamepad_FaceButton_Right"),
    ("IA_Fire", "Gamepad_RightTrigger"),
    ("IA_Aim", "Gamepad_LeftTrigger"),
    ("IA_Grapple", "Gamepad_RightShoulder"),
    ("IA_Melee", "Gamepad_FaceButton_Left"),
    ("IA_Takedown", "Gamepad_FaceButton_Top"),
    ("IA_Interact", "Gamepad_FaceButton_Top"),
    ("IA_Inventory", "Gamepad_Special_Left"),
    ("IA_Pause", "Gamepad_Special_Right"),
    ("IA_SlotScroll", "Gamepad_DPad_Left"),
    ("IA_SlotScroll", "Gamepad_DPad_Right"),
    ("IA_Slot1", "Gamepad_DPad_Up"),
    ("IA_Slot2", "Gamepad_DPad_Down"),
]

# Pause is bound on the controller so it survives the pawn being locked out or dead.
CONTROLLER_PROPS = [
    "flashback_widget_class", "hud_widget_class", "pause_widget_class",
    "settings_widget_class", "pause_action", "pause_mapping_context", "end_card_widget_class",
    "inventory_widget_class",
]

EXPECTED = (
    [c.asset_path(INPUT_PATH, n) for n in IA_NAMES]
    + [
        c.asset_path(INPUT_PATH, "IMC_Default"),
        c.asset_path(PLAYER_PATH, "BP_HawkeyeCharacter"),
        c.asset_path(PLAYER_PATH, "BP_Kate"),
        KATE_MATERIAL_PATH + "/M_KateSuit",
        KATE_MATERIAL_PATH + "/M_KateSuitDark",
        c.asset_path(PLAYER_PATH, "BP_HawkeyePlayerController"),
        c.asset_path(PLAYER_PATH, "BP_HawkeyeGameMode"),
        c.asset_path(UI_PATH, "WBP_Flashback"),
        c.asset_path(UI_PATH, "WBP_Hud"),
        c.asset_path(UI_PATH, "WBP_Pause"),
        c.asset_path(UI_PATH, "WBP_Settings"),
        c.asset_path(UI_PATH, "WBP_EndCard"),
        c.asset_path(UI_PATH, "WBP_Hotbar"),
        c.asset_path(UI_PATH, "WBP_Inventory"),
        c.asset_path(AI_PATH, "BP_Thug"),
    ]
    + [
        c.asset_path(THUG_MATERIAL_PATH, "M_ThugBody"),
        c.asset_path(THUG_MATERIAL_PATH, "M_ThugVisor"),
    ]
    + [
        WEAPON_PATH + "/DA_Weapon_Hands",
        WEAPON_PATH + "/DA_Bow_Kate",
        WEAPON_PATH + "/DA_Bow_Clint",
        WEAPON_PATH + "/DA_Arrow_Standard",
        WEAPON_PATH + "/DA_Arrow_Grapple",
        WEAPON_PATH + "/BP_Arrow_Standard",
        WEAPON_PATH + "/BP_Arrow_Grapple",
        WEAPON_PATH + "/SM_Bow_Placeholder",
        WEAPON_PATH + "/M_Bow",
        "/Game/Missions/DA_CH01_Rooftops",
        "/Game/Maps/L_District_EastVillage",
    ]
)

# Retired in the stage 2 pivot. Any of these back on disk, or any redirector left under /Game,
# means something restored them or a fix-up did not finish.
RETIRED = [
    "/Game/Blueprints/AI/BP_Guard",
    "/Game/Characters/Guard/M_GuardBody",
    "/Game/Characters/Guard/M_GuardVisor",
    WORLD_PATH + "/BP_Pickup_Pistol",
    WEAPON_PATH + "/DA_Weapon_Pistol",
    WEAPON_PATH + "/DA_Weapon_Rifle",
    "/Game/Materials/M_FrankArms",
    "/Game/Materials/M_FrankGloves",
    "/Game/Materials/M_Pistol",
    "/Game/Weapons/Pistol/Meshes/SM_Pistol",
    "/Game/Weapons/Pistol/Materials/MI_Weapon_Pistol",
    "/Game/Weapons/Rifle/Materials/M_Weapon",
    "/Game/Weapons/Pistol/Textures/T_Pistol_D",
]

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

    say("---- retired in the pivot ----")
    lingering = [p for p in RETIRED if c.exists(p)]
    for p in lingering:
        fail(p + " is back; it was retired in the stage 2 pivot")
    say("{0}/{1} retired assets gone".format(len(RETIRED) - len(lingering), len(RETIRED)))

    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    filt = unreal.ARFilter(
        class_paths=[unreal.TopLevelAssetPath("/Script/CoreUObject", "ObjectRedirector")],
        package_paths=["/Game"],
        recursive_paths=True,
    )
    redirectors = [str(d.package_name) for d in (registry.get_assets(filt) or [])]
    say("  redirectors under /Game: {0}".format(len(redirectors)))
    for redirector in redirectors:
        fail(redirector + " is a redirector; fix up redirectors")


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

    found_pairs = set()
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
        found_pairs.add((name_of(action), str(key_name)))

    say("---- gamepad mappings ----")
    missing_gamepad = [
        "{0} -> {1}".format(action, key)
        for action, key in GAMEPAD_MAPPINGS
        if (action, key) not in found_pairs
    ]
    say("  {0}/{1} gamepad mappings present".format(
        len(GAMEPAD_MAPPINGS) - len(missing_gamepad), len(GAMEPAD_MAPPINGS)))
    for entry in missing_gamepad:
        fail("IMC_Default missing gamepad mapping " + entry)


def check_blueprints():
    say("---- blueprints ----")
    char_class = c.load_generated_class(PLAYER_PATH, "BP_HawkeyeCharacter")
    if char_class is None:
        fail("BP_HawkeyeCharacter_C")
    else:
        cdo = unreal.get_default_object(char_class)
        for name in CHARACTER_INPUT_PROPS:
            value = prop(cdo, name)
            say("  BP_HawkeyeCharacter.{0:<24} = {1}".format(name, name_of(value)))
            if value is None:
                fail("BP_HawkeyeCharacter." + name + " is unset")

    gm_class = c.load_generated_class(PLAYER_PATH, "BP_HawkeyeGameMode")
    if gm_class is None:
        fail("BP_HawkeyeGameMode_C")
    else:
        cdo = unreal.get_default_object(gm_class)
        for name in ("default_pawn_class", "player_controller_class", "starting_mission"):
            say("  BP_HawkeyeGameMode.{0:<24} = {1}".format(name, name_of(prop(cdo, name))))
        if "BP_Kate" not in name_of(prop(cdo, "default_pawn_class")):
            fail("BP_HawkeyeGameMode.default_pawn_class is not BP_Kate; you would play the base Blueprint")

    pc_class = c.load_generated_class(PLAYER_PATH, "BP_HawkeyePlayerController")
    if pc_class is None:
        fail("BP_HawkeyePlayerController_C")
    else:
        cdo = unreal.get_default_object(pc_class)
        for name in CONTROLLER_PROPS:
            value = prop(cdo, name)
            say("  BP_HawkeyePlayerController.{0:<24} = {1}".format(name, name_of(value)))
            if value is None:
                fail("BP_HawkeyePlayerController." + name + " is unset")


def value_text(value):
    """Comparable text for a property value: 'WEAPON' for an enum, str() for everything else."""
    name = getattr(value, "name", None)
    if name is not None:
        return str(name)
    return str(value)


def check_world_blueprints():
    say("---- world blueprints ----")

    thug_class = c.load_generated_class(AI_PATH, "BP_Thug")
    if thug_class is None:
        fail("BP_Thug_C")
    else:
        cdo = unreal.get_default_object(thug_class)
        controller = prop(cdo, "ai_controller_class")
        say("  BP_Thug.ai_controller_class      = {0}".format(name_of(controller)))
        if controller is None or "ThugAIController" not in c.class_name(controller):
            fail("BP_Thug.ai_controller_class is not AThugAIController")
        say("  BP_Thug.auto_possess_ai          = {0}".format(prop(cdo, "auto_possess_ai")))
        tags = [str(t) for t in (prop(cdo, "tags") or [])]
        say("  BP_Thug.tags                     = {0}".format(tags))
        if "Thug" not in tags:
            fail("BP_Thug is not tagged Thug; takedowns cannot find it")

    for name in ("WBP_Hud", "WBP_Pause", "WBP_Settings", "WBP_EndCard",
                 "WBP_Hotbar", "WBP_Inventory"):
        if c.load_generated_class(UI_PATH, name) is None:
            fail(name + "_C")
        else:
            say("  {0}_C loads".format(name))


def check_thug_presentation():
    """The Tracksuit look, no flashlight, the locomotion sequences and the bat."""
    say("---- thug look ----")

    cls = c.load_generated_class(AI_PATH, "BP_Thug")
    if cls is None:
        fail("BP_Thug_C")
        return

    cdo = unreal.get_default_object(cls)

    for field in ("idle_anim", "walk_anim", "run_anim"):
        value = prop(cdo, field)
        say("  BP_Thug.{0:<10} = {1}".format(field, name_of(value)))
        if value is None:
            fail("BP_Thug." + field + " is unset; the thug would T-pose (or skate through the rush)")

    flashlight = prop(cdo, "flashlight")
    say("  BP_Thug.flashlight  = {0}".format(name_of(flashlight)))
    if flashlight is not None:
        fail("BP_Thug still carries a flashlight; street thugs have none")

    bat = prop(cdo, "bat_mesh")
    held = prop(cdo, "held_weapon_component")
    say("  BP_Thug.bat_mesh    = {0}, held_weapon_component = {1}".format(name_of(bat), name_of(held)))
    if bat is None or held is None:
        fail("BP_Thug has no bat (bat_mesh or held_weapon_component missing)")

    component = prop(cdo, "mesh")
    if component is None:
        fail("BP_Thug has no mesh component")
        return

    say("  BP_Thug.Mesh.animation_mode = {0}".format(prop(component, "animation_mode")))
    for slot, wanted in ((0, "M_ThugTracksuit"), (1, "M_ThugTrim")):
        material = None
        try:
            material = component.get_material(slot)
        except Exception:  # noqa: BLE001
            material = None
        say("  BP_Thug.Mesh slot {0} = {1}".format(slot, name_of(material)))
        if material is None or name_of(material) != wanted:
            fail("BP_Thug.Mesh slot {0} is {1}, expected {2}".format(slot, name_of(material), wanted))


def check_third_person():
    """The placeholder third-person player: a visible mannequin under a spring arm, no viewmodel."""
    say("---- third-person player ----")

    cls = c.load_generated_class(PLAYER_PATH, "BP_HawkeyeCharacter")
    if cls is None:
        fail("BP_HawkeyeCharacter_C")
        return

    cdo = unreal.get_default_object(cls)

    body = prop(cdo, "mesh")
    body_asset = prop(body, "skeletal_mesh_asset") if body is not None else None
    say("  Mesh.skeletal_mesh_asset     = {0}".format(name_of(body_asset)))
    if body_asset is None:
        fail("BP_HawkeyeCharacter.Mesh has no body mesh; run create_blueprints.py")
    if body is not None and prop(body, "owner_no_see"):
        fail("BP_HawkeyeCharacter.Mesh is hidden from its owner; the player would be invisible")

    for field in ("idle_anim", "walk_anim", "run_anim", "fall_anim"):
        value = prop(cdo, field)
        say("  BP_HawkeyeCharacter.{0:<9} = {1}".format(field, name_of(value)))
        if value is None:
            fail("BP_HawkeyeCharacter." + field + " is unset; the body stands in a T-pose")

    boom = prop(cdo, "camera_boom")
    camera = prop(cdo, "follow_camera")
    say("  CameraBoom.target_arm_length = {0}".format(prop(boom, "target_arm_length") if boom else None))
    say("  CameraBoom.socket_offset     = {0}".format(prop(boom, "socket_offset") if boom else None))
    say("  FollowCamera                 = {0}".format(name_of(camera)))
    if boom is None:
        fail("BP_HawkeyeCharacter has no CameraBoom")
    elif not prop(boom, "use_pawn_control_rotation"):
        fail("BP_HawkeyeCharacter.CameraBoom does not follow the control rotation")
    if camera is None:
        fail("BP_HawkeyeCharacter has no FollowCamera")

    for retired in ("arms_mesh", "weapon_mesh", "fatigues_material", "use_arms_mesh"):
        if prop(cdo, retired) is not None:
            fail("BP_HawkeyeCharacter still has {0}; the viewmodel code is back".format(retired))

    movement = prop(cdo, "character_movement")
    say("  orient_rotation_to_movement  = {0}".format(prop(movement, "orient_rotation_to_movement")))
    if movement is not None and not prop(movement, "orient_rotation_to_movement"):
        fail("BP_HawkeyeCharacter does not turn to face where it moves")


def check_kate():
    """BP_Kate: a BP_HawkeyeCharacter descendant, 170 cm capsule, mannequin in the purple suit.

    With the Game Animation Sample imported her parent is SandboxCharacter_CMC (itself on
    BP_HawkeyeCharacter); verify_gasp.py checks that chain and the AnimBP in detail.
    """
    say("---- BP_Kate ----")

    bp = c.load_or_none(c.asset_path(PLAYER_PATH, "BP_Kate"))
    cls = c.load_generated_class(PLAYER_PATH, "BP_Kate")
    if bp is None or cls is None:
        fail("BP_Kate_C")
        return

    # ParentClass is not exposed to Python as a property; the asset registry tag carries it.
    parent = None
    try:
        data = unreal.EditorAssetLibrary.find_asset_data(c.asset_path(PLAYER_PATH, "BP_Kate"))
        parent = str(data.get_tag_value("ParentClass") or "")
    except Exception:  # noqa: BLE001
        parent = None
    say("  parent_class                 = {0}".format(parent))
    if not any(name in str(parent or "") for name in ("BP_HawkeyeCharacter", "SandboxCharacter_CMC")):
        fail("BP_Kate's parent is {0}, expected SandboxCharacter_CMC or BP_HawkeyeCharacter".format(parent))

    cdo = unreal.get_default_object(cls)
    capsule = prop(cdo, "capsule_component")
    half_height = prop(capsule, "capsule_half_height") if capsule is not None else None
    radius = prop(capsule, "capsule_radius") if capsule is not None else None
    say("  capsule half-height / radius = {0} / {1}".format(half_height, radius))
    if half_height is None or abs(float(half_height) - 85.0) > 0.5:
        fail("BP_Kate capsule half-height is {0}, expected 85 (170 cm tall)".format(half_height))

    body = prop(cdo, "mesh")
    body_asset = prop(body, "skeletal_mesh_asset") if body is not None else None
    say("  Mesh.skeletal_mesh_asset     = {0}".format(name_of(body_asset)))
    if body_asset is None:
        fail("BP_Kate has no body mesh")
    location = prop(body, "relative_location") if body is not None else None
    say("  Mesh.relative_location       = {0}".format(location))
    if location is not None and abs(location.z + 85.0) > 0.5:
        fail("BP_Kate's feet are not on the capsule bottom (mesh z {0}, expected -85)".format(location.z))

    for slot, wanted in ((0, "M_KateSuit"), (1, "M_KateSuitDark")):
        material = None
        try:
            material = body.get_material(slot) if body is not None else None
        except Exception:  # noqa: BLE001
            material = None
        say("  Mesh slot {0}                  = {1}".format(slot, name_of(material)))
        if name_of(material) != wanted:
            fail("BP_Kate.Mesh slot {0} is {1}, expected {2}".format(slot, name_of(material), wanted))

    for field in ("idle_anim", "walk_anim", "run_anim", "default_mapping_context", "move_action"):
        value = prop(cdo, field)
        say("  BP_Kate.{0:<24} = {1}".format(field, name_of(value)))
        if value is None:
            fail("BP_Kate." + field + " is unset; it should inherit from BP_HawkeyeCharacter")

    inventory = prop(cdo, "inventory_component")
    hands = prop(inventory, "hands_definition") if inventory is not None else None
    say("  BP_Kate.Inventory.hands_definition = {0}".format(hands))
    if "DA_Weapon_Hands" not in str(hands or ""):
        fail("BP_Kate.InventoryComponent.hands_definition is not DA_Weapon_Hands")


def check_weapon_data():
    """DA_Weapon_Hands, the bows and arrows, and the places they have to be wired into."""
    say("---- weapon data ----")

    expected = {
        "DA_Weapon_Hands": [("is_melee", True), ("damage", 15.0), ("melee_range", 120.0)],
        "DA_Bow_Kate": [("full_draw_seconds", 0.8), ("min_draw_fraction", 0.25), ("max_speed", 6000.0),
                        ("min_speed_fraction", 0.4), ("min_spread", 0.5), ("max_spread", 4.0),
                        ("perfect_window_seconds", 0.1), ("perfect_bonus", 0.25), ("hand_socket", "palm_l_Socket")],
        "DA_Bow_Clint": [("full_draw_seconds", 1.0)],
        "DA_Arrow_Standard": [("slot", 1), ("damage", 40.0), ("cap", 30), ("recoverable", True),
                              ("on_hit_effect", "NONE")],
        "DA_Arrow_Grapple": [("slot", 2), ("cap", 6), ("recoverable", True), ("on_hit_effect", "GRAPPLE")],
    }

    for name, fields in expected.items():
        asset = c.load_or_none(c.asset_path(WEAPON_PATH, name))
        if asset is None:
            fail(name)
            continue
        say("  {0}".format(name))
        for field, want in fields:
            got = prop(asset, field)
            say("    {0:<22} = {1}".format(field, got))
            same = abs(float(got) - want) < 1e-4 if isinstance(want, float) and got is not None else \
                value_text(got) == str(want)
            if not same:
                fail("{0}.{1} is {2}, expected {3}".format(name, field, got, want))

    for bow in ("DA_Bow_Kate", "DA_Bow_Clint"):
        mesh = prop(c.load_or_none(c.asset_path(WEAPON_PATH, bow)), "bow_mesh")
        say("  {0}.bow_mesh = {1}".format(bow, mesh))
        if "SM_Bow_Placeholder" not in str(mesh or ""):
            fail(bow + ".bow_mesh is not SM_Bow_Placeholder")
    for arrow, projectile in (("DA_Arrow_Standard", "BP_Arrow_Standard"), ("DA_Arrow_Grapple", "BP_Arrow_Grapple")):
        cls = prop(c.load_or_none(c.asset_path(WEAPON_PATH, arrow)), "projectile_class")
        say("  {0}.projectile_class = {1}".format(arrow, name_of(cls)))
        if projectile not in str(name_of(cls)):
            fail("{0}.projectile_class is {1}, expected {2}_C".format(arrow, name_of(cls), projectile))

    char_class = c.load_generated_class(PLAYER_PATH, "BP_HawkeyeCharacter")
    if char_class is not None:
        inventory = prop(unreal.get_default_object(char_class), "inventory_component")
        hands = prop(inventory, "hands_definition") if inventory is not None else None
        say("  BP_HawkeyeCharacter.Inventory.hands_definition = {0}".format(hands))
        if inventory is None:
            fail("BP_HawkeyeCharacter has no InventoryComponent")
        elif "DA_Weapon_Hands" not in str(hands or ""):
            fail("BP_HawkeyeCharacter.InventoryComponent.hands_definition is not DA_Weapon_Hands")
        standard = prop(inventory, "standard_arrow_definition") if inventory is not None else None
        say("  BP_HawkeyeCharacter.Inventory.standard_arrow_definition = {0}".format(standard))
        if "DA_Arrow_Standard" not in str(standard or ""):
            fail("BP_HawkeyeCharacter.InventoryComponent.standard_arrow_definition is not DA_Arrow_Standard")


def check_data_assets():
    say("---- data assets ----")
    chapter = c.load_or_none("/Game/Missions/DA_CH01_Rooftops")
    if chapter is None:
        fail("DA_CH01_Rooftops")
    else:
        bow = prop(chapter, "starting_bow")
        grants = [(str(prop(g, "arrow")), prop(g, "count")) for g in list(prop(chapter, "starting_arrows") or [])]
        say("  DA_CH01_Rooftops: starting_bow={0} starting_arrows={1}".format(bow, grants))
        if "DA_Bow_Kate" not in str(bow or ""):
            fail("DA_CH01_Rooftops.starting_bow is not DA_Bow_Kate")
        ids = [str(prop(obj, "objective_id")) for obj in list(prop(chapter, "objectives") or [])]
        say("  DA_CH01_Rooftops: objectives={0}".format(ids))
        wanted_ids = ["reach_roof", "cross_block", "clear_roof", "find_arrow"]
        if ids != wanted_ids:
            fail("DA_CH01_Rooftops objectives are {0}, expected {1}".format(ids, wanted_ids))
        wanted = [("DA_Arrow_Standard", 30), ("DA_Arrow_Grapple", 6)]
        if len(grants) != len(wanted) or any(
                name not in arrow or count != want for (arrow, count), (name, want) in zip(grants, wanted)):
            fail("DA_CH01_Rooftops.starting_arrows is {0}, expected {1}".format(grants, wanted))


SKELETAL_MESH_COMPONENTS = (
    (AI_PATH, "BP_Thug", ("mesh",)),
    (PLAYER_PATH, "BP_HawkeyeCharacter", ("mesh",)),
    (PLAYER_PATH, "BP_Kate", ("mesh",)),
)


def base_material(material):
    """Walk a material instance up to the Material that owns the usage flags."""
    seen = 0
    while material is not None and not isinstance(material, unreal.Material) and seen < 8:
        try:
            material = material.get_editor_property("parent")
        except Exception:  # noqa: BLE001
            return None
        seen += 1
    return material if isinstance(material, unreal.Material) else None


def check_skeletal_material_usage():
    """Every material on a skeletal mesh must have bUsedWithSkeletalMesh.

    Without it the renderer silently substitutes the grey engine default and logs
    "missing usage flag SkeletalMesh!" - which is what had Frank in white plastic sleeves and
    the guards in mannequin grey while every asset check passed.
    """
    say("---- skeletal mesh material usage ----")
    for path, name, component_names in SKELETAL_MESH_COMPONENTS:
        cls = c.load_generated_class(path, name)
        if cls is None:
            continue
        cdo = unreal.get_default_object(cls)
        for component_name in component_names:
            component = prop(cdo, component_name)
            if component is None:
                continue
            for slot in range(4):
                try:
                    material = component.get_material(slot)
                except Exception:  # noqa: BLE001
                    break
                if material is None:
                    continue
                base = base_material(material)
                flag = None
                if base is not None:
                    try:
                        flag = bool(base.get_editor_property("used_with_skeletal_mesh"))
                    except Exception:  # noqa: BLE001
                        flag = None
                say("  {0}.{1} slot {2} = {3} used_with_skeletal_mesh={4}".format(
                    name, component_name, slot, name_of(material), flag))
                if flag is not True:
                    fail("{0}.{1} slot {2} wears {3}, which has no SkeletalMesh usage flag; "
                         "it will render as the grey default".format(
                             name, component_name, slot, name_of(material)))


def main():
    say("==== verifying starter content ====")
    check_existence()
    check_input()
    check_blueprints()
    check_world_blueprints()
    check_thug_presentation()
    check_third_person()
    check_kate()
    check_skeletal_material_usage()
    check_weapon_data()
    check_data_assets()
    if PROBLEMS:
        unreal.log_error("[Verify] FAIL: {0} problem(s)".format(len(PROBLEMS)))
        for problem in PROBLEMS:
            unreal.log_error("[Verify]   " + problem)
    else:
        say("==== PASS: every expected asset is present and populated ====")


main()

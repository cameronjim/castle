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
THUG_MATERIAL_PATH = "/Game/Characters/Thug"
WEAPON_PATH = "/Game/Blueprints/Weapons"
KATE_MATERIAL_PATH = "/Game/Characters/Kate"

IA_NAMES = [
    "IA_Move", "IA_Look", "IA_Jump", "IA_Sprint", "IA_Crouch", "IA_Fire",
    "IA_Aim", "IA_Reload", "IA_Takedown", "IA_Interact", "IA_Pause", "IA_Skip",
    "IA_Slot1", "IA_Slot2", "IA_Slot3", "IA_SlotScroll", "IA_Inventory",
]

CHARACTER_INPUT_PROPS = [
    "default_mapping_context", "move_action", "look_action", "jump_action", "sprint_action",
    "crouch_action", "fire_action", "aim_action", "reload_action", "takedown_action",
    "interact_action", "slot1_action", "slot2_action", "slot3_action",
    "slot_scroll_action", "inventory_action",
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
        c.asset_path(PLAYER_PATH, "BP_CastleCharacter"),
        c.asset_path(PLAYER_PATH, "BP_Kate"),
        KATE_MATERIAL_PATH + "/M_KateSuit",
        KATE_MATERIAL_PATH + "/M_KateSuitDark",
        c.asset_path(PLAYER_PATH, "BP_CastlePlayerController"),
        c.asset_path(PLAYER_PATH, "BP_CastleGameMode"),
        c.asset_path(UI_PATH, "WBP_Flashback"),
        c.asset_path(UI_PATH, "WBP_Hud"),
        c.asset_path(UI_PATH, "WBP_Pause"),
        c.asset_path(UI_PATH, "WBP_Settings"),
        c.asset_path(UI_PATH, "WBP_EndCard"),
        c.asset_path(UI_PATH, "WBP_Hotbar"),
        c.asset_path(UI_PATH, "WBP_Inventory"),
        c.asset_path(WORLD_PATH, "BP_Pickup_Keycard"),
        c.asset_path(WORLD_PATH, "BP_Door_Keycard"),
        c.asset_path(AI_PATH, "BP_Thug"),
    ]
    + [c.asset_path(IMAGE_PATH, "T_FB01_0{0}".format(i)) for i in range(1, 7)]
    + [
        c.asset_path(THUG_MATERIAL_PATH, "M_ThugBody"),
        c.asset_path(THUG_MATERIAL_PATH, "M_ThugVisor"),
    ]
    + [
        WEAPON_PATH + "/DA_Weapon_Hands",
        "/Game/Missions/DA_M01_CellBlockD",
        "/Game/Flashbacks/Definitions/DA_FB01_Sunday",
        "/Game/Maps/L_Sandbox",
        "/Game/Maps/L_M01_CellBlockD",
    ]
)

# Retired in the stage 2 pivot (Tools/Editor/pivot_cleanup.py). Any of these still on disk,
# or any redirector left under /Game, means the cleanup did not finish.
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
        fail(p + " still exists; run Tools/Editor/pivot_cleanup.py")
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
        if "BP_Kate" not in name_of(prop(cdo, "default_pawn_class")):
            fail("BP_CastleGameMode.default_pawn_class is not BP_Kate; you would play the base Blueprint")

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

    if door_class is not None:
        cdo = unreal.get_default_object(door_class)
        for component_name in ("frame_mesh", "door_mesh"):
            component = prop(cdo, component_name)
            material = None
            try:
                material = component.get_material(0) if component is not None else None
            except Exception:  # noqa: BLE001
                material = None
            say("  BP_Door_Keycard.{0} material = {1}".format(component_name, name_of(material)))
            if material is None or "M_SteelPainted" not in name_of(material):
                fail("BP_Door_Keycard.{0} has no M_SteelPainted material".format(component_name))

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


def check_pickup_parts():
    """The keycard pickup is built from Part components with real materials, not one grey cube."""
    say("---- pickup parts ----")

    for name, minimum in (("BP_Pickup_Keycard", 2),):
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


def check_thug_presentation():
    """The thug look (kept from the guards for now), a flashlight and the locomotion sequences."""
    say("---- thug look ----")

    cls = c.load_generated_class(AI_PATH, "BP_Thug")
    if cls is None:
        fail("BP_Thug_C")
        return

    cdo = unreal.get_default_object(cls)

    for field in ("idle_anim", "walk_anim"):
        value = prop(cdo, field)
        say("  BP_Thug.{0:<10} = {1}".format(field, name_of(value)))
        if value is None:
            fail("BP_Thug." + field + " is unset; the thug would T-pose")

    flashlight = prop(cdo, "flashlight")
    say("  BP_Thug.flashlight  = {0}".format(name_of(flashlight)))
    if flashlight is None:
        fail("BP_Thug has no flashlight component")

    component = prop(cdo, "mesh")
    if component is None:
        fail("BP_Thug has no mesh component")
        return

    say("  BP_Thug.Mesh.animation_mode = {0}".format(prop(component, "animation_mode")))
    for slot, wanted in ((0, "M_ThugBody"), (1, "M_ThugVisor")):
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

    cls = c.load_generated_class(PLAYER_PATH, "BP_CastleCharacter")
    if cls is None:
        fail("BP_CastleCharacter_C")
        return

    cdo = unreal.get_default_object(cls)

    body = prop(cdo, "mesh")
    body_asset = prop(body, "skeletal_mesh_asset") if body is not None else None
    say("  Mesh.skeletal_mesh_asset     = {0}".format(name_of(body_asset)))
    if body_asset is None:
        fail("BP_CastleCharacter.Mesh has no body mesh; run create_blueprints.py")
    if body is not None and prop(body, "owner_no_see"):
        fail("BP_CastleCharacter.Mesh is hidden from its owner; the player would be invisible")

    for field in ("idle_anim", "walk_anim", "run_anim", "fall_anim"):
        value = prop(cdo, field)
        say("  BP_CastleCharacter.{0:<9} = {1}".format(field, name_of(value)))
        if value is None:
            fail("BP_CastleCharacter." + field + " is unset; the body stands in a T-pose")

    boom = prop(cdo, "camera_boom")
    camera = prop(cdo, "follow_camera")
    say("  CameraBoom.target_arm_length = {0}".format(prop(boom, "target_arm_length") if boom else None))
    say("  CameraBoom.socket_offset     = {0}".format(prop(boom, "socket_offset") if boom else None))
    say("  FollowCamera                 = {0}".format(name_of(camera)))
    if boom is None:
        fail("BP_CastleCharacter has no CameraBoom")
    elif not prop(boom, "use_pawn_control_rotation"):
        fail("BP_CastleCharacter.CameraBoom does not follow the control rotation")
    if camera is None:
        fail("BP_CastleCharacter has no FollowCamera")

    for retired in ("arms_mesh", "weapon_mesh", "fatigues_material", "use_arms_mesh"):
        if prop(cdo, retired) is not None:
            fail("BP_CastleCharacter still has {0}; the viewmodel code is back".format(retired))

    movement = prop(cdo, "character_movement")
    say("  orient_rotation_to_movement  = {0}".format(prop(movement, "orient_rotation_to_movement")))
    if movement is not None and not prop(movement, "orient_rotation_to_movement"):
        fail("BP_CastleCharacter does not turn to face where it moves")


def check_kate():
    """BP_Kate: a BP_CastleCharacter descendant, 170 cm capsule, mannequin in the purple suit.

    With the Game Animation Sample imported her parent is SandboxCharacter_CMC (itself on
    BP_CastleCharacter); verify_gasp.py checks that chain and the AnimBP in detail.
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
    if not any(name in str(parent or "") for name in ("BP_CastleCharacter", "SandboxCharacter_CMC")):
        fail("BP_Kate's parent is {0}, expected SandboxCharacter_CMC or BP_CastleCharacter".format(parent))

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
            fail("BP_Kate." + field + " is unset; it should inherit from BP_CastleCharacter")

    inventory = prop(cdo, "inventory_component")
    hands = prop(inventory, "hands_definition") if inventory is not None else None
    say("  BP_Kate.Inventory.hands_definition = {0}".format(hands))
    if "DA_Weapon_Hands" not in str(hands or ""):
        fail("BP_Kate.InventoryComponent.hands_definition is not DA_Weapon_Hands")


def check_weapon_data():
    """DA_Weapon_Hands, and the one place it has to be wired into."""
    say("---- weapon data ----")

    expected = {
        "DA_Weapon_Hands": [("is_melee", True), ("damage", 15.0), ("melee_range", 120.0)],
    }

    for name, fields in expected.items():
        asset = c.load_or_none(c.asset_path(WEAPON_PATH, name))
        if asset is None:
            fail(name)
            continue
        say("  {0}: slot={1}".format(name, prop(asset, "slot")))
        for field, want in fields:
            got = prop(asset, field)
            say("    {0:<22} = {1}".format(field, got))
            if value_text(got) != str(want):
                fail("{0}.{1} is {2}, expected {3}".format(name, field, got, want))

    char_class = c.load_generated_class(PLAYER_PATH, "BP_CastleCharacter")
    if char_class is not None:
        inventory = prop(unreal.get_default_object(char_class), "inventory_component")
        hands = prop(inventory, "hands_definition") if inventory is not None else None
        say("  BP_CastleCharacter.Inventory.hands_definition = {0}".format(hands))
        if inventory is None:
            fail("BP_CastleCharacter has no InventoryComponent")
        elif "DA_Weapon_Hands" not in str(hands or ""):
            fail("BP_CastleCharacter.InventoryComponent.hands_definition is not DA_Weapon_Hands")


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
        ids = [str(prop(obj, "objective_id")) for obj in objectives]
        if len(objectives) != 3:
            fail("DA_M01_CellBlockD has {0} objectives, expected 3".format(len(objectives)))
        if "find_weapon" in ids:
            fail("DA_M01_CellBlockD still has find_weapon; nothing can complete it after the pivot")
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
    """The test map needs thugs with patrol points, a door, the keycard and a nav volume."""
    thugs = [a for a in actors if "BP_Thug" in c.class_name(type(a)) or "ThugCharacter" in c.class_name(type(a))]
    guards = [a for a in actors if "Guard" in c.class_name(type(a)) or "Guard" in a.get_name()
              or label_of(a).startswith("Guard_")]
    doors = [a for a in actors if "Door" in c.class_name(type(a)) and "Frame" not in c.class_name(type(a))]
    pickups = [a for a in actors if "Pickup" in c.class_name(type(a))]
    pistols = [a for a in pickups if "Pistol" in c.class_name(type(a)) or "Pistol" in label_of(a)]
    points = [a for a in actors if isinstance(a, unreal.TargetPoint)]
    nav = [a for a in actors if isinstance(a, unreal.NavMeshBoundsVolume)]

    say("    thugs: {0}, patrol points: {1}, pickups: {2}, doors: {3}, nav volumes: {4}".format(
        len(thugs), len(points), len(pickups), len(doors), len(nav)))

    for thug in thugs:
        assigned = list(prop(thug, "patrol_points") or [])
        loot = list(prop(thug, "drop_on_death") or [])
        say("      {0:<16} patrol={1} drops={2}".format(
            label_of(thug), len(assigned), ", ".join(c.class_name(x) for x in loot) or "-"))
        if len(assigned) < 2:
            fail("{0} has {1} patrol point(s), expected 2".format(label_of(thug), len(assigned)))
        if not label_of(thug).startswith("Thug_"):
            fail("{0} is not labelled Thug_*".format(label_of(thug)))
        for item in loot:
            if item is None or "Keycard" not in c.class_name(item):
                fail("{0} drops {1}; only the keycard is left after the pivot".format(
                    label_of(thug), c.class_name(item)))

    if len(thugs) != 5:
        fail("L_M01_CellBlockD has {0} thugs, expected 5".format(len(thugs)))
    if guards:
        fail("L_M01_CellBlockD still has guards: " + ", ".join(label_of(g) for g in guards))
    if len(points) < 10:
        fail("L_M01_CellBlockD has {0} ATargetPoints, expected at least 10".format(len(points)))
    if not doors:
        fail("L_M01_CellBlockD has no BP_Door_Keycard")
    if not pickups:
        fail("L_M01_CellBlockD has no keycard pickup")
    if pistols:
        fail("L_M01_CellBlockD still has a pistol pickup: " + ", ".join(label_of(p) for p in pistols))
    if not nav:
        fail("L_M01_CellBlockD has no NavMeshBoundsVolume; thugs cannot move")

    looters = [t for t in thugs if list(prop(t, "drop_on_death") or [])]
    if len(looters) != 1:
        fail("{0} thug(s) carry loot, expected exactly 1".format(len(looters)))


SKELETAL_MESH_COMPONENTS = (
    (AI_PATH, "BP_Thug", ("mesh",)),
    (PLAYER_PATH, "BP_CastleCharacter", ("mesh",)),
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
    say("==== verifying stage 1-2 starter content ====")
    check_existence()
    check_input()
    check_blueprints()
    check_world_blueprints()
    check_pickup_parts()
    check_thug_presentation()
    check_third_person()
    check_kate()
    check_skeletal_material_usage()
    check_weapon_data()
    check_data_assets()
    check_maps()
    if PROBLEMS:
        unreal.log_error("[Verify] FAIL: {0} problem(s)".format(len(PROBLEMS)))
        for problem in PROBLEMS:
            unreal.log_error("[Verify]   " + problem)
    else:
        say("==== PASS: every expected asset is present and populated ====")


main()

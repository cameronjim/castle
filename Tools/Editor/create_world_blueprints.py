"""Create the stage-2 world, AI and HUD Blueprints and wire their class defaults.

    /Game/Blueprints/UI/WBP_Hud                parent UCastleHudWidget, HotbarWidgetClass
    /Game/Blueprints/World/BP_Pickup_Keycard   parent APickupActor, Keycard "cellblock"
    /Game/Blueprints/World/BP_Door_Keycard     parent ADoorActor, locked on "cellblock"
    /Game/Blueprints/AI/BP_Thug                parent AThugCharacter, mannequin mesh in a red
                                               tracksuit (M_ThugTracksuit, M_ThugTrim), idle,
                                               walk and run clips, a bat for Bat thugs
    /Game/Blueprints/World/BP_GrappleAnchor    parent AGrappleAnchor, 40 cm dark steel cube
    /Game/Blueprints/World/BP_TraversableBlock parent the Game Animation Sample's
                                               LevelBlock_Traversable, level-style lookup off

Then:

    BP_CastlePlayerController.HudWidgetClass = WBP_Hud_C

Meshes are /Engine/BasicShapes/Cube scaled into shape, so nothing here needs art. Every step
is idempotent: an existing Blueprint is loaded and only missing defaults are set.
"""

import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _common as c  # noqa: E402
import _materials as m  # noqa: E402
import create_blueprints as cb  # noqa: E402

WORLD_PATH = "/Game/Blueprints/World"
AI_PATH = "/Game/Blueprints/AI"
UI_PATH = "/Game/Blueprints/UI"
PLAYER_PATH = "/Game/Blueprints/Player"

CUBE_PATH = "/Engine/BasicShapes/Cube.Cube"

# The UE4 mannequin, copied out of the engine's Standard/Mannequin feature pack. Its assets
# hard-reference /Game/Mannequin/..., so the folder keeps that name rather than moving under
# Content/Characters. Only the sequences are used: the pack's AnimBP does not compile in a
# headless editor, so thugs drive ThirdPersonIdle / ThirdPersonWalk directly.
MANNEQUIN_MESH_PATH = "/Game/Mannequin/Character/Mesh/SK_Mannequin"
MANNEQUIN_PHYSICS_ASSET_PATH = "/Game/Mannequin/Character/Mesh/SK_Mannequin_PhysicsAsset"
MANNEQUIN_IDLE_PATH = "/Game/Mannequin/Animations/ThirdPersonIdle"
MANNEQUIN_WALK_PATH = "/Game/Mannequin/Animations/ThirdPersonWalk"
MANNEQUIN_RUN_PATH = "/Game/Mannequin/Animations/ThirdPersonRun"

THUG_MATERIAL_PATH = "/Game/Characters/Thug"
M_THUG_BODY = THUG_MATERIAL_PATH + "/M_ThugBody"
M_THUG_VISOR = THUG_MATERIAL_PATH + "/M_ThugVisor"

# The Tracksuit Mafia look (docs/DESIGN.md, Cast). SK_Mannequin has two material slots: the body
# (head included) and the chest logo patch. So the ski mask and the side stripes are drawn by the
# body material from the mesh's own bind-pose (pre-skinned) position and normal, and the second
# slot is the light trim. Both carry the HitFlash and Telegraph parameters AThugCharacter pulses.
M_THUG_TRACKSUIT = THUG_MATERIAL_PATH + "/M_ThugTracksuit"
M_THUG_TRIM = THUG_MATERIAL_PATH + "/M_ThugTrim"
THUG_MATERIAL_VERSION = "tracksuit-1"     # bump to rebuild both graphs on the next run
THUG_VERSION_TAG = "CastleVersion"
TRACKSUIT_RED = (0.6, 0.05, 0.05)
TRACKSUIT_STRIPE = (0.85, 0.85, 0.85)
SKI_MASK = (0.012, 0.012, 0.012)
MASK_BOTTOM_CM = 152.0      # bind-pose height above the feet where the black ski mask starts
STRIPE_EDGE = 9.0           # outward normal x times lateral position, cm; above it is stripe
HIT_FLASH_COLOR = (1.0, 0.85, 0.7)
HIT_FLASH_INTENSITY = 6.0
TELEGRAPH_COLOR = (1.0, 0.04, 0.01)
TELEGRAPH_INTENSITY = 60.0

# The bat: the engine cylinder (100 cm tall, 100 across) scaled to 85 x 6 cm, in the right hand.
CYLINDER_PATH = "/Engine/BasicShapes/Cylinder"
BAT_SCALE = unreal.Vector(0.06, 0.06, 0.85)
BAT_LOCATION = unreal.Vector(0.0, 0.0, 0.0)
BAT_ROTATION = unreal.Rotator(0.0, 0.0, 0.0)

# The template's own offsets: the mesh hangs from the capsule centre and faces +X.
THUG_MESH_LOCATION = unreal.Vector(0.0, 0.0, -96.0)
THUG_MESH_ROTATION = unreal.Rotator(0.0, 0.0, -90.0)

# Doors are 100 wide x 220 tall (claude-docs/asset-conventions.md); the cube is 100 cm. Only the
# leaf has a mesh: the surround around this doorway is built by the room-art pass.
DOOR_LEAF_SCALE = unreal.Vector(0.1, 1.0, 2.2)


def mesh(path):
    return c.load_or_none(path.split(".")[0])


def set_component_mesh(bp, component_name, mesh_asset, scale, relative_location=None):
    """Set StaticMesh + scale on an inherited component through the Blueprint CDO.

    The CDO's component instances are the templates every spawned actor copies, so writing to
    them here is what a designer would do in the Blueprint's Components panel.
    """
    cdo = c.blueprint_cdo(bp)
    if cdo is None:
        return False

    component = None
    try:
        component = cdo.get_editor_property(component_name)
    except Exception:  # noqa: BLE001
        pass
    if component is None:
        unreal.log_warning(
            "[Castle] skipped   {0}.{1}: no such component".format(bp.get_name(), component_name)
        )
        return False

    wanted = [("relative_scale3d", scale)]
    if relative_location is not None:
        wanted.append(("relative_location", relative_location))

    # Only write what differs, so a re-run leaves the .uasset byte-identical.
    changed = []
    for prop, value in wanted:
        try:
            if component.get_editor_property(prop) == value:
                continue
        except Exception:  # noqa: BLE001 - set_props reports a missing property
            pass
        changed.append((prop, value))
    c.set_props(component, changed, bp.get_name() + "." + component_name)

    if mesh_asset is not None and component.get_editor_property("static_mesh") != mesh_asset:
        try:
            component.set_static_mesh(mesh_asset)
            changed.append(("static_mesh", mesh_asset))
        except Exception as exc:  # noqa: BLE001
            c.log_error("set_static_mesh " + bp.get_name(), exc)
    return bool(changed)


def _build_thug_body(material):
    """Riot kit: near-black, half rough, a touch of metal so the light catches the shoulders."""
    color = m.constant3(material, (0.02, 0.02, 0.02), -400, -200)
    m.connect_property(color, unreal.MaterialProperty.MP_BASE_COLOR)
    m.set_scalar_property(material, 0.6, unreal.MaterialProperty.MP_ROUGHNESS, -400, 0)
    m.set_scalar_property(material, 0.2, unreal.MaterialProperty.MP_METALLIC, -400, 150)


def _build_thug_visor(material):
    """Black with a red glowing strip, so a thug's face reads as a visor line in the dark."""
    color = m.constant3(material, (0.01, 0.01, 0.01), -700, -200)
    m.connect_property(color, unreal.MaterialProperty.MP_BASE_COLOR)
    m.set_scalar_property(material, 0.25, unreal.MaterialProperty.MP_ROUGHNESS, -700, 0)

    glow = m.constant3(material, (0.9, 0.05, 0.02), -700, 200)
    bright = m.multiply(material, glow, None, -400, 200, const_b=20.0)
    m.connect_property(bright, unreal.MaterialProperty.MP_EMISSIVE_COLOR)


def _scalar_param(material, name, x, y):
    return m.expr(material, "MaterialExpressionScalarParameter", x, y,
                  [("parameter_name", name), ("default_value", 0.0)], name)


def _pulse_emissive(material, head_mask, x=-700, y=500):
    """HitFlash (a warm white over everything) plus Telegraph (red, only where head_mask is 1)."""
    flash = _scalar_param(material, "HitFlash", x, y)
    flash_color = m.constant3(material, HIT_FLASH_COLOR, x, y + 150)
    flash_rgb = m.multiply(material, flash_color, flash, x + 250, y)
    flash_rgb = m.multiply(material, flash_rgb, None, x + 450, y, const_b=HIT_FLASH_INTENSITY)
    if head_mask is None:
        return flash_rgb
    tele = _scalar_param(material, "Telegraph", x, y + 300)
    tele_color = m.constant3(material, TELEGRAPH_COLOR, x, y + 450)
    tele_on = m.multiply(material, tele, head_mask, x + 250, y + 300)
    tele_rgb = m.multiply(material, tele_color, tele_on, x + 450, y + 300)
    tele_rgb = m.multiply(material, tele_rgb, None, x + 650, y + 300, const_b=TELEGRAPH_INTENSITY)
    return m.add(material, flash_rgb, tele_rgb, x + 850, y + 150)


def _build_thug_tracksuit(material):
    """Red tracksuit, light stripes down the outside of the legs and body, a black ski mask."""
    pos = m.expr(material, "MaterialExpressionPreSkinnedPosition", -1600, -300, None, "PreSkinnedPosition")
    nrm = m.expr(material, "MaterialExpressionPreSkinnedNormal", -1600, 0, None, "PreSkinnedNormal")
    z = m.component_mask(material, pos, b=True, x=-1400, y=-400)
    head = m.step(material, z, MASK_BOTTOM_CM, -1200, -400, sharpness=0.5)
    px = m.component_mask(material, pos, r=True, x=-1400, y=-200)
    nx = m.component_mask(material, nrm, r=True, x=-1400, y=0)
    outward = m.multiply(material, nx, px, -1200, -100)
    stripe = m.step(material, outward, STRIPE_EDGE, -1000, -100, sharpness=0.5)

    red = m.constant3(material, TRACKSUIT_RED, -700, -500)
    white = m.constant3(material, TRACKSUIT_STRIPE, -700, -350)
    mask = m.constant3(material, SKI_MASK, -700, -200)
    suit = m.lerp(material, red, white, stripe, -500, -400)
    base = m.lerp(material, suit, mask, head, -300, -300)
    m.connect_property(base, unreal.MaterialProperty.MP_BASE_COLOR)
    m.set_scalar_property(material, 0.45, unreal.MaterialProperty.MP_ROUGHNESS, -300, 0)
    m.connect_property(_pulse_emissive(material, head), unreal.MaterialProperty.MP_EMISSIVE_COLOR)


def _build_thug_trim(material):
    """The chest patch: the same light grey as the stripes, flashing with the body."""
    m.connect_property(m.constant3(material, TRACKSUIT_STRIPE, -700, -200),
                       unreal.MaterialProperty.MP_BASE_COLOR)
    m.set_scalar_property(material, 0.45, unreal.MaterialProperty.MP_ROUGHNESS, -700, 0)
    m.connect_property(_pulse_emissive(material, None), unreal.MaterialProperty.MP_EMISSIVE_COLOR)


def _ensure_versioned_material(full_path, build_fn):
    """ensure_material, rebuilt when its CastleVersion metadata is not THUG_MATERIAL_VERSION."""
    existing = c.load_or_none(full_path)
    current = None
    if existing is not None:
        current = unreal.EditorAssetLibrary.get_metadata_tag(existing, THUG_VERSION_TAG)
    rebuild = existing is not None and current != THUG_MATERIAL_VERSION
    material = m.ensure_material(full_path, build_fn, rebuild=rebuild, skeletal=True)
    if material is not None and current != THUG_MATERIAL_VERSION:
        unreal.EditorAssetLibrary.set_metadata_tag(material, THUG_VERSION_TAG, THUG_MATERIAL_VERSION)
        c.save(material)
    return material


def ensure_thug_materials():
    """M_ThugTracksuit and M_ThugTrim at /Game/Characters/Thug. Idempotent.

    Both go on SK_Mannequin, so both need bUsedWithSkeletalMesh; without it the thugs wore the
    grey engine default and the log filled with "missing usage flag SkeletalMesh!". The prison
    build's M_ThugBody and M_ThugVisor are left on disk, unused.
    """
    c.ensure_directory(THUG_MATERIAL_PATH)
    return {
        "body": _ensure_versioned_material(M_THUG_TRACKSUIT, _build_thug_tracksuit),
        "visor": _ensure_versioned_material(M_THUG_TRIM, _build_thug_trim),
    }


def set_component_material(component, slot, material, context):
    """component.set_material(slot, material) when it is not already that. Returns True if set."""
    if component is None or material is None:
        return False
    try:
        if component.get_material(slot) == material:
            return False
    except Exception:  # noqa: BLE001 - an empty slot reads back as None
        pass
    try:
        component.set_material(slot, material)
        c.log("updated", context, "slot {0} = {1}".format(slot, material.get_name()))
        return True
    except Exception as exc:  # noqa: BLE001
        c.log_error(context, exc)
        return False


def set_thug_materials(bp, materials):
    """Tracksuit on slot 0 (body and head), trim on slot 1 (the chest logo patch)."""
    cdo = c.blueprint_cdo(bp)
    component = None
    if cdo is not None:
        try:
            component = cdo.get_editor_property("mesh")
        except Exception:  # noqa: BLE001
            component = None
    if component is None:
        c.log("skipped", "BP_Thug.Mesh materials", "no inherited mesh component")
        return False

    changed = set_component_material(component, 0, materials.get("body"), "BP_Thug.Mesh")
    changed = set_component_material(component, 1, materials.get("visor"), "BP_Thug.Mesh") or changed
    return changed


M_ANCHOR_STEEL = m.MATERIALS_PATH + "/M_AnchorSteel"
ANCHOR_NAME = "BP_GrappleAnchor"


def _build_anchor_steel(material):
    """Dark, worn steel: reads as a fitting against the grey roofs without shouting."""
    color = m.constant3(material, (0.035, 0.037, 0.04), -400, -200)
    m.connect_property(color, unreal.MaterialProperty.MP_BASE_COLOR)
    m.set_scalar_property(material, 0.45, unreal.MaterialProperty.MP_ROUGHNESS, -400, 0)
    m.set_scalar_property(material, 0.9, unreal.MaterialProperty.MP_METALLIC, -400, 150)


def make_grapple_anchor():
    """BP_GrappleAnchor: AGrappleAnchor's 40 cm cube in dark steel. The C++ sets mesh and size;
    only the material is data. generate_city.py places it on the district's roofs."""
    parent = c.find_class("GrappleAnchor", "/Script/Castle.GrappleAnchor")
    bp, created = cb.make_blueprint(ANCHOR_NAME, WORLD_PATH, parent, ("BlueprintFactory",))
    if bp is None:
        return None
    if created:
        c.compile_blueprint(bp)
        c.save(bp)

    steel = m.ensure_material(M_ANCHOR_STEEL, _build_anchor_steel)
    cdo = c.blueprint_cdo(bp)
    component = None
    if cdo is not None:
        try:
            component = cdo.get_editor_property("mesh")
        except Exception:  # noqa: BLE001
            component = None
    if component is None:
        c.log("skipped", ANCHOR_NAME + ".Mesh", "no mesh component")
        return bp
    if set_component_material(component, 0, steel, ANCHOR_NAME + ".Mesh"):
        c.compile_blueprint(bp)
        c.save(bp)
    else:
        c.log("exists", c.asset_path(WORLD_PATH, ANCHOR_NAME), "dark steel already set")
    return bp


TRAVERSABLE_BLOCK_NAME = "BP_TraversableBlock"
TRAVERSABLE_PARENT = "/Game/Levels/LevelPrototyping/LevelBlock_Traversable.LevelBlock_Traversable_C"
# LevelBlock's construction script looks up the sample's LevelVisuals actor (sky, sun, fog and
# post process for the sample's own map) for its grid colours unless UseLevelVisualsColor is off,
# and renames the actor after its height unless AutoNameFromHeight is off. The district has no
# LevelVisuals and its labels are generated, so both are off here.
TRAVERSABLE_DEFAULTS = (("UseLevelVisualsColor", False), ("AutoNameFromHeight", False))


def make_traversable_block():
    """BP_TraversableBlock: the sample's traversable block for generate_city.py's roof-edge ledges
    and parkour test blocks. Skipped when the sample is not imported."""
    try:
        parent = unreal.load_class(None, TRAVERSABLE_PARENT)
    except Exception:  # noqa: BLE001 - the sample is optional
        parent = None
    if parent is None:
        c.log("skipped", c.asset_path(WORLD_PATH, TRAVERSABLE_BLOCK_NAME), "LevelBlock_Traversable not imported")
        return None
    bp, created = cb.make_blueprint(TRAVERSABLE_BLOCK_NAME, WORLD_PATH, parent, ("BlueprintFactory",))
    if bp is None:
        return None
    if created:
        c.compile_blueprint(bp)
    cdo = c.blueprint_cdo(bp)
    changed = created
    for prop, value in TRAVERSABLE_DEFAULTS:
        try:
            if cdo.get_editor_property(prop) != value:
                cdo.set_editor_property(prop, value)
                changed = True
        except Exception as exc:  # noqa: BLE001
            c.log_error(TRAVERSABLE_BLOCK_NAME + "." + prop, exc)
    if changed:
        c.compile_blueprint(bp)
        c.save(bp)
        c.log("updated", c.asset_path(WORLD_PATH, TRAVERSABLE_BLOCK_NAME), "level-style lookup and auto name off")
    return bp


def make_hud():
    widget_parent = c.find_class("CastleHudWidget", "/Script/Castle.CastleHudWidget")
    bp, _ = cb.make_blueprint("WBP_Hud", UI_PATH, widget_parent, ("WidgetBlueprintFactory",))
    if bp is not None:
        c.compile_blueprint(bp)
        c.save(bp, only_if_dirty=True)
        # The hotbar lives inside the HUD's own overlay, so the HUD is what holds its class.
        cb.apply_defaults(bp, "WBP_Hud", UI_PATH,
                          [("hotbar_widget_class", c.load_generated_class(UI_PATH, "WBP_Hotbar"))])
    return bp


def clear_component_mesh(bp, component_name, context):
    """Empty a component's static mesh. Used to retire the placeholder cube on the root."""
    cdo = c.blueprint_cdo(bp)
    component = None
    if cdo is not None:
        try:
            component = cdo.get_editor_property(component_name)
        except Exception:  # noqa: BLE001
            component = None
    if component is None:
        return False

    try:
        if component.get_editor_property("static_mesh") is None:
            return False
        component.set_static_mesh(None)
        c.log("updated", context, "placeholder cube removed")
        return True
    except Exception as exc:  # noqa: BLE001
        c.log_error(context, exc)
        return False


def set_pickup_part(bp, part_name, cube, size_cm, location, material, rotation=None):
    """Shape one Part component: the engine cube scaled to size_cm and moved into place.

    The cube is 100 cm on a side, so a scale of size/100 gives centimetres directly.
    """
    cdo = c.blueprint_cdo(bp)
    component = None
    if cdo is not None:
        try:
            component = cdo.get_editor_property(part_name)
        except Exception:  # noqa: BLE001
            component = None
    if component is None:
        c.log("skipped", bp.get_name() + "." + part_name, "no such component")
        return False

    context = bp.get_name() + "." + part_name
    scale = unreal.Vector(size_cm[0] / 100.0, size_cm[1] / 100.0, size_cm[2] / 100.0)

    changed = []
    wanted = [
        ("relative_scale3d", scale),
        ("relative_location", unreal.Vector(location[0], location[1], location[2])),
    ]
    if rotation is not None:
        wanted.append(("relative_rotation", unreal.Rotator(rotation[0], rotation[1], rotation[2])))

    for prop, value in wanted:
        try:
            if component.get_editor_property(prop) == value:
                continue
        except Exception:  # noqa: BLE001 - set_props reports a missing property
            pass
        if c.set_props(component, [(prop, value)], context):
            changed.append(prop)

    try:
        if cube is not None and component.get_editor_property("static_mesh") != cube:
            component.set_static_mesh(cube)
            changed.append("static_mesh")
    except Exception as exc:  # noqa: BLE001
        c.log_error(context + " static_mesh", exc)

    if set_component_material(component, 0, material, context):
        changed.append("material")

    return bool(changed)


def shape_keycard(bp, cube, materials):
    """A white card with a coloured stripe along one edge."""
    changed = set_pickup_part(
        bp, "part1", cube, (8.6, 5.4, 0.2), (0.0, 0.0, 0.0), materials.get("keycard"))
    changed = set_pickup_part(
        bp, "part2", cube, (8.6, 1.0, 0.05), (0.0, 1.8, 0.13), materials.get("stripe")) or changed
    return changed


def make_pickup(name, values, shape_fn):
    parent = c.find_class("PickupActor", "/Script/Castle.PickupActor")
    bp, _ = cb.make_blueprint(name, WORLD_PATH, parent, ("BlueprintFactory",))
    if bp is None:
        return None

    c.compile_blueprint(bp)
    c.save(bp, only_if_dirty=True)

    changed = bool(cb.apply_defaults(bp, name, WORLD_PATH, values))
    # The silhouette now comes from the Part components, so the root cube goes.
    changed = clear_component_mesh(bp, "mesh", name + ".Mesh") or changed
    changed = shape_fn(bp) or changed
    if changed:
        c.compile_blueprint(bp)
        c.save(bp)
    return bp


def make_door(door_material):
    parent = c.find_class("DoorActor", "/Script/Castle.DoorActor")
    bp, _ = cb.make_blueprint("BP_Door_Keycard", WORLD_PATH, parent, ("BlueprintFactory",))
    if bp is None:
        return None

    c.compile_blueprint(bp)
    c.save(bp, only_if_dirty=True)

    cb.apply_defaults(
        bp,
        "BP_Door_Keycard",
        WORLD_PATH,
        [
            ("locked", True),
            ("required_keycard_id", "cellblock"),
            ("completes_objective_id", "security_door"),
            ("slide_distance", 110.0),
        ],
    )

    cube = mesh(CUBE_PATH)
    # The leaf fills the 100x220 opening and slides sideways. The frame stays empty: a cube big
    # enough to read as a surround is also a cube that plugs the doorway and hides the leaf
    # behind it, and the room-art pass already builds a real steel frame out of jambs, a lintel
    # and a header around this opening.
    changed = clear_component_mesh(bp, "frame_mesh", "BP_Door_Keycard.FrameMesh")
    changed = set_component_mesh(bp, "door_mesh", cube, DOOR_LEAF_SCALE, unreal.Vector(0.0, 0.0, 110.0)) or changed

    cdo = c.blueprint_cdo(bp)
    if cdo is not None and door_material is not None:
        try:
            component = cdo.get_editor_property("door_mesh")
        except Exception:  # noqa: BLE001
            component = None
        changed = set_component_material(
            component, 0, door_material, "BP_Door_Keycard.door_mesh") or changed

    if changed:
        c.compile_blueprint(bp)
        c.save(bp)
    return bp


def make_thug():
    parent = c.find_class("ThugCharacter", "/Script/Castle.ThugCharacter")
    bp, _ = cb.make_blueprint("BP_Thug", AI_PATH, parent, ("BlueprintFactory",))
    if bp is None:
        return None

    c.compile_blueprint(bp)
    c.save(bp, only_if_dirty=True)

    # apply_defaults compares with ==, which is false for two handles to the same UClass, so
    # the controller class would be re-set (and the asset re-saved) on every run. Compare names.
    controller_class = c.find_class("ThugAIController", "/Script/Castle.ThugAIController")
    values = [("auto_possess_ai", unreal.AutoPossessAI.PLACED_IN_WORLD_OR_SPAWNED)]
    cdo = c.blueprint_cdo(bp)
    current = None
    if cdo is not None:
        try:
            current = cdo.get_editor_property("ai_controller_class")
        except Exception:  # noqa: BLE001
            current = None
    if c.class_name(current) != c.class_name(controller_class):
        values.insert(0, ("ai_controller_class", controller_class))

    changed = bool(cb.apply_defaults(bp, "BP_Thug", AI_PATH, values))

    cdo = c.blueprint_cdo(bp)
    if cdo is not None:
        try:
            capsule = cdo.get_editor_property("capsule_component")
            capsule.set_capsule_size(34.0, 96.0)
        except Exception as exc:  # noqa: BLE001
            unreal.log_warning("[Castle] skipped   BP_Thug capsule size ({0})".format(exc))
        try:
            movement = cdo.get_editor_property("character_movement")
            movement.set_editor_property("max_walk_speed", 300.0)
        except Exception as exc:  # noqa: BLE001
            unreal.log_warning("[Castle] skipped   BP_Thug walk speed ({0})".format(exc))

    # Idle, walk and run as plain sequences: AThugCharacter swaps between them in Tick, because
    # the pack's AnimBP does not compile headless and left every thug in a T-pose. Run is the
    # melee rush.
    changed = bool(cb.apply_defaults(
        bp, "BP_Thug", AI_PATH,
        [
            ("idle_anim", c.load_or_none(MANNEQUIN_IDLE_PATH)),
            ("walk_anim", c.load_or_none(MANNEQUIN_WALK_PATH)),
            ("run_anim", c.load_or_none(MANNEQUIN_RUN_PATH)),
            ("bat_mesh", c.load_or_none(CYLINDER_PATH)),
        ])) or changed
    changed = set_thug_bat(bp) or changed

    changed = set_thug_mesh(bp) or changed
    changed = set_thug_materials(bp, ensure_thug_materials()) or changed
    changed = remove_thug_body(bp) or changed
    if changed:
        c.compile_blueprint(bp)
        c.save(bp)
    return bp


def set_thug_bat(bp):
    """Shape the HeldWeapon component into a bat: scale and grip offset on the CDO template."""
    cdo = c.blueprint_cdo(bp)
    component = None
    if cdo is not None:
        try:
            component = cdo.get_editor_property("held_weapon_component")
        except Exception:  # noqa: BLE001
            component = None
    if component is None:
        c.log("skipped", "BP_Thug.HeldWeapon", "no held weapon component; build the module")
        return False
    changed = []
    for prop, value in (("relative_scale3d", BAT_SCALE), ("relative_location", BAT_LOCATION),
                        ("relative_rotation", BAT_ROTATION)):
        try:
            if component.get_editor_property(prop) == value:
                continue
        except Exception:  # noqa: BLE001
            pass
        if c.set_props(component, [(prop, value)], "BP_Thug.HeldWeapon"):
            changed.append(prop)
    if changed:
        c.log("updated", "BP_Thug.HeldWeapon", ", ".join(changed))
    return bool(changed)


def subobject_object(sds, handle, bp):
    """The UObject behind a subobject handle, across the spellings UE 5.x has used."""
    data = sds.k2_find_subobject_data_from_handle(handle)
    if data is None:
        return None
    lib = getattr(unreal, "SubobjectDataBlueprintFunctionLibrary", None)
    for getter, args in (
        (getattr(lib, "get_object_for_blueprint", None), (data, bp)),
        (getattr(lib, "get_object", None), (data,)),
    ):
        if getter is None:
            continue
        try:
            found = getter(*args)
            if found is not None:
                return found
        except Exception:  # noqa: BLE001 - try the next spelling
            continue
    return None


def find_subobject_handle(sds, bp, name):
    """Handle of the Blueprint component called ``name``, or None."""
    handles = sds.k2_gather_subobject_data_for_blueprint(bp)
    for handle in handles or []:
        found = subobject_object(sds, handle, bp)
        if found is not None and name in found.get_name():
            return handle, found
    return None, None


def set_mannequin_physics_asset():
    """Re-point SK_Mannequin at its physics asset.

    The mesh's PhysicsAsset reference does not survive the file copy out of the feature pack,
    and without it AThugCharacter::GoLimp refuses to ragdoll and only logs a warning.
    """
    mesh_asset = c.load_or_none(MANNEQUIN_MESH_PATH)
    physics_asset = c.load_or_none(MANNEQUIN_PHYSICS_ASSET_PATH)
    if mesh_asset is None or physics_asset is None:
        c.log("skipped", "SK_Mannequin.physics_asset", "mesh or physics asset not found")
        return False

    try:
        if mesh_asset.get_editor_property("physics_asset") == physics_asset:
            c.log("exists", "SK_Mannequin.physics_asset", "already SK_Mannequin_PhysicsAsset")
            return False
    except Exception:  # noqa: BLE001 - set_props reports a missing property
        pass

    if not c.set_props(mesh_asset, [("physics_asset", physics_asset)], "SK_Mannequin"):
        return False

    c.save(mesh_asset)
    c.log("updated", "SK_Mannequin.physics_asset", "SK_Mannequin_PhysicsAsset")
    return True


def set_thug_mesh(bp):
    """Point BP_Thug's inherited SkeletalMeshComponent at the mannequin.

    Written on the Blueprint CDO's component template, which is what every spawned thug
    copies - the same thing a designer does in the Components panel.
    """
    mesh_asset = c.load_or_none(MANNEQUIN_MESH_PATH)
    if mesh_asset is None:
        c.log("skipped", "BP_Thug.Mesh", MANNEQUIN_MESH_PATH + " not found")
        return False

    cdo = c.blueprint_cdo(bp)
    component = None
    if cdo is not None:
        try:
            component = cdo.get_editor_property("mesh")
        except Exception:  # noqa: BLE001
            component = None
    if component is None:
        c.log("skipped", "BP_Thug.Mesh", "no inherited mesh component")
        return False

    changed = []

    try:
        if component.get_editor_property("skeletal_mesh_asset") != mesh_asset:
            component.set_skeletal_mesh_asset(mesh_asset)
            changed.append("skeletal_mesh_asset")
    except Exception as exc:  # noqa: BLE001
        c.log_error("BP_Thug.Mesh skeletal_mesh_asset", exc)

    for prop, value in (
        ("relative_location", THUG_MESH_LOCATION),
        ("relative_rotation", THUG_MESH_ROTATION),
    ):
        try:
            if component.get_editor_property(prop) == value:
                continue
        except Exception:  # noqa: BLE001 - set_props reports a missing property
            pass
        if c.set_props(component, [(prop, value)], "BP_Thug.Mesh"):
            changed.append(prop)

    # No AnimBP: the mannequin pack's ThirdPerson_AnimBP does not compile in a headless editor,
    # so every thug drove nothing and stood in a T-pose. AThugCharacter plays IdleAnim and
    # WalkAnim on the single-node slot instead, which needs this mode set on the template.
    try:
        if component.get_editor_property("animation_mode") != unreal.AnimationMode.ANIMATION_SINGLE_NODE:
            if c.set_props(
                component,
                [("animation_mode", unreal.AnimationMode.ANIMATION_SINGLE_NODE)],
                "BP_Thug.Mesh",
            ):
                changed.append("animation_mode")
    except Exception as exc:  # noqa: BLE001
        c.log_error("BP_Thug.Mesh animation_mode", exc)

    if changed:
        c.log("updated", "BP_Thug.Mesh", ", ".join(changed))
        return True

    c.log("exists", "BP_Thug.Mesh", "mannequin already assigned")
    return False


def remove_thug_body(bp):
    """Delete the grey cylinder stand-in now that BP_Thug has a real skeletal mesh.

    The component was called GuardBody before the pivot, and an old save may still carry it.
    """
    getter = getattr(unreal, "get_engine_subsystem", None)
    if getter is None or not hasattr(unreal, "SubobjectDataSubsystem"):
        return False

    try:
        sds = getter(unreal.SubobjectDataSubsystem)
        handle, found = find_subobject_handle(sds, bp, "GuardBody")
        if handle is None or found is None:
            return False

        sds.delete_subobject(handle, handle, bp)
        c.log("updated", "BP_Thug.GuardBody", "removed; the mannequin replaces it")
        return True
    except Exception as exc:  # noqa: BLE001
        c.log_error("BP_Thug.ThugBody removal", exc)
        return False


def wire_hud_into_controller():
    bp = c.load_or_none(c.asset_path(PLAYER_PATH, "BP_CastlePlayerController"))
    if bp is None:
        c.log("skipped", "BP_CastlePlayerController.hud_widget_class", "Blueprint not found")
        return
    hud_class = c.load_generated_class(UI_PATH, "WBP_Hud")
    cb.apply_defaults(
        bp, "BP_CastlePlayerController", PLAYER_PATH, [("hud_widget_class", hud_class)]
    )


def run():
    for path in (WORLD_PATH, AI_PATH, UI_PATH):
        c.ensure_directory(path)

    cube = mesh(CUBE_PATH)

    make_hud()

    # The keycard's glowing stripe is an instance of the room-art pass's M_Emissive, and this
    # script runs first, so make sure the lamp materials exist before asking for the props.
    m.ensure_light_materials()
    prop_materials = m.ensure_prop_materials()
    surface_materials = m.ensure_surface_materials()

    make_pickup(
        "BP_Pickup_Keycard",
        [
            ("pickup_type", unreal.PickupType.KEYCARD),
            ("keycard_id", "cellblock"),
        ],
        lambda bp: shape_keycard(bp, cube, prop_materials),
    )

    make_door(surface_materials.get("steel"))
    set_mannequin_physics_asset()
    make_thug()
    make_grapple_anchor()
    make_traversable_block()
    wire_hud_into_controller()


if __name__ == "__main__":
    run()
    c.print_summary("world blueprints")

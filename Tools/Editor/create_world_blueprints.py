"""Create the stage-2 world, AI and HUD Blueprints and wire their class defaults.

    /Game/Blueprints/UI/WBP_Hud                parent UCastleHudWidget, HotbarWidgetClass
    /Game/Blueprints/World/BP_Pickup_Pistol    parent APickupActor, Weapon
    /Game/Blueprints/World/BP_Pickup_Keycard   parent APickupActor, Keycard "cellblock"
    /Game/Blueprints/World/BP_Door_Keycard     parent ADoorActor, locked on "cellblock"
    /Game/Blueprints/AI/BP_Guard               parent AGuardCharacter, mannequin mesh

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
# headless editor, so guards drive ThirdPersonIdle / ThirdPersonWalk directly.
MANNEQUIN_MESH_PATH = "/Game/Mannequin/Character/Mesh/SK_Mannequin"
MANNEQUIN_PHYSICS_ASSET_PATH = "/Game/Mannequin/Character/Mesh/SK_Mannequin_PhysicsAsset"
MANNEQUIN_IDLE_PATH = "/Game/Mannequin/Animations/ThirdPersonIdle"
MANNEQUIN_WALK_PATH = "/Game/Mannequin/Animations/ThirdPersonWalk"

GUARD_MATERIAL_PATH = "/Game/Characters/Guard"
M_GUARD_BODY = GUARD_MATERIAL_PATH + "/M_GuardBody"
M_GUARD_VISOR = GUARD_MATERIAL_PATH + "/M_GuardVisor"

# The template's own offsets: the mesh hangs from the capsule centre and faces +X.
GUARD_MESH_LOCATION = unreal.Vector(0.0, 0.0, -96.0)
GUARD_MESH_ROTATION = unreal.Rotator(0.0, 0.0, -90.0)

# Doors are 100 wide x 220 tall (claude-docs/asset-conventions.md); the cube is 100 cm.
DOOR_LEAF_SCALE = unreal.Vector(0.1, 1.0, 2.2)
DOOR_FRAME_SCALE = unreal.Vector(0.2, 1.4, 2.6)


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


def _build_guard_body(material):
    """Riot kit: near-black, half rough, a touch of metal so the light catches the shoulders."""
    color = m.constant3(material, (0.02, 0.02, 0.02), -400, -200)
    m.connect_property(color, unreal.MaterialProperty.MP_BASE_COLOR)
    m.set_scalar_property(material, 0.6, unreal.MaterialProperty.MP_ROUGHNESS, -400, 0)
    m.set_scalar_property(material, 0.2, unreal.MaterialProperty.MP_METALLIC, -400, 150)


def _build_guard_visor(material):
    """Black with a red glowing strip, so a guard's face reads as a visor line in the dark."""
    color = m.constant3(material, (0.01, 0.01, 0.01), -700, -200)
    m.connect_property(color, unreal.MaterialProperty.MP_BASE_COLOR)
    m.set_scalar_property(material, 0.25, unreal.MaterialProperty.MP_ROUGHNESS, -700, 0)

    glow = m.constant3(material, (0.9, 0.05, 0.02), -700, 200)
    bright = m.multiply(material, glow, None, -400, 200, const_b=20.0)
    m.connect_property(bright, unreal.MaterialProperty.MP_EMISSIVE_COLOR)


def ensure_guard_materials():
    """M_GuardBody and M_GuardVisor at /Game/Characters/Guard. Idempotent."""
    c.ensure_directory(GUARD_MATERIAL_PATH)
    return {
        "body": m.ensure_material(M_GUARD_BODY, _build_guard_body),
        "visor": m.ensure_material(M_GUARD_VISOR, _build_guard_visor),
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


def set_guard_materials(bp, materials):
    """Body on slot 0, visor on slot 1.

    The UE4 mannequin has two slots and the head shares the body slot, so the emissive goes on
    slot 1 (the chest logo patch) rather than on a face that does not exist as its own slot.
    """
    cdo = c.blueprint_cdo(bp)
    component = None
    if cdo is not None:
        try:
            component = cdo.get_editor_property("mesh")
        except Exception:  # noqa: BLE001
            component = None
    if component is None:
        c.log("skipped", "BP_Guard.Mesh materials", "no inherited mesh component")
        return False

    changed = set_component_material(component, 0, materials.get("body"), "BP_Guard.Mesh")
    changed = set_component_material(component, 1, materials.get("visor"), "BP_Guard.Mesh") or changed
    return changed


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


def shape_pistol(bp, cube, materials):
    """Slide, frame, grip and trigger guard, all in gun-metal. Sizes are centimetres."""
    pistol = materials.get("pistol")
    parts = [
        ("part1", (18.0, 3.0, 3.0), (0.0, 0.0, 4.0), None),
        ("part2", (12.0, 3.0, 4.0), (-1.0, 0.0, 0.5), None),
        ("part3", (3.0, 3.0, 9.0), (-5.0, 0.0, -4.0), (15.0, 0.0, 0.0)),
        ("part4", (4.0, 2.5, 1.0), (-2.0, 0.0, -2.0), None),
    ]
    changed = False
    for part_name, size, location, rotation in parts:
        changed = set_pickup_part(bp, part_name, cube, size, location, pistol, rotation) or changed
    return changed


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
    # Frame sits in the wall; the leaf fills the 100x220 opening and slides sideways.
    changed = set_component_mesh(bp, "frame_mesh", cube, DOOR_FRAME_SCALE, unreal.Vector(0.0, 0.0, 130.0))
    changed = set_component_mesh(bp, "door_mesh", cube, DOOR_LEAF_SCALE, unreal.Vector(0.0, 0.0, 110.0)) or changed

    cdo = c.blueprint_cdo(bp)
    if cdo is not None and door_material is not None:
        for component_name in ("frame_mesh", "door_mesh"):
            try:
                component = cdo.get_editor_property(component_name)
            except Exception:  # noqa: BLE001
                component = None
            changed = set_component_material(
                component, 0, door_material, "BP_Door_Keycard." + component_name
            ) or changed

    if changed:
        c.compile_blueprint(bp)
        c.save(bp)
    return bp


def make_guard():
    parent = c.find_class("GuardCharacter", "/Script/Castle.GuardCharacter")
    bp, _ = cb.make_blueprint("BP_Guard", AI_PATH, parent, ("BlueprintFactory",))
    if bp is None:
        return None

    c.compile_blueprint(bp)
    c.save(bp, only_if_dirty=True)

    # apply_defaults compares with ==, which is false for two handles to the same UClass, so
    # the controller class would be re-set (and the asset re-saved) on every run. Compare names.
    controller_class = c.find_class("GuardAIController", "/Script/Castle.GuardAIController")
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

    changed = bool(cb.apply_defaults(bp, "BP_Guard", AI_PATH, values))

    cdo = c.blueprint_cdo(bp)
    if cdo is not None:
        try:
            capsule = cdo.get_editor_property("capsule_component")
            capsule.set_capsule_size(34.0, 96.0)
        except Exception as exc:  # noqa: BLE001
            unreal.log_warning("[Castle] skipped   BP_Guard capsule size ({0})".format(exc))
        try:
            movement = cdo.get_editor_property("character_movement")
            movement.set_editor_property("max_walk_speed", 300.0)
        except Exception as exc:  # noqa: BLE001
            unreal.log_warning("[Castle] skipped   BP_Guard walk speed ({0})".format(exc))

    # Idle and walk as plain sequences: AGuardCharacter swaps between them in Tick, because
    # the pack's AnimBP does not compile headless and left every guard in a T-pose.
    changed = bool(cb.apply_defaults(
        bp, "BP_Guard", AI_PATH,
        [
            ("idle_anim", c.load_or_none(MANNEQUIN_IDLE_PATH)),
            ("walk_anim", c.load_or_none(MANNEQUIN_WALK_PATH)),
        ])) or changed

    changed = set_guard_mesh(bp) or changed
    changed = set_guard_materials(bp, ensure_guard_materials()) or changed
    changed = remove_guard_body(bp) or changed
    if changed:
        c.compile_blueprint(bp)
        c.save(bp)
    return bp


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
    and without it AGuardCharacter::GoLimp refuses to ragdoll and only logs a warning.
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


def set_guard_mesh(bp):
    """Point BP_Guard's inherited SkeletalMeshComponent at the mannequin.

    Written on the Blueprint CDO's component template, which is what every spawned guard
    copies - the same thing a designer does in the Components panel.
    """
    mesh_asset = c.load_or_none(MANNEQUIN_MESH_PATH)
    if mesh_asset is None:
        c.log("skipped", "BP_Guard.Mesh", MANNEQUIN_MESH_PATH + " not found")
        return False

    cdo = c.blueprint_cdo(bp)
    component = None
    if cdo is not None:
        try:
            component = cdo.get_editor_property("mesh")
        except Exception:  # noqa: BLE001
            component = None
    if component is None:
        c.log("skipped", "BP_Guard.Mesh", "no inherited mesh component")
        return False

    changed = []

    try:
        if component.get_editor_property("skeletal_mesh_asset") != mesh_asset:
            component.set_skeletal_mesh_asset(mesh_asset)
            changed.append("skeletal_mesh_asset")
    except Exception as exc:  # noqa: BLE001
        c.log_error("BP_Guard.Mesh skeletal_mesh_asset", exc)

    for prop, value in (
        ("relative_location", GUARD_MESH_LOCATION),
        ("relative_rotation", GUARD_MESH_ROTATION),
    ):
        try:
            if component.get_editor_property(prop) == value:
                continue
        except Exception:  # noqa: BLE001 - set_props reports a missing property
            pass
        if c.set_props(component, [(prop, value)], "BP_Guard.Mesh"):
            changed.append(prop)

    # No AnimBP: the mannequin pack's ThirdPerson_AnimBP does not compile in a headless editor,
    # so every guard drove nothing and stood in a T-pose. AGuardCharacter plays IdleAnim and
    # WalkAnim on the single-node slot instead, which needs this mode set on the template.
    try:
        if component.get_editor_property("animation_mode") != unreal.AnimationMode.ANIMATION_SINGLE_NODE:
            if c.set_props(
                component,
                [("animation_mode", unreal.AnimationMode.ANIMATION_SINGLE_NODE)],
                "BP_Guard.Mesh",
            ):
                changed.append("animation_mode")
    except Exception as exc:  # noqa: BLE001
        c.log_error("BP_Guard.Mesh animation_mode", exc)

    if changed:
        c.log("updated", "BP_Guard.Mesh", ", ".join(changed))
        return True

    c.log("exists", "BP_Guard.Mesh", "mannequin already assigned")
    return False


def remove_guard_body(bp):
    """Delete the grey cylinder stand-in now that BP_Guard has a real skeletal mesh."""
    getter = getattr(unreal, "get_engine_subsystem", None)
    if getter is None or not hasattr(unreal, "SubobjectDataSubsystem"):
        return False

    try:
        sds = getter(unreal.SubobjectDataSubsystem)
        handle, found = find_subobject_handle(sds, bp, "GuardBody")
        if handle is None or found is None:
            return False

        sds.delete_subobject(handle, handle, bp)
        c.log("updated", "BP_Guard.GuardBody", "removed; the mannequin replaces it")
        return True
    except Exception as exc:  # noqa: BLE001
        c.log_error("BP_Guard.GuardBody removal", exc)
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
        "BP_Pickup_Pistol",
        [
            ("pickup_type", unreal.PickupType.WEAPON),
            ("magazine_amount", 12),
            ("ammo_amount", 24),
            ("completes_objective_id", "find_weapon"),
        ],
        lambda bp: shape_pistol(bp, cube, prop_materials),
    )

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
    make_guard()
    wire_hud_into_controller()


if __name__ == "__main__":
    run()
    c.print_summary("world blueprints")

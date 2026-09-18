"""Create the stage-2 world, AI and HUD Blueprints and wire their class defaults.

    /Game/Blueprints/UI/WBP_Hud                parent UCastleHudWidget
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
import create_blueprints as cb  # noqa: E402

WORLD_PATH = "/Game/Blueprints/World"
AI_PATH = "/Game/Blueprints/AI"
UI_PATH = "/Game/Blueprints/UI"
PLAYER_PATH = "/Game/Blueprints/Player"

CUBE_PATH = "/Engine/BasicShapes/Cube.Cube"

# The UE4 mannequin, copied out of the engine's Standard/Mannequin feature pack. Its assets
# hard-reference /Game/Mannequin/..., so the folder keeps that name rather than moving under
# Content/Characters. The AnimBP is a plain Blueprint over the same skeleton: idle, walk, run
# and jump, with no template C++ behind it.
MANNEQUIN_MESH_PATH = "/Game/Mannequin/Character/Mesh/SK_Mannequin"
MANNEQUIN_PHYSICS_ASSET_PATH = "/Game/Mannequin/Character/Mesh/SK_Mannequin_PhysicsAsset"
MANNEQUIN_ANIM_BP_PATH = "/Game/Mannequin/Animations/ThirdPerson_AnimBP"

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


def make_hud():
    widget_parent = c.find_class("CastleHudWidget", "/Script/Castle.CastleHudWidget")
    bp, _ = cb.make_blueprint("WBP_Hud", UI_PATH, widget_parent, ("WidgetBlueprintFactory",))
    if bp is not None:
        c.compile_blueprint(bp)
        c.save(bp, only_if_dirty=True)
    return bp


def make_pickup(name, values, scale, mesh_asset):
    parent = c.find_class("PickupActor", "/Script/Castle.PickupActor")
    bp, _ = cb.make_blueprint(name, WORLD_PATH, parent, ("BlueprintFactory",))
    if bp is None:
        return None

    c.compile_blueprint(bp)
    c.save(bp, only_if_dirty=True)

    changed = bool(cb.apply_defaults(bp, name, WORLD_PATH, values))
    changed = set_component_mesh(bp, "mesh", mesh_asset, scale) or changed
    if changed:
        c.compile_blueprint(bp)
        c.save(bp)
    return bp


def make_door():
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

    changed = set_guard_mesh(bp) or changed
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

    # The AnimBP is optional: a guard with none is a T-pose that still ragdolls correctly.
    anim_bp_class = None
    try:
        anim_bp_class = unreal.load_class(None, MANNEQUIN_ANIM_BP_PATH + "_C")
    except Exception:  # noqa: BLE001 - a missing AnimBP is not an error, just a T-pose
        anim_bp_class = None
    if anim_bp_class is None:
        c.log("skipped", "BP_Guard.Mesh", "ThirdPerson_AnimBP_C not found; guard stays in T-pose")
    else:
        current = None
        try:
            current = component.get_editor_property("anim_class")
        except Exception:  # noqa: BLE001
            current = None
        if c.class_name(current) != c.class_name(anim_bp_class):
            if c.set_props(
                component,
                [
                    ("animation_mode", unreal.AnimationMode.ANIMATION_BLUEPRINT),
                    ("anim_class", anim_bp_class),
                ],
                "BP_Guard.Mesh",
            ):
                changed.append("anim_class")

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

    make_pickup(
        "BP_Pickup_Pistol",
        [
            ("pickup_type", unreal.PickupType.WEAPON),
            ("magazine_amount", 12),
            ("ammo_amount", 24),
            ("completes_objective_id", "find_weapon"),
        ],
        unreal.Vector(0.3, 0.1, 0.2),
        cube,
    )

    make_pickup(
        "BP_Pickup_Keycard",
        [
            ("pickup_type", unreal.PickupType.KEYCARD),
            ("keycard_id", "cellblock"),
        ],
        unreal.Vector(0.12, 0.08, 0.02),
        cube,
    )

    make_door()
    set_mannequin_physics_asset()
    make_guard()
    wire_hud_into_controller()


if __name__ == "__main__":
    run()
    c.print_summary("world blueprints")

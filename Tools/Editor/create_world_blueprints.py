"""Create the stage-2 world, AI and HUD Blueprints and wire their class defaults.

    /Game/Blueprints/UI/WBP_Hud                parent UCastleHudWidget
    /Game/Blueprints/World/BP_Pickup_Pistol    parent APickupActor, Weapon
    /Game/Blueprints/World/BP_Pickup_Keycard   parent APickupActor, Keycard "cellblock"
    /Game/Blueprints/World/BP_Door_Keycard     parent ADoorActor, locked on "cellblock"
    /Game/Blueprints/AI/BP_Guard               parent AGuardCharacter

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
CYLINDER_PATH = "/Engine/BasicShapes/Cylinder.Cylinder"

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

    values = [("relative_scale3d", scale)]
    if relative_location is not None:
        values.append(("relative_location", relative_location))
    c.set_props(component, values, bp.get_name() + "." + component_name)

    if mesh_asset is not None:
        try:
            component.set_static_mesh(mesh_asset)
        except Exception as exc:  # noqa: BLE001
            c.log_error("set_static_mesh " + bp.get_name(), exc)
            return False
    return True


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

    cb.apply_defaults(bp, name, WORLD_PATH, values)
    set_component_mesh(bp, "mesh", mesh_asset, scale)
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
    set_component_mesh(bp, "frame_mesh", cube, DOOR_FRAME_SCALE, unreal.Vector(0.0, 0.0, 130.0))
    set_component_mesh(bp, "door_mesh", cube, DOOR_LEAF_SCALE, unreal.Vector(0.0, 0.0, 110.0))
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

    controller_class = c.find_class("GuardAIController", "/Script/Castle.GuardAIController")
    cb.apply_defaults(
        bp,
        "BP_Guard",
        AI_PATH,
        [
            ("ai_controller_class", controller_class),
            ("auto_possess_ai", unreal.AutoPossessAI.PLACED_IN_WORLD_OR_SPAWNED),
        ],
    )

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

    add_guard_body(bp)
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


def add_guard_body(bp):
    """Give the guard a visible grey cylinder, since there is no skeletal mesh yet.

    Added as a real Blueprint component (not just on the CDO) so it shows up in the editor and
    can be deleted by hand once BP_Guard gets a real character mesh.
    """
    getter = getattr(unreal, "get_engine_subsystem", None)
    if getter is None or not hasattr(unreal, "SubobjectDataSubsystem"):
        c.log("skipped", "BP_Guard.GuardBody", "SubobjectDataSubsystem unavailable")
        return False

    try:
        sds = getter(unreal.SubobjectDataSubsystem)
        handles = sds.k2_gather_subobject_data_for_blueprint(bp)
        if not handles:
            c.log("skipped", "BP_Guard.GuardBody", "no subobject data for the Blueprint")
            return False

        for handle in handles:
            found = subobject_object(sds, handle, bp)
            if found is not None and "GuardBody" in found.get_name():
                c.log("exists", "BP_Guard.GuardBody")
                return False

        params = unreal.AddNewSubobjectParams(
            parent_handle=handles[0],
            new_class=unreal.StaticMeshComponent,
            blueprint_context=bp,
        )
        handle, fail = sds.add_new_subobject(params)
        if fail and str(fail):
            c.log("skipped", "BP_Guard.GuardBody", str(fail))
            return False

        sds.rename_subobject(handle, unreal.Text("GuardBody"))

        component = subobject_object(sds, handle, bp)
        if component is None:
            c.log("skipped", "BP_Guard.GuardBody", "component added but not resolvable")
            return True

        cylinder = mesh(CYLINDER_PATH)
        values = [
            # 34 radius, 192 tall: the cylinder primitive is 100 across and 100 tall.
            ("relative_scale3d", unreal.Vector(0.68, 0.68, 1.92)),
            ("relative_location", unreal.Vector(0.0, 0.0, -96.0)),
        ]
        if cylinder is not None:
            values.append(("static_mesh", cylinder))
        c.set_props(component, values, "BP_Guard.GuardBody")
        try:
            component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        except Exception:  # noqa: BLE001 - cosmetic; the capsule owns collision
            pass

        c.log("created", "BP_Guard.GuardBody", "grey cylinder stand-in")
        return True
    except Exception as exc:  # noqa: BLE001
        c.log_error("BP_Guard.GuardBody", exc)
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
    make_guard()
    wire_hud_into_controller()


if __name__ == "__main__":
    run()
    c.print_summary("world blueprints")

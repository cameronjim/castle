"""Load the copied UE4 mannequin and print what it resolved to.

Read-only. Answers the one question that matters after copying .uasset files in by hand:
did every internal reference survive the move? The feature pack's assets hard-reference
/Game/Mannequin/..., so Content/Mannequin is where they have to live.

    UnrealEditor-Cmd.exe Castle.uproject -run=pythonscript ^
        -script="Tools\\Editor\\verify_mannequin.py" -unattended -nullrhi -nosplash -nop4 -stdout
"""

import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _common as c  # noqa: E402

MANNEQUIN_PATH = "/Game/Mannequin"
MESH_PATH = MANNEQUIN_PATH + "/Character/Mesh/SK_Mannequin"
SKELETON_PATH = MANNEQUIN_PATH + "/Character/Mesh/SK_Mannequin_Skeleton"
PHYSICS_PATH = MANNEQUIN_PATH + "/Character/Mesh/SK_Mannequin_PhysicsAsset"
ANIM_BP_PATH = MANNEQUIN_PATH + "/Animations/ThirdPerson_AnimBP"

GUARD_PATH = "/Game/Blueprints/AI"
PLAYER_PATH = "/Game/Blueprints/Player"

# The same mannequin does double duty as Frank's first-person arms: UE 5.8 ships no arms-only
# mesh, so the full body is hidden down to the arms and pushed under the camera.
ARMS_ANIM_PATHS = [
    MANNEQUIN_PATH + "/Animations/ThirdPersonIdle",
    MANNEQUIN_PATH + "/Animations/ThirdPersonWalk",
]

EXPECTED = [MESH_PATH, SKELETON_PATH, PHYSICS_PATH, ANIM_BP_PATH]

PROBLEMS = []


def say(line):
    unreal.log("[Mannequin] " + line)


def fail(line):
    PROBLEMS.append(line)
    unreal.log_warning("[Mannequin] PROBLEM " + line)


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


def check_mesh():
    say("---- skeletal mesh ----")
    for path in EXPECTED:
        if not c.exists(path):
            fail(path + " is missing")

    mesh = c.load_or_none(MESH_PATH)
    if mesh is None:
        fail(MESH_PATH + " would not load")
        return

    skeleton = prop(mesh, "skeleton")
    physics = prop(mesh, "physics_asset")

    say("  SK_Mannequin.skeleton      = {0}".format(name_of(skeleton)))
    say("  SK_Mannequin.physics_asset = {0}".format(name_of(physics)))

    # A broken reference shows up as None here, which is the whole point of the script.
    if skeleton is None:
        fail("SK_Mannequin has no skeleton; the reference did not survive the copy")
    if physics is None:
        fail("SK_Mannequin has no physics asset; guards cannot ragdoll")

    materials = list(prop(mesh, "materials") or [])
    say("  SK_Mannequin materials     = {0}".format(len(materials)))
    for index, slot in enumerate(materials):
        say("    slot {0}: {1}".format(index, name_of(prop(slot, "material_interface"))))
        if prop(slot, "material_interface") is None:
            fail("SK_Mannequin material slot {0} is empty".format(index))


def check_locomotion_anims():
    say("---- locomotion sequences ----")
    for path in ARMS_ANIM_PATHS:
        if c.exists(path):
            say("  {0} loads".format(path))
        else:
            fail(path + " is missing; guards and arms would have nothing to play")


def check_view_model():
    say("---- first-person arms ----")
    player_class = c.load_generated_class(PLAYER_PATH, "BP_CastleCharacter")
    if player_class is None:
        fail("BP_CastleCharacter_C would not load")
        return

    cdo = unreal.get_default_object(player_class)

    arms = prop(cdo, "arms_mesh")
    if arms is None:
        fail("BP_CastleCharacter has no ArmsMesh component")
    else:
        assigned = prop(arms, "skeletal_mesh_asset")
        say("  ArmsMesh.skeletal_mesh_asset = {0}".format(name_of(assigned)))
        say("  ArmsMesh.relative_location   = {0}".format(prop(arms, "relative_location")))
        if assigned is None:
            fail("BP_CastleCharacter.ArmsMesh has no mesh; run create_blueprints.py")

    weapon = prop(cdo, "weapon_mesh")
    if weapon is None:
        fail("BP_CastleCharacter has no WeaponMesh component")
    else:
        assigned = prop(weapon, "static_mesh")
        say("  WeaponMesh.static_mesh       = {0}".format(name_of(assigned)))
        if assigned is None:
            fail("BP_CastleCharacter.WeaponMesh has no pistol; run create_blueprints.py")

    say("  hidden bones                 = {0}".format(list(prop(cdo, "hidden_view_model_bones") or [])))


def check_guard_look():
    say("---- guard look ----")
    guard_class = c.load_generated_class(GUARD_PATH, "BP_Guard")
    if guard_class is None:
        return

    cdo = unreal.get_default_object(guard_class)
    for field in ("idle_anim", "walk_anim"):
        value = prop(cdo, field)
        say("  BP_Guard.{0:<10} = {1}".format(field, name_of(value)))
        if value is None:
            fail("BP_Guard." + field + " is unset; the guard stands in a T-pose")

    if prop(cdo, "flashlight") is None:
        fail("BP_Guard has no flashlight component")
    else:
        say("  BP_Guard.flashlight  = present")

    component = prop(cdo, "mesh")
    for slot in (0, 1):
        material = None
        try:
            material = component.get_material(slot) if component is not None else None
        except Exception:  # noqa: BLE001
            material = None
        say("  BP_Guard.Mesh slot {0} = {1}".format(slot, name_of(material)))
        if material is None:
            fail("BP_Guard.Mesh slot {0} has no material".format(slot))


def check_anim_bp():
    say("---- anim blueprint ----")
    try:
        anim_class = unreal.load_class(None, ANIM_BP_PATH + "_C")
    except Exception:  # noqa: BLE001
        anim_class = None

    if anim_class is None:
        say("  ThirdPerson_AnimBP_C did not load; unused, guards drive sequences directly")
        return

    say("  ThirdPerson_AnimBP_C loads ({0})".format(c.class_name(anim_class)))


def check_guard():
    say("---- BP_Guard ----")
    guard_class = c.load_generated_class(GUARD_PATH, "BP_Guard")
    if guard_class is None:
        fail("BP_Guard_C would not load")
        return

    cdo = unreal.get_default_object(guard_class)
    component = prop(cdo, "mesh")
    if component is None:
        fail("BP_Guard has no mesh component")
        return

    assigned = prop(component, "skeletal_mesh_asset")
    say("  BP_Guard.Mesh.skeletal_mesh_asset = {0}".format(name_of(assigned)))
    say("  BP_Guard.Mesh.relative_location   = {0}".format(prop(component, "relative_location")))
    say("  BP_Guard.Mesh.relative_rotation   = {0}".format(prop(component, "relative_rotation")))
    say("  BP_Guard.Mesh.anim_class          = {0}".format(c.class_name(prop(component, "anim_class"))))

    if assigned is None:
        fail("BP_Guard.Mesh has no skeletal mesh; run create_world_blueprints.py")


def main():
    say("==== verifying the copied mannequin ====")
    check_mesh()
    check_locomotion_anims()
    check_anim_bp()
    check_guard()
    check_guard_look()
    check_view_model()
    if PROBLEMS:
        unreal.log_error("[Mannequin] FAIL: {0} problem(s)".format(len(PROBLEMS)))
        for problem in PROBLEMS:
            unreal.log_error("[Mannequin]   " + problem)
    else:
        say("==== PASS: the mannequin loaded with its skeleton and physics asset ====")


main()

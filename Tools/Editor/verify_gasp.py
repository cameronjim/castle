"""Check the Game Animation Sample import: it loads, and BP_Kate is wired to it.

Read-only. Loads the sample's CharacterMovement character and AnimBP, the UEFN mannequin mesh,
skeleton and physics asset, the pose-search chooser, IMC_Sandbox and BP_Kate, then checks:

    BP_Kate -> SandboxCharacter_CMC -> BP_CastleCharacter          (the parent chain)
    BP_Kate.Mesh = SKM_UEFN_Mannequin, AnimBP SandboxCharacter_CMC_ABP, suit on slot 0
    IMC_Sandbox has no mappings                                    (Castle input owns every key)
    SandboxCharacter_CMC has CharacterInputState                   (what ACastleCharacter writes)

Load warnings ("Failed to load", "Can't find file") go to the log, not to Python; read them
from the log of this run:

    UnrealEditor-Cmd.exe Castle.uproject -run=pythonscript ^
        -script="Tools\\Editor\\verify_gasp.py" -unattended -nullrhi -nosplash -nop4 -stdout ^
        -abslog=Saved\\Logs\\CastleVerifyGasp.log
    Select-String Saved\\Logs\\CastleVerifyGasp.log -Pattern "Failed to load|Can't find file|\\[Gasp\\]"
"""

import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _common as c  # noqa: E402

PLAYER_PATH = "/Game/Blueprints/Player"
GASP_PATH = "/Game/Blueprints"

EXPECTED_ASSETS = [
    GASP_PATH + "/SandboxCharacter_CMC",
    GASP_PATH + "/SandboxCharacter_CMC_ABP",
    GASP_PATH + "/BPI_SandboxCharacter_Pawn",
    "/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin",
    "/Game/Characters/UEFN_Mannequin/Meshes/SK_UEFN_Mannequin",
    "/Game/Characters/UEFN_Mannequin/Animations/MotionMatchingData/CHT_PoseSearchDatabases",
    "/Game/Input/IMC_Sandbox",
]

PROBLEMS = []


def say(line):
    unreal.log("[Gasp] " + line)


def fail(line):
    PROBLEMS.append(line)
    unreal.log_error("[Gasp] FAIL " + line)


def name_of(obj):
    try:
        return obj.get_name() if obj is not None else "None"
    except Exception:  # noqa: BLE001
        return str(obj)


def check_assets():
    say("---- assets ----")
    for path in EXPECTED_ASSETS:
        asset = c.load_or_none(path)
        say("  {0:<80} {1}".format(path, name_of(asset.get_class()) if asset is not None else "MISSING"))
        if asset is None:
            fail(path + " does not load")

    mesh = c.load_or_none("/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin")
    if mesh is not None:
        skeleton = mesh.get_editor_property("skeleton")
        physics = mesh.get_editor_property("physics_asset")
        say("  SKM_UEFN_Mannequin skeleton = {0}, physics = {1}, materials = {2}".format(
            name_of(skeleton), name_of(physics), len(mesh.get_editor_property("materials") or [])))
        if skeleton is None:
            fail("SKM_UEFN_Mannequin has no skeleton")


def parent_chain(cls, depth=6):
    names = []
    while cls is not None and depth > 0:
        names.append(cls.get_name())
        try:
            cls = cls.get_super_class()
        except Exception:  # noqa: BLE001 - older API name
            cls = None
        depth -= 1
    return names


def check_kate():
    say("---- BP_Kate ----")
    bp = c.load_or_none(c.asset_path(PLAYER_PATH, "BP_Kate"))
    cls = c.load_generated_class(PLAYER_PATH, "BP_Kate")
    if bp is None or cls is None:
        fail("BP_Kate_C does not load")
        return

    kate_parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(bp)
    say("  BP_Kate parent = {0}".format(name_of(kate_parent)))
    if name_of(kate_parent) != "SandboxCharacter_CMC_C":
        fail("BP_Kate's parent is {0}, expected SandboxCharacter_CMC_C".format(name_of(kate_parent)))

    sandbox = c.load_or_none(GASP_PATH + "/SandboxCharacter_CMC")
    if sandbox is not None:
        sandbox_parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(sandbox)
        say("  SandboxCharacter_CMC parent = {0}".format(name_of(sandbox_parent)))
        if name_of(sandbox_parent) != "BP_CastleCharacter_C":
            fail("SandboxCharacter_CMC's parent is {0}, expected BP_CastleCharacter_C".format(name_of(sandbox_parent)))

    cdo = unreal.get_default_object(cls)
    body = cdo.get_editor_property("mesh")
    mesh = body.get_editor_property("skeletal_mesh_asset") if body is not None else None
    anim_class = body.get_editor_property("anim_class") if body is not None else None
    mode = body.get_editor_property("animation_mode") if body is not None else None
    say("  Mesh = {0}, anim class = {1}, mode = {2}".format(name_of(mesh), name_of(anim_class), mode))
    if name_of(mesh) != "SKM_UEFN_Mannequin":
        fail("BP_Kate.Mesh is {0}, expected SKM_UEFN_Mannequin".format(name_of(mesh)))
    if name_of(anim_class) != "SandboxCharacter_CMC_ABP_C":
        fail("BP_Kate.Mesh anim class is {0}, expected SandboxCharacter_CMC_ABP_C".format(name_of(anim_class)))
    if mode != unreal.AnimationMode.ANIMATION_BLUEPRINT:
        fail("BP_Kate.Mesh is not in AnimationBlueprint mode")
    for slot in range(4):
        try:
            material = body.get_material(slot)
        except Exception:  # noqa: BLE001
            break
        if material is None:
            continue
        say("  Mesh slot {0} = {1}".format(slot, name_of(material)))
    if body is not None and name_of(body.get_material(0)) != "M_KateSuit":
        fail("BP_Kate.Mesh slot 0 is not M_KateSuit")

    uses = False
    try:
        uses = bool(cdo.uses_gasp_locomotion())
    except Exception as exc:  # noqa: BLE001
        fail("uses_gasp_locomotion() not callable: {0}".format(exc))
    say("  uses_gasp_locomotion = {0}".format(uses))
    if not uses:
        fail("BP_Kate has no CharacterInputState; the GASP bridge would do nothing")

    try:
        names = []
        unreal.BlueprintEditorLibrary.list_member_variable_names(sandbox, names, False)
        say("  SandboxCharacter_CMC variables: " + ", ".join(sorted(names)))
    except Exception:  # noqa: BLE001 - optional detail
        pass


def check_imc():
    say("---- IMC_Sandbox ----")
    imc = c.load_or_none("/Game/Input/IMC_Sandbox")
    if imc is None:
        fail("IMC_Sandbox does not load")
        return
    count = len(imc.get_editor_property("default_key_mappings").get_editor_property("mappings"))
    say("  mappings = {0}".format(count))
    if count:
        fail("IMC_Sandbox still maps {0} key(s); run create-content".format(count))


def main():
    say("==== verifying the Game Animation Sample import ====")
    check_assets()
    check_kate()
    check_imc()
    if PROBLEMS:
        unreal.log_error("[Gasp] FAIL: {0} problem(s)".format(len(PROBLEMS)))
        for problem in PROBLEMS:
            unreal.log_error("[Gasp]   " + problem)
    else:
        say("==== PASS: the sample loads and BP_Kate runs on it ====")


main()

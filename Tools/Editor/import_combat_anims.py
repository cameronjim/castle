"""Turn downloaded combat clips into game animations (claude-docs/animation.md).

Reads Tools/Data/Anims/manifest.json. For every source it can find:

    mixamo_fbx       FBX files in a folder. The source's mesh_file (a clip downloaded With Skin)
                     imports as skeletal mesh plus its animation and defines the source skeleton;
                     every other file imports as an animation onto it. Under /Game/AnimSources/<Source>.
    uasset_project   .uasset clips in another project's Content folder (Paragon Sparrow in the Game
                     Animation Sample's). The clips, the source mesh and their hard-reference closure
                     are copied to the same /Game/ paths here (Tools/Editor/_packages.py reads UE4 and
                     UE5 packages), never overwriting.

then, per source:

    IK_<Source>                      an IK Rig on the source mesh with the standard chains (root,
                                     spine, neck, head, clavicles, arms, legs, toes, fingers when the
                                     bones exist) from the source's chain map in the manifest
    RTG_<Source>_To_<Target>         an IK Retargeter from it to each target rig: the engine's default
                                     op stack (pelvis, FK chains, IK chains, IK solve, root motion),
                                     chains mapped by name, the target's retarget pose auto-aligned to
                                     the source's, root motion copied from the source root (generated
                                     from the pelvis for Mixamo, which has no root bone)

and per clip, per target its characters need (uefn: Kate and Clint; thug: thugs and archers):

    <target output>/A_<Role>_<Variant>    the batch-retargeted sequence, root motion on for strike
                                          roles on the UEFN target, root locked everywhere else
    <target output>/AM_<Role>_<Variant>   its montage: slot, ANS_HitWindow, ANS_ComboWindow, a motion
                                          warping window, loop or hold, by role (ROLE_DEFAULTS),
                                          overridable per clip in the manifest

Targets: IK_UEFN_Mannequin (Epic's, copied from the sample if missing) on SKM_UEFN_Mannequin, and
IK_Thug_Mannequin, built here on the UE4 mannequin the thugs wear.

Idempotent: every asset that exists is kept unless --force; only what changed is saved; one log
line per clip. A missing source folder, file or asset is reported and skipped, never fatal.

Run headless (Tools\\import-anims.ps1 wraps this):

    UnrealEditor-Cmd.exe Hawkeye.uproject -run=pythonscript "-script=Tools\\Editor\\import_combat_anims.py --force"

Flags (in the -script string, or HAWKEYE_ANIM_ARGS):
    --force           re-import, rebuild rigs and retargeters, re-retarget and rebuild montages
    --only NAME       only this source
    --selftest        run the manifest's selftest block instead and assert on the results
    --keep            with --selftest, keep the SelfTest outputs (default: deleted afterwards)
"""

import hashlib
import json
import os
import re
import shutil
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _common as c  # noqa: E402
import _packages  # noqa: E402

SCRIPT_BUILD = "combat-anims-1"   # bump when the rig, retargeter or montage layout below changes
BUILD_TAG = "HawkeyeBuild"
CHARACTERS_TAG = "HawkeyeCharacters"
SOURCE_TAG = "HawkeyeSource"

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
MANIFEST = os.path.join(REPO, "Tools", "Data", "Anims", "manifest.json")
SOURCES_ROOT = "/Game/AnimSources"
GASP_CONTENT = os.environ.get("HAWKEYE_GASP_CONTENT", r"C:\Users\camer\code\GASP\Content")

ALL_CHARACTERS = ("kate", "clint", "thug", "archer")

# Per-role layout. Windows are fractions of the clip; warp runs from 0 to warp_end.
_STRIKE = {"slot": "DefaultSlot", "hit": [0.25, 0.45], "combo": [0.45, 0.8], "warp_end": 0.25, "root_motion": True}
ROLE_DEFAULTS = {
    "Light1": _STRIKE, "Light2": _STRIKE, "Light3": _STRIKE, "Kick": _STRIKE,
    "Heavy": {"slot": "DefaultSlot", "hit": [0.4, 0.55], "combo": [0.6, 0.85], "warp_end": 0.4, "root_motion": True},
    "Parry": {"slot": "DefaultSlot", "root_motion": False},
    "DodgeForward": {"slot": "DefaultSlot"}, "DodgeBack": {"slot": "DefaultSlot"},
    "DodgeLeft": {"slot": "DefaultSlot"}, "DodgeRight": {"slot": "DefaultSlot"},
    "HitFront": {"slot": "DefaultSlot"}, "HitBack": {"slot": "DefaultSlot"},
    "HitLeft": {"slot": "DefaultSlot"}, "HitRight": {"slot": "DefaultSlot"},
    "Knockdown": {"slot": "DefaultSlot", "hold": True},
    "GetUp": {"slot": "DefaultSlot"},
    "FinisherAttacker": {"slot": "DefaultSlot"}, "FinisherVictim": {"slot": "DefaultSlot"},
    "FinisherBow": {"slot": "DefaultSlot"},
    "BowDraw": {"slot": "UpperBody"}, "BowAimIdle": {"slot": "UpperBody", "loop": True},
    "BowFire": {"slot": "UpperBody"}, "BowNock": {"slot": "UpperBody"},
}

_MIXAMO_PREFIX = re.compile(r"^mixamorig\d*[:_]", re.IGNORECASE)

FORCE = False


# --------------------------------------------------------------------------------------------------
# arguments and manifest
# --------------------------------------------------------------------------------------------------


def parse_args(argv):
    args = {"force": False, "only": None, "selftest": False, "keep": False}
    words = list(argv) + os.environ.get("HAWKEYE_ANIM_ARGS", "").split()
    i = 0
    while i < len(words):
        word = words[i]
        if word == "--force":
            args["force"] = True
        elif word == "--selftest":
            args["selftest"] = True
        elif word == "--keep":
            args["keep"] = True
        elif word == "--only" and i + 1 < len(words):
            args["only"] = words[i + 1]
            i += 1
        i += 1
    return args


def load_manifest(path=MANIFEST):
    with open(path, encoding="utf-8") as handle:
        return json.load(handle)


def clip_characters(clip):
    wanted = clip.get("characters", "all")
    if isinstance(wanted, str):
        wanted = [wanted]
    if "all" in wanted:
        return list(ALL_CHARACTERS)
    return [w for w in wanted if w in ALL_CHARACTERS]


def targets_for(clip, targets):
    """(target name, characters on it) for every target one of the clip's characters uses."""
    chars = clip_characters(clip)
    out = []
    for name, target in targets.items():
        config = target.get("config", target)
        on = [ch for ch in chars if ch in config["characters"]]
        if on:
            out.append((name, on))
    return out


def clip_layout(clip):
    layout = dict(ROLE_DEFAULTS.get(clip["role"], {"slot": "DefaultSlot"}))
    for key in ("slot", "hit", "combo", "warp_end", "loop", "hold", "root_motion", "blend_in", "blend_out"):
        if key in clip:
            layout[key] = clip[key]
    return layout


def asset_names(clip):
    base = "{0}_{1}".format(clip["role"], re.sub(r"[^A-Za-z0-9]", "", clip.get("variant", "Clip")) or "Clip")
    return "A_" + base, "AM_" + base


def fingerprint(*parts):
    return hashlib.sha1(json.dumps(parts, sort_keys=True).encode("utf-8")).hexdigest()[:12]


# --------------------------------------------------------------------------------------------------
# small engine helpers
# --------------------------------------------------------------------------------------------------


def tag_of(asset, tag=BUILD_TAG):
    try:
        return unreal.EditorAssetLibrary.get_metadata_tag(asset, tag) or ""
    except Exception:  # noqa: BLE001
        return ""


def set_tag(asset, value, tag=BUILD_TAG):
    unreal.EditorAssetLibrary.set_metadata_tag(asset, tag, value)


def delete_if_exists(path):
    if c.exists(path):
        unreal.EditorAssetLibrary.delete_asset(path)


def skeleton_bones(skeleton):
    """Bone names of a skeleton, from its reference pose."""
    pose = unreal.AnimPoseExtensions.get_reference_pose(skeleton)
    return [str(b) for b in unreal.AnimPoseExtensions.get_bone_names(pose)]


def mesh_skeleton(mesh):
    return mesh.get_editor_property("skeleton") if mesh is not None else None


def resolve_bone(name, bones, lower_map):
    """name, or the bone that is name after a Mixamo prefix, or a case-insensitive match. None if absent."""
    if not name:
        return None
    if name in bones:
        return name
    low = name.lower()
    if low in lower_map:
        return lower_map[low]
    for bone in bones:
        if _MIXAMO_PREFIX.sub("", bone).lower() == low:
            return bone
    return None


def load_path(path):
    """The asset at path, through the registry, or straight from its package when the registry has
    not caught up with a file copied in this session."""
    asset = c.load_or_none(path)
    if asset is None:
        try:
            asset = unreal.load_asset(path)
        except Exception:  # noqa: BLE001 - a missing package is expected here
            asset = None
    return asset


def scan(paths):
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous(sorted(set(paths)), True)


# --------------------------------------------------------------------------------------------------
# IK rigs
# --------------------------------------------------------------------------------------------------


def ensure_ik_rig(path, mesh, chain_map_name, chain_maps):
    """An IK Rig at path on mesh with chain_map's chains. "auto" uses the engine's auto definition."""
    chain_map = chain_maps.get(chain_map_name) if chain_map_name != "auto" else None
    stamp = fingerprint(SCRIPT_BUILD, chain_map_name, chain_map, mesh.get_path_name())
    existing = c.load_or_none(path)
    if existing is not None and not FORCE and tag_of(existing) == stamp:
        c.log("exists", path)
        return existing
    folder, name = path.rsplit("/", 1)
    if existing is None:
        rig, _created = c.create_asset(name, folder, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory(), quiet=True)
    else:
        rig = existing
    if rig is None:
        c.log("FAILED", path, "could not create the IK Rig")
        return None
    ctl = unreal.IKRigController.get_controller(rig)
    ctl.set_skeletal_mesh(mesh)
    for chain in list(ctl.get_retarget_chains()):
        ctl.remove_retarget_chain(chain.get_editor_property("chain_name"))
    if chain_map is None:
        ok = ctl.apply_auto_generated_retarget_definition()
        made = [str(ch.get_editor_property("chain_name")) for ch in ctl.get_retarget_chains()]
        c.log("updated" if existing is not None else "created", path,
              "auto definition {0}: {1} chains, root {2}".format("ok" if ok else "FAILED", len(made), ctl.get_retarget_root()))
    else:
        bones = skeleton_bones(mesh_skeleton(mesh))
        lower = dict((b.lower(), b) for b in bones)
        root = resolve_bone(chain_map.get("retarget_root", "pelvis"), bones, lower)
        if root:
            ctl.set_retarget_root(root)
        made, skipped = [], []
        for chain_name, (start, end) in chain_map["chains"].items():
            start_bone = resolve_bone(start, bones, lower)
            end_bone = resolve_bone(end, bones, lower)
            if start_bone and end_bone:
                ctl.add_retarget_chain(chain_name, start_bone, end_bone, "")
                made.append(chain_name)
            else:
                skipped.append(chain_name)
        c.log("updated" if existing is not None else "created", path,
              "root {0}, {1} chains{2}".format(root, len(made), ", no bones for " + ", ".join(skipped) if skipped else ""))
    set_tag(rig, stamp)
    c.save(rig)
    return rig


def ensure_uefn_rig(target, chain_maps):
    """Epic's IK_UEFN_Mannequin, copied from the sample when the GASP import left it out."""
    path = target["ik_rig"]
    if not c.exists(path) and target.get("ik_rig_from_gasp"):
        source = os.path.join(GASP_CONTENT, target["ik_rig_from_gasp"].replace("/", os.sep))
        destination = os.path.join(c.project_dir(), "Content", path[len("/Game/"):].replace("/", os.sep) + ".uasset")
        if os.path.isfile(source) and not os.path.exists(destination):
            os.makedirs(os.path.dirname(destination), exist_ok=True)
            shutil.copy2(source, destination)
            scan([path.rsplit("/", 1)[0]])
            c.log("copied", path, "Epic's rig from the Game Animation Sample")
    rig = c.load_or_none(path)
    if rig is not None:
        c.log("exists", path, "target rig")
        return rig
    mesh = c.load_or_none(target["skeletal_mesh"])
    if mesh is None:
        c.log("skipped", path, "no {0} (run import_gasp first)".format(target["skeletal_mesh"]))
        return None
    return ensure_ik_rig(path, mesh, target["chains"], chain_maps)


def ensure_target(name, target, chain_maps):
    mesh = c.load_or_none(target["skeletal_mesh"])
    if mesh is None:
        c.log("skipped", "target " + name, "no " + target["skeletal_mesh"])
        return None
    if target.get("ik_rig_from_gasp"):
        rig = ensure_uefn_rig(target, chain_maps)
    else:
        rig = ensure_ik_rig(target["ik_rig"], mesh, target["chains"], chain_maps)
    if rig is None:
        return None
    return {"name": name, "mesh": mesh, "rig": rig, "config": target}


# --------------------------------------------------------------------------------------------------
# retargeters
# --------------------------------------------------------------------------------------------------


def _configure_root_motion(ctl, chain_map, target_mesh):
    """Root motion from the source root, or generated from the target pelvis when the source has none."""
    op_class = getattr(unreal, "IKRetargetRootMotionController", None)
    if op_class is None:
        return "no root motion op controller"
    source_root = (chain_map or {}).get("root_bone", "root")
    for index in range(ctl.get_num_retarget_ops()):
        op = ctl.get_op_controller(index)
        if not isinstance(op, op_class):
            continue
        target_bones = skeleton_bones(mesh_skeleton(target_mesh))
        op.set_target_root_bone("root" if "root" in target_bones else target_bones[0])
        op.set_target_pelvis_bone("pelvis" if "pelvis" in target_bones else target_bones[1])
        if source_root:
            op.set_source_root_bone(source_root)
            return "root motion copied from " + source_root
        settings = op.get_settings()
        source_enum = getattr(unreal, "RootMotionSource", None)
        if source_enum is not None and hasattr(source_enum, "GENERATE_FROM_TARGET_PELVIS"):
            settings.set_editor_property("root_motion_source", source_enum.GENERATE_FROM_TARGET_PELVIS)
            op.set_settings(settings)
            return "root motion generated from the target pelvis"
        return "root motion source enum not exposed; left at copy-from-source"
    return "no root motion op"


def ensure_retargeter(path, source_rig, source_mesh, source_chain_map, target):
    stamp = fingerprint(SCRIPT_BUILD, source_rig.get_path_name(), target["rig"].get_path_name(), tag_of(source_rig),
                        tag_of(target["rig"]))
    existing = c.load_or_none(path)
    if existing is not None and not FORCE and tag_of(existing) == stamp:
        c.log("exists", path)
        return existing
    folder, name = path.rsplit("/", 1)
    delete_if_exists(path)
    rtg, _created = c.create_asset(name, folder, unreal.IKRetargeter, unreal.IKRetargetFactory(), quiet=True)
    if rtg is None:
        c.log("FAILED", path, "could not create the IK Retargeter")
        return None
    side = unreal.RetargetSourceOrTarget
    ctl = unreal.IKRetargeterController.get_controller(rtg)
    ctl.set_ik_rig(side.SOURCE, source_rig)
    ctl.set_ik_rig(side.TARGET, target["rig"])
    ctl.set_preview_mesh(side.SOURCE, source_mesh)
    ctl.set_preview_mesh(side.TARGET, target["mesh"])
    ctl.add_default_ops()
    ctl.assign_ik_rig_to_all_ops(side.SOURCE, source_rig)
    ctl.assign_ik_rig_to_all_ops(side.TARGET, target["rig"])
    ctl.auto_map_chains(unreal.AutoMapChainType.EXACT, True)
    root_note = _configure_root_motion(ctl, source_chain_map, target["mesh"])
    # T-pose sources (Mixamo) against the A-pose mannequins: line the target's chains up with the
    # source's in a retarget pose of its own, so the arms are not rotated 45 degrees all clip long.
    pose = ctl.create_retarget_pose("HawkeyeAligned", side.TARGET)
    ctl.set_current_retarget_pose(pose, side.TARGET)
    ctl.auto_align_all_bones(side.TARGET, unreal.RetargetAutoAlignMethod.CHAIN_TO_CHAIN)
    mapped = 0
    for chain in unreal.IKRigController.get_controller(target["rig"]).get_retarget_chains():
        if str(ctl.get_source_chain(chain.get_editor_property("chain_name"))) not in ("", "None"):
            mapped += 1
    set_tag(rtg, stamp)
    c.save(rtg)
    c.log("created", path, "{0} target chains mapped, {1}".format(mapped, root_note))
    return rtg


# --------------------------------------------------------------------------------------------------
# sources
# --------------------------------------------------------------------------------------------------


def _import_task(filename, destination, name, options):
    task = unreal.AssetImportTask()
    task.filename = filename
    task.destination_path = destination
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    task.options = options
    c.asset_tools().import_asset_tasks([task])
    return [unreal.load_asset(p) for p in task.get_editor_property("imported_object_paths")]


def _anim_import_data(options):
    data = options.get_editor_property("anim_sequence_import_data")
    # Mixamo exports at 30 fps; a clip whose length is not a whole number of frames snaps to one
    # rather than failing the import.
    c.set_props(data, [("import_bone_tracks", True), ("remove_redundant_keys", False),
                       ("use_default_sample_rate", False), ("custom_sample_rate", 30),
                       ("snap_to_closest_frame_boundary", True)], "anim import data")
    return data


def _fbx_name(filename):
    return re.sub(r"[^A-Za-z0-9]", "", os.path.splitext(os.path.basename(filename))[0]) or "Clip"


def import_fbx_source(source, clips, folder):
    """Imports the mesh file (once) and each clip file onto its skeleton. {clip index: AnimSequence}."""
    base = SOURCES_ROOT + "/" + source["name"]
    mesh_file = source.get("mesh_file")
    mesh_path = os.path.join(folder, mesh_file) if mesh_file else None
    mesh_asset = base + "/SK_" + _fbx_name(mesh_file or "Mesh")
    mesh = c.load_or_none(mesh_asset)
    if mesh is None or FORCE:
        if not mesh_path or not os.path.isfile(mesh_path):
            c.log("missing", mesh_path or source["name"] + " mesh_file", "download one clip With Skin under this name")
            return None, {}
        options = unreal.FbxImportUI()
        # The mesh alone: one asset name for one asset. Its animation, if it is a clip too, comes in
        # below onto the skeleton like every other clip.
        c.set_props(options, [("import_mesh", True), ("import_as_skeletal", True), ("import_animations", False),
                              ("import_materials", False), ("import_textures", False), ("create_physics_asset", False),
                              ("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)], "fbx mesh import")
        _anim_import_data(options)
        imported = _import_task(mesh_path, base, "SK_" + _fbx_name(mesh_file), options)
        mesh = next((o for o in imported if isinstance(o, unreal.SkeletalMesh)), None)
        if mesh is None:
            c.log("FAILED", mesh_path, "no skeletal mesh in it: download it With Skin")
            return None, {}
        for obj in imported:
            c.save(obj)
        c.log("imported", mesh.get_path_name().split(".")[0], "skeleton {0}, bones: {1}".format(
            mesh_skeleton(mesh).get_name(), ", ".join(skeleton_bones(mesh_skeleton(mesh)))))
    else:
        c.log("exists", mesh_asset)
    skeleton = mesh_skeleton(mesh)

    found = {}
    for index, clip in clips:
        filename = os.path.join(folder, clip["file"])
        anim_path = base + "/A_Src_" + _fbx_name(clip["file"])
        anim = c.load_or_none(anim_path)
        if anim is not None and not FORCE:
            found[index] = anim
            continue
        if not os.path.isfile(filename):
            c.log("missing", filename, "not downloaded yet; skipped")
            continue
        options = unreal.FbxImportUI()
        c.set_props(options, [("import_mesh", False), ("import_as_skeletal", True), ("import_animations", True),
                              ("skeleton", skeleton), ("import_materials", False), ("import_textures", False),
                              ("mesh_type_to_import", unreal.FBXImportType.FBXIT_ANIMATION)], "fbx anim import")
        _anim_import_data(options)
        imported = [o for o in _import_task(filename, base, "A_Src_" + _fbx_name(clip["file"]), options)
                    if isinstance(o, unreal.AnimSequence)]
        if not imported:
            c.log("FAILED", filename, "imported no animation onto " + skeleton.get_name())
            continue
        c.save(imported[0])
        found[index] = imported[0]
        c.log("imported", imported[0].get_path_name().split(".")[0], "{0:.2f} s".format(
            unreal.AnimationLibrary.get_sequence_length(imported[0])))
    return mesh, found


def _assets_in(folder):
    out = []
    for path in unreal.EditorAssetLibrary.list_assets(folder, recursive=False, include_folder=False):
        asset = unreal.load_asset(path.split(".")[0])
        if asset is not None:
            out.append(asset)
    return out


def import_project_source(source, clips, content):
    """Copies the clips and the mesh (with their hard closure) from another Content folder. {index: anim}."""
    roots = [source["skeletal_mesh"]] + [clip["asset"] for _index, clip in clips]
    plan = _packages.plan(content, roots)
    for package in plan["missing"]:
        if package in roots:
            c.log("missing", package, "not in " + content)
    copied, present = _packages.copy(plan, os.path.join(c.project_dir(), "Content"))
    if copied:
        scan(set("/".join(p.split("/")[:-1]) for p in copied))

    c.log("copied" if copied else "exists", source["name"] + " packages",
          "{0} copied, {1} already here, closure {2:.0f} MB".format(len(copied), len(present), plan["bytes"] / 1e6))
    mesh = load_path(source["skeletal_mesh"])
    if mesh is None:
        c.log("FAILED", source["skeletal_mesh"], "the source mesh did not load")
        return None, {}
    found = {}
    for index, clip in clips:
        anim = load_path(clip["asset"])
        if isinstance(anim, unreal.AnimSequence):
            found[index] = anim
        elif anim is None:
            c.log("missing", clip["asset"], "copied but did not load" if clip["asset"] not in plan["missing"] else "not in " + content)
        else:
            c.log("skipped", clip["asset"], "a {0}, not an animation sequence".format(type(anim).__name__))
    return mesh, found


# --------------------------------------------------------------------------------------------------
# retarget and montage per clip
# --------------------------------------------------------------------------------------------------


def retarget_clip(anim, source_mesh, rtg, target, out_folder, seq_name):
    seq_path = out_folder + "/" + seq_name
    if c.exists(seq_path) and not FORCE:
        return c.load_or_none(seq_path), "exists"
    delete_if_exists(seq_path)
    c.ensure_directory(out_folder)
    inputs = unreal.IKRetargetBatchOperationInputs()
    c.set_props(inputs, [
        ("assets_to_retarget", [unreal.EditorAssetLibrary.find_asset_data(anim.get_path_name().split(".")[0])]),
        ("source_mesh", source_mesh), ("target_mesh", target["mesh"]), ("ik_retarget_asset", rtg),
        ("search", anim.get_name()), ("replace", seq_name), ("prefix", ""), ("suffix", ""),
        ("target_path", out_folder), ("use_source_path", False), ("include_referenced_assets", False),
        ("overwrite_existing_files", True)], "batch retarget")
    results = unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
    seq = c.load_or_none(seq_path)
    if seq is None:
        names = [str(r.get_editor_property("package_name")) for r in (results or [])]
        return None, "retarget produced {0}, not {1}".format(names or "nothing", seq_path)
    return seq, "retargeted"


def apply_root_motion(seq, layout, target):
    wanted = bool(layout.get("root_motion", False)) and bool(target["config"].get("root_motion", False))
    unreal.AnimationLibrary.set_root_motion_enabled(seq, wanted)
    # In place otherwise: the capsule, the dash or the lunge moves the character, not the clip.
    unreal.AnimationLibrary.set_is_root_motion_lock_forced(seq, not wanted)
    return wanted


def build_montage(seq, montage_path, layout, root_motion, characters, source_name):
    spec = unreal.HawkeyeCombatMontageSpec()
    hit = layout.get("hit") or [-1.0, -1.0]
    combo = layout.get("combo") or [-1.0, -1.0]
    c.set_props(spec, [
        ("slot_name", layout.get("slot", "DefaultSlot")),
        ("hit_start", float(hit[0])), ("hit_end", float(hit[1])),
        ("combo_start", float(combo[0])), ("combo_end", float(combo[1])),
        ("warp_end", float(layout.get("warp_end", -1.0)) if root_motion else -1.0),
        ("loop", bool(layout.get("loop", False))), ("hold_last_frame", bool(layout.get("hold", False))),
        ("blend_in_seconds", float(layout.get("blend_in", 0.1))), ("blend_out_seconds", float(layout.get("blend_out", 0.2))),
    ], "montage spec")
    stamp = fingerprint(SCRIPT_BUILD, layout, root_motion, seq.get_path_name())
    existing = c.load_or_none(montage_path)
    if existing is not None and not FORCE and tag_of(existing) == stamp:
        return existing, "exists"
    if existing is not None and FORCE:
        delete_if_exists(montage_path)
    montage = unreal.HawkeyeCombatMontageBuilder.build_combat_montage(seq, montage_path, spec)
    if montage is None:
        return None, "the montage builder refused (see LogHawkeye)"
    set_tag(montage, stamp)
    set_tag(montage, ",".join(characters), CHARACTERS_TAG)
    set_tag(montage, source_name, SOURCE_TAG)
    c.save(montage)
    skeleton = seq.get_editor_property("skeleton")
    if skeleton is not None:
        c.save(skeleton, only_if_dirty=True)
    return montage, "built"


def process_clip(clip, anim, source, source_mesh, retargeters, targets, output_override=None):
    """Retargets one clip to each target it needs and builds the montages. Returns the results."""
    seq_name, montage_name = asset_names(clip)
    layout = clip_layout(clip)
    results = []
    for target_name, characters in targets_for(clip, targets):
        target = targets[target_name]
        rtg = retargeters.get(target_name)
        out_folder = output_override(target) if output_override else target["config"]["output"]
        where = out_folder + "/" + seq_name
        if rtg is None:
            c.log("skipped", where, "no retargeter to " + target_name)
            continue
        seq, how = retarget_clip(anim, source_mesh, rtg, target, out_folder, seq_name)
        if seq is None:
            c.log("FAILED", where, how)
            continue
        root_motion = apply_root_motion(seq, layout, target)
        c.save(seq, only_if_dirty=True)
        montage, built = build_montage(seq, out_folder + "/" + montage_name, layout, root_motion, characters, source["name"])
        if montage is None:
            c.log("FAILED", out_folder + "/" + montage_name, built)
            continue
        c.log("created" if how == "retargeted" or built == "built" else "exists", out_folder + "/" + montage_name,
              "{0} for {1}: {2}".format(clip.get("file") or clip.get("asset", "").rsplit("/", 1)[-1], ",".join(characters),
                                        unreal.HawkeyeCombatMontageBuilder.describe_montage(montage)))
        results.append({"clip": clip, "anim": anim, "target": target_name, "sequence": seq, "montage": montage})
    return results


# --------------------------------------------------------------------------------------------------
# the run
# --------------------------------------------------------------------------------------------------


def source_folder(source):
    path = os.environ.get(source.get("path_env", ""), "") or source["path"]
    if not os.path.isabs(path):
        path = os.path.join(REPO, path.replace("/", os.sep))
    return path


def process_source(source, clips, targets, chain_maps, output_override=None):
    folder = source_folder(source)
    if not os.path.isdir(folder):
        c.log("missing", source["name"], "no folder at {0}; its {1} clip(s) skipped".format(folder, len(clips)))
        return []
    if source["kind"] == "mixamo_fbx":
        present = [clip for _i, clip in clips if os.path.isfile(os.path.join(folder, clip["file"]))]
        if not present:
            c.log("missing", source["name"], "none of its {0} file(s) are in {1} yet".format(len(clips), folder))
            return []
        mesh, anims = import_fbx_source(source, clips, folder)
    elif source["kind"] == "uasset_project":
        mesh, anims = import_project_source(source, clips, folder)
    else:
        c.log("FAILED", source["name"], "unknown kind " + source["kind"])
        return []
    if mesh is None:
        return []
    rig = ensure_ik_rig("{0}/{1}/IK_{1}".format(SOURCES_ROOT, source["name"]), mesh, source["chains"], chain_maps)
    if rig is None:
        return []
    needed = set(name for _i, clip in clips for name, _chars in targets_for(clip, targets))
    retargeters = {}
    for name in sorted(needed):
        retargeters[name] = ensure_retargeter("{0}/{1}/RTG_{1}_To_{2}".format(SOURCES_ROOT, source["name"], name.upper()),
                                              rig, mesh, chain_maps.get(source["chains"]), targets[name])
    results = []
    for index, clip in clips:
        anim = anims.get(index)
        if anim is None:
            continue
        results += process_clip(clip, anim, source, mesh, retargeters, targets, output_override)
    return results


def run(manifest=None, args=None, output_override=None):
    global FORCE
    manifest = manifest or load_manifest()
    args = args or {"force": False, "only": None}
    FORCE = bool(args.get("force"))
    chain_maps = manifest["chain_maps"]
    targets = {}
    for name, config in manifest["targets"].items():
        target = ensure_target(name, config, chain_maps)
        if target is not None:
            targets[name] = target
    sources = dict((s["name"], s) for s in manifest["sources"])
    results = []
    for name, source in sources.items():
        if args.get("only") and args["only"] != name:
            continue
        clips = [(i, clip) for i, clip in enumerate(manifest["clips"]) if clip["source"] == name]
        if not clips:
            continue
        try:
            results += process_source(source, clips, targets, chain_maps, output_override)
        except Exception as exc:  # noqa: BLE001 - one broken source must not stop the rest
            c.log_error("source " + name, exc)
    return results


# --------------------------------------------------------------------------------------------------
# self-test
# --------------------------------------------------------------------------------------------------


def _pose_at(anim, time):
    options = unreal.AnimPoseEvaluationOptions()
    return unreal.AnimPoseExtensions.get_anim_pose_at_time(anim, time, options)


def _bone(pose, name):
    return unreal.AnimPoseExtensions.get_bone_pose(pose, name, unreal.AnimPoseSpaces.WORLD).translation


def _dist(a, b):
    return ((a.x - b.x) ** 2 + (a.y - b.y) ** 2 + (a.z - b.z) ** 2) ** 0.5


def bow_pose_metrics(anim, time, names):
    """Component-space checks on a bow draw: arm extension, hands forward, above the pelvis."""
    pose = _pose_at(anim, time)
    ref = unreal.AnimPoseExtensions.get_reference_pose(anim.get_editor_property("skeleton"))
    at = dict((key, _bone(pose, bone)) for key, bone in names.items())
    ref_at = dict((key, unreal.AnimPoseExtensions.get_ref_bone_pose(ref, bone, unreal.AnimPoseSpaces.WORLD).translation)
                  for key, bone in names.items())
    arm = _dist(ref_at["upperarm_l"], ref_at["lowerarm_l"]) + _dist(ref_at["lowerarm_l"], ref_at["hand_l"])
    forward_axis = "y"   # the mannequins, Paragon heroes included, face their own +Y
    fwd = lambda v: getattr(v, forward_axis)  # noqa: E731
    return {
        "bow_arm_extension": _dist(at["upperarm_l"], at["hand_l"]) / max(arm, 1.0),
        "bow_hand_forward": fwd(at["hand_l"]) - fwd(at["pelvis"]),
        "string_hand_forward": fwd(at["hand_r"]) - fwd(at["pelvis"]),
        "bow_hand_height": at["hand_l"].z - at["pelvis"].z,
        "head_height": at["head"].z - at["pelvis"].z,
        "hands_apart": _dist(at["hand_l"], at["hand_r"]),
    }


class SelfTest(object):
    def __init__(self):
        self.failures = []
        self.checks = 0

    def check(self, ok, what):
        self.checks += 1
        c.log("PASS" if ok else "FAIL", "selftest", what)
        if not ok:
            self.failures.append(what)


def selftest_output(target):
    return target["config"]["output"] + "/SelfTest"


def run_selftest(args):
    manifest = load_manifest()
    block = manifest["selftest"]
    chain_maps = dict(manifest["chain_maps"])
    chain_maps.update(block.get("chain_maps", {}))
    test_manifest = {
        "targets": manifest["targets"],
        "chain_maps": chain_maps,
        "sources": block["sources"] + [s for s in manifest["sources"] if s["name"] == "ParagonSparrow"],
        "clips": block["clips"],
    }
    results = run(test_manifest, {"force": True, "only": args.get("only")}, output_override=selftest_output)
    test = SelfTest()
    expected = {}
    for clip in block["clips"]:
        for target_name, _chars in targets_for(clip, manifest["targets"]):
            expected[(asset_names(clip)[0], target_name)] = clip
    got = dict(((r["sequence"].get_name(), r["target"]), r) for r in results)
    for key, clip in sorted(expected.items()):
        source_ok = os.path.isdir(source_folder(next(s for s in test_manifest["sources"] if s["name"] == clip["source"])))
        if not source_ok:
            c.log("skipped", "selftest " + key[0], "no source folder for " + clip["source"])
            continue
        result = got.get(key)
        test.check(result is not None, "{0} on {1} was retargeted and has a montage".format(*key))
        if result is None:
            continue
        seq, anim, montage = result["sequence"], result["anim"], result["montage"]
        target_skeleton = manifest["targets"][key[1]]["skeletal_mesh"]
        skeleton = seq.get_editor_property("skeleton")
        mesh_skel = mesh_skeleton(c.load_or_none(target_skeleton))
        test.check(skeleton == mesh_skel, "{0}: on the target skeleton {1}".format(key[0], mesh_skel.get_name()))
        src_len = unreal.AnimationLibrary.get_sequence_length(anim)
        out_len = unreal.AnimationLibrary.get_sequence_length(seq)
        test.check(abs(src_len - out_len) <= 1.0 / 29.0,
                   "{0}: length {1:.3f} s matches the source's {2:.3f} s".format(key[0], out_len, src_len))
        tracks = [str(t) for t in unreal.AnimationLibrary.get_animation_track_names(seq)]
        bones = skeleton_bones(skeleton)
        wanted = [b for b in ("pelvis", "spine_01", "upperarm_l", "hand_r", "thigh_r") if b in bones]
        # Virtual bones ("vb hand_l_prop_01" on the UEFN mannequin) carry tracks without being in the hierarchy.
        strays = [t for t in tracks if t not in bones and not t.startswith("vb ")]
        test.check(len(tracks) > 0 and not strays, "{0}: {1} bone tracks, all on the target skeleton{2}".format(
            key[0], len(tracks), " (not on it: " + ", ".join(strays[:8]) + ")" if strays else ""))
        test.check(len(wanted) == 5, "{0}: target skeleton has pelvis, spine_01, upperarm_l, hand_r, thigh_r".format(key[0]))
        described = unreal.HawkeyeCombatMontageBuilder.describe_montage(montage)
        layout = clip_layout(clip)
        if layout.get("hit"):
            test.check("hit=-" not in described, "{0}: montage has its ANS_HitWindow ({1})".format(key[0], described))
        test.check("slot=" + layout.get("slot", "DefaultSlot") in described, "{0}: montage slot {1}".format(key[0], layout.get("slot")))
        if clip["role"] == "BowDraw":
            names = dict((k, k) for k in ("pelvis", "head", "upperarm_l", "lowerarm_l", "hand_l", "hand_r"))
            for when in (0.5, 1.0):
                src_m = bow_pose_metrics(anim, src_len * when, names)
                out_m = bow_pose_metrics(seq, out_len * when, names)
                c.log("info", "selftest bow pose at {0:.0%}".format(when), "source {0} | {1} {2}".format(
                    _fmt(src_m), key[1], _fmt(out_m)))
                test.check(out_m["head_height"] > 40.0, "{0} at {1:.0%}: head well above the pelvis".format(key[0], when))
                test.check(abs(out_m["bow_arm_extension"] - src_m["bow_arm_extension"]) < 0.15,
                           "{0} at {1:.0%}: bow arm extended as in the source ({2:.2f} vs {3:.2f})".format(
                               key[0], when, out_m["bow_arm_extension"], src_m["bow_arm_extension"]))
                test.check((out_m["bow_hand_forward"] > 0) == (src_m["bow_hand_forward"] > 0),
                           "{0} at {1:.0%}: bow hand on the same side of the body as the source's ({2:.0f} cm)".format(
                               key[0], when, out_m["bow_hand_forward"]))
                test.check(out_m["bow_hand_height"] > 0.0, "{0} at {1:.0%}: bow hand above the pelvis".format(key[0], when))
    if not args.get("keep"):
        for target in manifest["targets"].values():
            folder = target["output"] + "/SelfTest"
            if unreal.EditorAssetLibrary.does_directory_exist(folder):
                unreal.EditorAssetLibrary.delete_directory(folder)
        c.log("deleted", "selftest outputs", "SelfTest folders removed (--keep keeps them)")
    if test.failures:
        unreal.log_error("[Hawkeye] selftest FAILED: {0} of {1} checks".format(len(test.failures), test.checks))
        for failure in test.failures:
            unreal.log_error("[Hawkeye]   " + failure)
    else:
        unreal.log("[Hawkeye] selftest PASSED: {0} checks".format(test.checks))
    return test


def _fmt(metrics):
    return " ".join("{0}={1:.2f}".format(k, v) for k, v in sorted(metrics.items()))


def main(argv):
    args = parse_args(argv)
    c.reset_summary()
    if args["selftest"]:
        run_selftest(args)
        c.print_summary("combat anims selftest")
        return
    run(load_manifest(), args)
    c.print_summary("combat anims")


if __name__ == "__main__":
    main(sys.argv[1:])

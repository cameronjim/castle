"""Put both hands on the bow (claude-docs/gameplay-semantics.md, "bow and arrows").

    /Game/Blueprints/Animation/ABP_BowIK_Post        post-process AnimBP for the UEFN mannequin (Kate,
                                                     Clint), built headless by UHawkeyeBowIKGraphBuilder
                                                     (C++, anim graph nodes): the mannequin's own
                                                     ABP_UEFN_Mannequin_PostProcess as a linked graph,
                                                     then the full-body and upper-body clip slots, the spine
                                                     and neck turn and two-bone IK on both arms
    /Game/Blueprints/Animation/ABP_BowIK_Post_Thug   the same graph for the old mannequin (BP_Archer),
                                                     with nothing to chain

Then BowComponent.HandsIKClass on BP_Kate, BP_Clint, BP_Thug (for the hit lean) and BP_Archer. UBowComponent sets it as the
mesh's post-process override at BeginPlay; the mesh assets (the sample's, git-ignored) are never
touched.

Runs after create_partner (BP_Clint) and create_enemies (BP_Archer). Idempotent: the AnimBPs carry a
build tag and are rebuilt only when it changes; the class references are compared before they are
written.
"""

import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _common as c  # noqa: E402
import create_blueprints as cb  # noqa: E402

ANIM_PATH = "/Game/Blueprints/Animation"
BUILD_TAG = "HawkeyeBuild"
IK_BUILD = "bow-ik-3"   # bump when UHawkeyeBowIKGraphBuilder's graph changes (2: hit lean, arm alphas; 3: clip slots)

# (asset, skeleton, the mesh's own post-process AnimBP class to chain or None, spine bone, neck bone)
GRAPHS = [
    ("ABP_BowIK_Post", "/Game/Characters/UEFN_Mannequin/Meshes/SK_UEFN_Mannequin",
     "/Game/Characters/UEFN_Mannequin/Rigs/ABP_UEFN_Mannequin_PostProcess.ABP_UEFN_Mannequin_PostProcess_C",
     "spine_03", "neck_01"),
    ("ABP_BowIK_Post_Thug", "/Game/Mannequin/Character/Mesh/SK_Mannequin_Skeleton", None,
     "spine_03", "neck_01"),
]

# (Blueprint folder, Blueprint, AnimBP it runs, other BowComponent values)
USERS = [
    ("/Game/Blueprints/Player", "BP_Kate", "ABP_BowIK_Post", []),
    ("/Game/Blueprints/Player", "BP_Clint", "ABP_BowIK_Post", []),
    # Every thug runs the old mannequin's graph for the hit lean (the heavy and the gunner inherit it);
    # with no bow its arms stay on the animation.
    ("/Game/Blueprints/AI", "BP_Thug", "ABP_BowIK_Post_Thug", []),
    # The old mannequin has no palm socket, so his bow hangs off hand_l (the wrist): 8 cm on along
    # the aim puts the grip in his fist.
    ("/Game/Blueprints/Bosses", "BP_Archer", "ABP_BowIK_Post_Thug", [("hand_grip_offset", unreal.Vector(8.0, 0.0, 0.0))]),
]


def ensure_graph(name, skeleton_path, chained_path, spine, neck):
    full = c.asset_path(ANIM_PATH, name)
    builder = getattr(unreal, "HawkeyeBowIKGraphBuilder", None)
    if builder is None:
        c.log("FAILED", full, "UHawkeyeBowIKGraphBuilder not exposed; build the module")
        return None
    existing = c.load_or_none(full)
    if existing is not None and unreal.EditorAssetLibrary.get_metadata_tag(existing, BUILD_TAG) == IK_BUILD:
        c.log("exists", full, "{0} nodes".format(builder.count_anim_graph_nodes(existing)))
        return existing
    skeleton = c.load_or_none(skeleton_path)
    if skeleton is None:
        c.log("skipped", full, "no skeleton at {0} (run import_gasp / the mannequin copy first)".format(skeleton_path))
        return existing
    chained = None
    if chained_path:
        chained = unreal.load_class(None, chained_path)
        if chained is None:
            c.log("skipped", full, "no {0} to chain".format(chained_path))
            return existing
    c.ensure_directory(ANIM_PATH)
    abp = builder.build_bow_ik_post_process(full, skeleton, chained, spine, neck)
    if abp is None:
        c.log("FAILED", full, "the AnimBlueprint did not build or compile; see LogHawkeye")
        return None
    unreal.EditorAssetLibrary.set_metadata_tag(abp, BUILD_TAG, IK_BUILD)
    c.save(abp)
    # Compiling the slot nodes registers DefaultSlot and UpperBody on the skeleton.
    c.save(skeleton, only_if_dirty=True)
    c.log("updated" if existing is not None else "created", full,
          "{0} nodes, skeleton {1}, chained {2}".format(builder.count_anim_graph_nodes(abp), skeleton.get_name(),
                                                        chained.get_name() if chained else "nothing"))
    return abp


def _class_path(value):
    return value.get_path_name() if value is not None else ""


def assign(folder, bp_name, abp_name, abp, values):
    full = c.asset_path(folder, bp_name)
    bp = c.load_or_none(full)
    # Only a built and saved AnimBP: a failed build can leave an unsaved one in memory.
    hands = abp.generated_class() if abp is not None else None
    if bp is None or hands is None:
        c.log("skipped", full + ".BowComponent", "no Blueprint or no built " + abp_name)
        return
    bow = cb.cdo_component(bp, "bow_component")
    if bow is None:
        c.log("FAILED", full + ".BowComponent", "no bow component on the class default")
        return
    changed = cb.set_if_different(bow, values, full + ".BowComponent")
    if _class_path(bow.get_editor_property("hands_ik_class")) != _class_path(hands):
        c.set_props(bow, [("hands_ik_class", hands)], full + ".BowComponent")
        changed.append("hands_ik_class")
    if not changed:
        c.log("exists", full + ".BowComponent", "hands IK " + abp_name)
        return
    c.compile_blueprint(bp)
    c.save(bp)
    c.log("updated", full + ".BowComponent", "hands IK {0} ({1})".format(abp_name, ", ".join(changed)))


def run():
    graphs = {}
    for name, skeleton, chained, spine, neck in GRAPHS:
        graphs[name] = ensure_graph(name, skeleton, chained, spine, neck)
    for folder, bp_name, abp_name, values in USERS:
        assign(folder, bp_name, abp_name, graphs.get(abp_name), values)
    return graphs


if __name__ == "__main__":
    run()
    c.print_summary("bow ik")

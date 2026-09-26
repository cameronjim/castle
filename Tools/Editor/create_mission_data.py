"""Create the chapter data assets.

    /Game/Missions/DA_CH01_Rooftops               UMissionDefinition (docs/DESIGN.md, chapter 1)

DA_CH01_Rooftops is started on L_District_EastVillage by BP_GameMode_EastVillage, which
generate_city.py makes; its objectives are completed by the City_Obj_* trigger volumes that
script places on three roofs. It grants DA_Bow_Kate with 30 standard, 6 grapple, 2 putty, 2 bola,
1 smoke and 1 EMP arrow at the start, for now (create_weapon_data.py makes those, and runs first).

Property names come from Source/Hawkeye/Mission/MissionDefinition.h and
Source/Hawkeye/Mission/MissionObjective.h. Note the objective id property is ``ObjectiveTag``,
not ObjectiveId.
"""

import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _common as c  # noqa: E402

MISSION_PATH = "/Game/Missions"
WEAPON_PATH = "/Game/Blueprints/Weapons"

# Spellings the objective-id field has had in C++; the first one that exists is used.
OBJECTIVE_ID_PROPS = ["objective_id", "objective_tag"]


def data_asset_factory(data_asset_class):
    factory = c.new_factory("DataAssetFactory")
    if factory is None:
        return None
    c.set_props(factory, [("data_asset_class", data_asset_class)], "DataAssetFactory")
    return factory


def objective_id(objective):
    for prop in OBJECTIVE_ID_PROPS:
        try:
            return str(objective.get_editor_property(prop))
        except Exception:  # noqa: BLE001 - try the next spelling
            continue
    return ""


# --------------------------------------------------------------------------------------
# chapter 1: Rooftops
# --------------------------------------------------------------------------------------

CH01_NAME = "DA_CH01_Rooftops"

# Scalar fields, checked on every run so an existing asset is corrected rather than skipped.
CH01_FIELDS = [
    ("mission_name", "Rooftops"),
    ("mission_number", 1),
    ("end_card_line", "Okay. That's not one of mine."),
    ("show_objective_text", True),
]

# (ObjectiveId, Title, Description). find_arrow is the chapter's closing beat: an arrow that
# isn't Kate's, fletched purple. A trigger volume completes it until the arrow prop exists.
CH01_OBJECTIVES = [
    ("reach_roof", "Get to a rooftop", "Up is where the patrol starts."),
    ("cross_block", "Cross the block without touching the street",
     "Roof to roof. The street is for people who aren't Hawkeye."),
    # Beat 4: the Tracksuit pair on the cross_block roof. City_ThugGroup_clear_roof completes it
    # when both thugs tagged RoofPair are down.
    ("clear_roof", "Deal with the Tracksuits on the roof",
     "Two of them, arguing. Fists, the bow, or a quiet one from behind."),
    ("find_arrow", "Find the arrow", "Someone else has been shooting up here."),
]


# The chapter's starting quiver: the bow, then (arrow asset, count) per type. The trick arrows are
# here for testing (claude-docs/gameplay-semantics.md, "trick arrows"); explosive is left out, so
# slot 7 starts empty.
CH01_BOW = "DA_Bow_Kate"
CH01_ARROWS = [("DA_Arrow_Standard", 30), ("DA_Arrow_Grapple", 6), ("DA_Arrow_Putty", 2), ("DA_Arrow_Bola", 2),
               ("DA_Arrow_Smoke", 1), ("DA_Arrow_EMP", 1)]


def _object_path(value):
    """Package path of an object or a soft reference to one ("" for none), so both compare equal."""
    if value is None:
        return ""
    for getter in ("get_path_name", "export_text"):
        method = getattr(value, getter, None)
        if method is not None:
            try:
                return str(method()).split(".")[0]
            except Exception:  # noqa: BLE001
                pass
    return str(value).split(".")[0]


def _quiver_matches(asset, bow, arrows):
    try:
        if _object_path(asset.get_editor_property("starting_bow")) != _object_path(bow):
            return False
        current = list(asset.get_editor_property("starting_arrows") or [])
    except Exception:  # noqa: BLE001
        return False
    if len(current) != len(arrows):
        return False
    for grant, (arrow, count) in zip(current, arrows):
        same_arrow = _object_path(grant.get_editor_property("arrow")) == _object_path(arrow)
        if not same_arrow or int(grant.get_editor_property("count")) != count:
            return False
    return True


def _apply_quiver(asset):
    """StartingBow and StartingArrows from CH01_BOW / CH01_ARROWS. Returns True when it changed."""
    bow = c.load_or_none(c.asset_path(WEAPON_PATH, CH01_BOW))
    arrows = [(c.load_or_none(c.asset_path(WEAPON_PATH, name)), count) for name, count in CH01_ARROWS]
    if bow is None or any(arrow is None for arrow, _count in arrows):
        c.log("skipped", CH01_NAME + " quiver", "run create_weapon_data first")
        return False
    if _quiver_matches(asset, bow, arrows):
        return False
    grants = []
    for arrow, count in arrows:
        grant = unreal.HawkeyeArrowGrant()
        grant.set_editor_property("arrow", arrow)
        grant.set_editor_property("count", count)
        grants.append(grant)
    return bool(c.set_props(asset, [("starting_bow", bow), ("starting_arrows", grants)], CH01_NAME))


def _text_value(value):
    return str(value) if value is not None else ""


def _objectives_match(asset):
    try:
        current = list(asset.get_editor_property("objectives") or [])
    except Exception:  # noqa: BLE001
        return False
    if len(current) != len(CH01_OBJECTIVES):
        return False
    for obj, (oid, title, description) in zip(current, CH01_OBJECTIVES):
        if obj is None or objective_id(obj) != oid:
            return False
        if _text_value(obj.get_editor_property("title")) != title:
            return False
        if _text_value(obj.get_editor_property("description")) != description:
            return False
        if bool(obj.get_editor_property("optional")):
            return False
    return True


def _build_objectives(asset, objective_cls):
    objectives = []
    for oid, title, description in CH01_OBJECTIVES:
        objective = unreal.new_object(objective_cls, outer=asset)
        c.set_first_prop(objective, OBJECTIVE_ID_PROPS, oid, "MissionObjective " + oid)
        c.set_props(objective, [("title", title), ("description", description), ("optional", False)],
                    "MissionObjective " + oid)
        objectives.append(objective)
    return objectives


def create_chapter_one():
    """DA_CH01_Rooftops. Creates it, or corrects whichever fields differ. No flashback yet."""
    full = c.asset_path(MISSION_PATH, CH01_NAME)
    cls = c.find_class("MissionDefinition", "/Script/Hawkeye.MissionDefinition")
    objective_cls = c.find_class("MissionObjective", "/Script/Hawkeye.MissionObjective")
    if cls is None or objective_cls is None:
        c.log("FAILED", full, "UMissionDefinition / UMissionObjective not exposed to Python")
        return None

    asset, created = c.create_asset(CH01_NAME, MISSION_PATH, cls, data_asset_factory(cls), quiet=True)
    if asset is None:
        return None

    changed = []
    for prop, value in CH01_FIELDS:
        try:
            current = asset.get_editor_property(prop)
        except Exception as exc:  # noqa: BLE001
            c.log_error("{0}.{1}".format(full, prop), exc)
            continue
        same = (bool(current) == value) if isinstance(value, bool) else (
            current == value if isinstance(value, int) else _text_value(current) == value)
        if not same and c.set_props(asset, [(prop, value)], CH01_NAME):
            changed.append(prop)

    if not _objectives_match(asset):
        if c.set_props(asset, [("objectives", _build_objectives(asset, objective_cls))], CH01_NAME):
            changed.append("objectives")

    if _apply_quiver(asset):
        changed.append("starting quiver")

    if created or changed:
        c.save(asset)
        c.log("created" if created else "updated", full,
              "{0} objectives".format(len(CH01_OBJECTIVES)) if created else ", ".join(changed))
    else:
        c.log("exists", full)
    return asset


def run():
    c.ensure_directory(MISSION_PATH)
    return create_chapter_one()


if __name__ == "__main__":
    run()
    c.print_summary("mission data")

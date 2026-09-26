"""Create the mission-1 and flashback-1 data assets, and chapter 1 of the Hawkeye build.

    /Game/Missions/DA_M01_CellBlockD              UMissionDefinition (prison build, retiring)
    /Game/Flashbacks/Definitions/DA_FB01_Sunday   UFlashbackDefinition
    /Game/Missions/DA_CH01_Rooftops               UMissionDefinition (docs/DESIGN.md, chapter 1)

DA_CH01_Rooftops is started on L_District_EastVillage by BP_GameMode_EastVillage, which
generate_city.py makes; its objectives are completed by the City_Obj_* trigger volumes that
script places on three roofs.

Property names come from Source/Castle/Mission/MissionDefinition.h,
Source/Castle/Mission/MissionObjective.h and Source/Castle/Flashback/FlashbackDefinition.h.
Note the objective id property is ``ObjectiveTag``, not ObjectiveId.

As a convenience the mission is also assigned to BP_CastleGameMode.StartingMission, which
is what actually starts the mission when a level loads.
"""

import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _common as c  # noqa: E402

MISSION_PATH = "/Game/Missions"
FLASHBACK_PATH = "/Game/Flashbacks/Definitions"
IMAGE_PATH = "/Game/Flashbacks/Images"
PLAYER_PATH = "/Game/Blueprints/Player"

MISSION_NAME = "DA_M01_CellBlockD"
FLASHBACK_NAME = "DA_FB01_Sunday"

# Spellings the objective-id field has had in C++; the first one that exists is used.
OBJECTIVE_ID_PROPS = ["objective_id", "objective_tag"]

# (ObjectiveId, Title, Description)
OBJECTIVES = [
    ("leave_cell", "Get out of the cell", "The door is open. Nobody came to close it."),
    ("security_door", "Get through the security door", "It needs a keycard. Thugs carry them."),
    ("reach_stairwell", "Reach the stairwell", "Up is out. Keep moving."),
]

# Objectives an existing mission is stripped of. find_weapon was completed by the pistol pickup,
# which went with the first-person build; left in, the mission could never be finished.
RETIRED_OBJECTIVES = ["find_weapon"]

# (texture name, caption)
SLIDES = [
    ("T_FB01_01", "Sunday."),
    ("T_FB01_02", "Maria wanted the park. Frank wanted to sleep in."),
    ("T_FB01_03", "Lisa found a dog. Not ours."),
    ("T_FB01_04", "Frank Jr. wouldn't get out of the car."),
    ("T_FB01_05", "It rained on the way home."),
    ("T_FB01_06", ""),
]

HOLD_SECONDS = 4.0
CROSSFADE_SECONDS = 1.0

# Shown under the mission name on the end card.
END_CARD_LINE = "They kept me alive for a reason. I'm going to find out what."


def data_asset_factory(data_asset_class):
    factory = c.new_factory("DataAssetFactory")
    if factory is None:
        return None
    c.set_props(factory, [("data_asset_class", data_asset_class)], "DataAssetFactory")
    return factory


def create_flashback():
    full = c.asset_path(FLASHBACK_PATH, FLASHBACK_NAME)
    cls = c.find_class("FlashbackDefinition", "/Script/Castle.FlashbackDefinition")
    if cls is None:
        c.log("FAILED", full, "UFlashbackDefinition not exposed to Python")
        return None

    asset, created = c.create_asset(
        FLASHBACK_NAME, FLASHBACK_PATH, cls, data_asset_factory(cls), quiet=True
    )
    if asset is None:
        return None
    if not created:
        c.log("exists", full)
        return asset

    slide_struct = getattr(unreal, "FlashbackSlide", None)
    slides = []
    if slide_struct is None:
        c.log("skipped", full, "FFlashbackSlide not exposed; slides left empty")
    else:
        for texture_name, caption in SLIDES:
            texture = c.load_or_none(c.asset_path(IMAGE_PATH, texture_name))
            if texture is None:
                unreal.log_warning(
                    "[Castle] flashback slide texture missing: {0}".format(texture_name)
                )
            slide = slide_struct()
            c.set_props(
                slide,
                [
                    ("image", texture),
                    ("caption", caption),
                    ("hold_seconds", HOLD_SECONDS),
                    ("crossfade_seconds", CROSSFADE_SECONDS),
                ],
                "FlashbackSlide " + texture_name,
            )
            slides.append(slide)

    c.set_props(
        asset,
        [("title", "Sunday"), ("slides", slides), ("skippable", True)],
        FLASHBACK_NAME,
    )
    c.log("created", full, "{0} slides".format(len(slides)))
    c.save(asset)
    return asset


def create_mission(flashback):
    full = c.asset_path(MISSION_PATH, MISSION_NAME)
    cls = c.find_class("MissionDefinition", "/Script/Castle.MissionDefinition")
    if cls is None:
        c.log("FAILED", full, "UMissionDefinition not exposed to Python")
        return None

    asset, created = c.create_asset(
        MISSION_NAME, MISSION_PATH, cls, data_asset_factory(cls), quiet=True
    )
    if asset is None:
        return None
    if not created:
        # The asset predates EndCardLine, so fill that one field in on an existing mission
        # rather than leaving the end card blank. Everything else is left alone.
        update_end_card_line(asset, full)
        retire_objectives(asset, full)
        return asset

    c.set_props(
        asset,
        [
            ("mission_name", "Cell Block D"),
            ("mission_number", 1),
            ("end_card_line", END_CARD_LINE),
        ],
        MISSION_NAME,
    )

    objective_cls = c.find_class("MissionObjective", "/Script/Castle.MissionObjective")
    if objective_cls is None:
        c.log("skipped", full, "UMissionObjective not exposed; objectives left empty")
    else:
        objectives = []
        for tag, title, description in OBJECTIVES:
            objective = unreal.new_object(objective_cls, outer=asset)
            # The C++ field has been called both ObjectiveId and ObjectiveTag; try both.
            c.set_first_prop(objective, OBJECTIVE_ID_PROPS, tag, "MissionObjective " + tag)
            c.set_props(
                objective,
                [
                    ("title", title),
                    ("description", description),
                    ("optional", False),
                ],
                "MissionObjective " + tag,
            )
            objectives.append(objective)

        if not c.set_props(asset, [("objectives", objectives)], MISSION_NAME):
            c.log(
                "skipped",
                full,
                "Objectives array not settable from Python; add the objectives by hand",
            )

    if flashback is not None:
        c.set_props(asset, [("flashback_to_play", flashback)], MISSION_NAME)

    c.log("created", full, "{0} objectives".format(len(OBJECTIVES)))
    c.save(asset)
    return asset


def objective_id(objective):
    for prop in OBJECTIVE_ID_PROPS:
        try:
            return str(objective.get_editor_property(prop))
        except Exception:  # noqa: BLE001 - try the next spelling
            continue
    return ""


def retire_objectives(asset, full):
    """Drop RETIRED_OBJECTIVES from an existing mission. A no-op once they are gone."""
    try:
        objectives = list(asset.get_editor_property("objectives") or [])
    except Exception as exc:  # noqa: BLE001
        c.log_error("read objectives " + full, exc)
        return False

    kept = [obj for obj in objectives if obj is None or objective_id(obj) not in RETIRED_OBJECTIVES]
    if len(kept) == len(objectives):
        c.log("exists", full + ".objectives", "{0} objectives, none retired".format(len(kept)))
        return False

    if c.set_props(asset, [("objectives", kept)], MISSION_NAME):
        c.save(asset)
        c.log("updated", full + ".objectives", "removed " + ", ".join(RETIRED_OBJECTIVES))
        return True
    return False


def update_end_card_line(asset, full):
    """Set EndCardLine on an existing mission when it is missing or different."""
    try:
        current = asset.get_editor_property("end_card_line")
    except Exception:  # noqa: BLE001 - older build without the property
        c.log("exists", full, "no end_card_line property on this build")
        return False

    if str(current) == END_CARD_LINE:
        c.log("exists", full)
        return False

    if c.set_props(asset, [("end_card_line", END_CARD_LINE)], MISSION_NAME):
        c.save(asset)
        c.log("updated", full, "end_card_line")
        return True

    c.log("exists", full, "end_card_line not settable")
    return False


def assign_to_game_mode(mission):
    """Point BP_CastleGameMode.StartingMission at the mission so levels actually start it."""
    if mission is None:
        return
    full = c.asset_path(PLAYER_PATH, "BP_CastleGameMode")
    bp = c.load_or_none(full)
    if bp is None:
        c.log("skipped", full, "run create_blueprints.py first")
        return
    cdo = c.blueprint_cdo(bp)
    if cdo is None:
        c.log("skipped", full, "no class default object")
        return
    try:
        if cdo.get_editor_property("starting_mission") == mission:
            c.log("exists", full, "starting_mission already set")
            return
    except Exception:  # noqa: BLE001 - property may not exist yet
        pass
    if c.set_props(cdo, [("starting_mission", mission)], "BP_CastleGameMode"):
        c.compile_blueprint(bp)
        c.save(bp)
        c.log("updated", full, "starting_mission = " + MISSION_NAME)
    else:
        c.log("skipped", full, "starting_mission not settable")


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
    ("find_arrow", "Find the arrow", "Someone else has been shooting up here."),
]


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
    cls = c.find_class("MissionDefinition", "/Script/Castle.MissionDefinition")
    objective_cls = c.find_class("MissionObjective", "/Script/Castle.MissionObjective")
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

    if created or changed:
        c.save(asset)
        c.log("created" if created else "updated", full,
              "{0} objectives".format(len(CH01_OBJECTIVES)) if created else ", ".join(changed))
    else:
        c.log("exists", full)
    return asset


def run():
    c.ensure_directory(MISSION_PATH)
    c.ensure_directory(FLASHBACK_PATH)
    flashback = create_flashback()
    mission = create_mission(flashback)
    assign_to_game_mode(mission)
    create_chapter_one()
    return mission, flashback


if __name__ == "__main__":
    run()
    c.print_summary("mission data")

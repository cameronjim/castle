"""Create the mission-1 and flashback-1 data assets.

    /Game/Missions/DA_M01_CellBlockD              UMissionDefinition
    /Game/Flashbacks/Definitions/DA_FB01_Sunday   UFlashbackDefinition

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
    ("find_weapon", "Find a weapon", "There is a pistol somewhere in the guard station."),
    ("security_door", "Get through the security door", "It needs a keycard. Guards carry them."),
    ("reach_stairwell", "Reach the stairwell", "Up is out. Keep moving."),
]

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
        c.log("exists", full)
        return asset

    c.set_props(
        asset,
        [("mission_name", "Cell Block D"), ("mission_number", 1)],
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
                "Objectives array not settable from Python; add the four objectives by hand",
            )

    if flashback is not None:
        c.set_props(asset, [("flashback_to_play", flashback)], MISSION_NAME)

    c.log("created", full, "{0} objectives".format(len(OBJECTIVES)))
    c.save(asset)
    return asset


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


def run():
    c.ensure_directory(MISSION_PATH)
    c.ensure_directory(FLASHBACK_PATH)
    flashback = create_flashback()
    mission = create_mission(flashback)
    assign_to_game_mode(mission)
    return mission, flashback


if __name__ == "__main__":
    run()
    c.print_summary("mission data")

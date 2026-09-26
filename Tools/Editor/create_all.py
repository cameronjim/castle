"""Create every stage-1/stage-2 starter asset, in dependency order.

    0. pivot_cleanup.run_renames   BP_Guard -> BP_Thug, M_Guard* -> M_Thug* (before anyone asks)
    1. create_input_assets        IA_* and IMC_Default
    2. create_placeholder_textures T_FB01_01..06   (before the data assets that point at them)
    3. create_blueprints           BP_Castle* and WBP_Flashback (before the maps that use them)
    3b. create_world_blueprints    WBP_Hud, BP_Pickup_Keycard, BP_Door_Keycard, BP_Thug
    3c. create_weapon_data         DA_Weapon_Hands
    4. create_mission_data         DA_M01_CellBlockD, DA_FB01_Sunday
    5. create_sandbox_map          L_Sandbox, L_M01_CellBlockD
    6. create_room_art             procedural materials + the M01 cell/corridor art pass
                                   (it dresses the map the previous step builds)
    6b. pivot_cleanup.run_deletions retire the pistol, rifle and first-person assets, then fix
                                   up redirectors (after the maps stop referencing them)
    7. generate_city               L_District_EastVillage from OpenStreetMap, after everything
                                   else and only when Tools/Data/osm/east_village.buildings.json
                                   exists (Tools/fetch-osm.ps1 writes it)

Run headless:

    UnrealEditor-Cmd.exe Castle.uproject -run=pythonscript ^
        -script="Tools\\Editor\\create_all.py" -unattended -nullrhi -nosplash -nop4 -stdout

or via Tools\\create-content.ps1. Every step is idempotent, so re-running only fills gaps.
"""

import os
import sys
import traceback

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _common as c  # noqa: E402

# (title, module, function). The function defaults to run().
STEPS = [
    ("pivot renames", "pivot_cleanup", "run_renames"),
    ("input assets", "create_input_assets"),
    ("placeholder textures", "create_placeholder_textures"),
    ("blueprints", "create_blueprints"),
    ("world blueprints", "create_world_blueprints"),
    ("weapon data", "create_weapon_data"),
    ("mission data", "create_mission_data"),
    ("maps", "create_sandbox_map"),
    ("room art", "create_room_art"),
    ("pivot deletions", "pivot_cleanup", "run_deletions"),
    ("city", "generate_city"),
]

# Module names to leave out of this run, comma separated, e.g. CASTLE_SKIP_STEPS=generate_city.
SKIP_STEPS = set(
    name.strip() for name in os.environ.get("CASTLE_SKIP_STEPS", "").split(",") if name.strip())

# Steps that need downloaded data: skipped (not failed) until the file exists.
STEP_REQUIRES = {
    "generate_city": os.path.join(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
        "Data", "osm", "east_village.buildings.json"),
}


def main():
    c.reset_summary()
    unreal.log("[Castle] ==== creating stage 1-2 starter content ====")

    failures = []
    for step in STEPS:
        title, module_name = step[0], step[1]
        function_name = step[2] if len(step) > 2 else "run"
        unreal.log("[Castle] ---- {0} ----".format(title))
        if module_name in SKIP_STEPS:
            c.log("skipped", title, "CASTLE_SKIP_STEPS")
            continue
        required = STEP_REQUIRES.get(module_name)
        if required and not os.path.exists(required):
            c.log("skipped", title, "missing " + required)
            continue
        try:
            module = __import__(module_name)
            getattr(module, function_name)()
        except Exception as exc:  # noqa: BLE001 - one broken step must not stop the rest
            failures.append(title)
            unreal.log_error(
                "[Castle] FAILED   step '{0}' ({1}: {2})".format(title, type(exc).__name__, exc)
            )
            unreal.log_error(traceback.format_exc())

    unreal.log("[Castle] ==== summary ====")
    for action, path, extra in c.summary():
        unreal.log("[Castle] {0:<8} {1}{2}".format(action, path, "  (" + extra + ")" if extra else ""))

    counts = c.print_summary("content creation complete")
    if failures:
        unreal.log_error("[Castle] steps that raised: " + ", ".join(failures))
    if counts.get("FAILED"):
        unreal.log_error("[Castle] {0} asset(s) failed".format(counts["FAILED"]))
    return counts


main()

"""Turn a playtest session folder into one markdown file an agent (or Cameron) can read.

    python Tools/playtest-report.py Saved/Playtest/2026-09-29_14-03-11            # writes report.md there
    python Tools/playtest-report.py Saved/Playtest/2026-09-29_14-03-11 --stdout   # prints it instead
    python Tools/playtest-report.py --latest                                      # the newest session

The folder is what the game writes (claude-docs/testing.md, "Playtest capture"): notes.json (every F12 note
with its state, position and the LogHawkeye lines before it), note_<n>.png, photo_<n>.png, summary.json
(the session's counters) and, after a normal exit, Hawkeye.log. Any of them may be missing (a crash leaves no
summary); the report says so and carries on. Plain Python 3, no engine, no packages.
"""

import argparse
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PLAYTEST_DIR = os.path.join(ROOT, "Saved", "Playtest")


def get(obj, name, default=None):
    """A field by name, whatever case the engine's JSON writer gave its first letter."""
    if not isinstance(obj, dict):
        return default
    if name in obj:
        return obj[name]
    lowered = name.lower()
    for key, value in obj.items():
        if key.lower() == lowered:
            return value
    return default


def load_json(path):
    if not os.path.isfile(path):
        return None
    try:
        with open(path, encoding="utf-8-sig") as handle:
            return json.load(handle)
    except (OSError, ValueError) as exc:
        return {"_error": str(exc)}


def clock(seconds):
    seconds = int(round(float(seconds or 0)))
    hours, rest = divmod(seconds, 3600)
    minutes, secs = divmod(rest, 60)
    return "{0}:{1:02d}:{2:02d}".format(hours, minutes, secs) if hours else "{0}:{1:02d}".format(minutes, secs)


def yes(flag):
    return "yes" if flag else "no"


def state_summary(state):
    """The note's state as a short readable line; the full key=value text is printed too."""
    parts = []
    move = get(state, "Movement", "?")
    speed = get(state, "Speed", 0) or 0
    gait = []
    if get(state, "bSprinting"):
        gait.append("sprinting")
    if get(state, "bCrouching"):
        gait.append("crouched")
    parts.append("{0} at {1:.0f} cm/s{2}".format(move, float(speed), (" (" + ", ".join(gait) + ")") if gait else ""))
    parkour = get(state, "Parkour", "None")
    if parkour and parkour != "None":
        parts.append("parkour: " + parkour)
    if get(state, "bZipping"):
        parts.append("zipping")
    if get(state, "bAiming"):
        parts.append("aiming")
    if get(state, "bDowned"):
        parts.append("DOWNED")
    alerted = get(state, "AlertedThugs", 0) or 0
    if alerted:
        parts.append("in a fight ({0} alerted)".format(alerted))
    if get(state, "Crime"):
        parts.append("crime: " + get(state, "Crime"))
    if get(state, "Challenge"):
        parts.append("challenge: " + get(state, "Challenge"))
    if get(state, "bInterior"):
        parts.append("interior")
    if get(state, "bPaused"):
        parts.append("paused")
    parts.append("health {0:.0f}".format(float(get(state, "Health", 0) or 0)))
    parts.append("{0}, {1}".format(get(state, "TimeOfDay", "?") or "?", get(state, "Difficulty", "?") or "?"))
    return "; ".join(parts)


def summary_section(summary, lines):
    lines.append("## Session")
    lines.append("")
    if summary is None:
        lines.append("No summary.json (the game did not exit normally, or the session is still running).")
        lines.append("")
        return
    if "_error" in summary:
        lines.append("summary.json could not be read: " + summary["_error"])
        lines.append("")
        return
    fights = get(summary, "Fights", 0)
    rows = [
        ("Written", "{0} ({1})".format(get(summary, "WrittenAt", "?"), get(summary, "Reason", "?"))),
        ("Play time (world running)", clock(get(summary, "PlaySeconds"))),
        ("Paused (menus, map, phone, photo mode)", clock(get(summary, "PausedSeconds"))),
        ("Notes / photos", "{0} / {1}".format(get(summary, "Notes", 0), get(summary, "Photos", 0))),
        ("Deaths", get(summary, "Deaths", 0)),
        ("Fights won", "{0} of {1}".format(get(summary, "FightsWon", 0), fights)),
        ("Time in fights", clock(get(summary, "FightSeconds"))),
        ("Crimes stopped", "{0} of {1} started".format(get(summary, "CrimesStopped", 0), get(summary, "CrimesStarted", 0))),
        ("Challenges run", "{0} (gold {1}, silver {2}, bronze {3})".format(
            get(summary, "ChallengesRun", 0), get(summary, "GoldMedals", 0), get(summary, "SilverMedals", 0),
            get(summary, "BronzeMedals", 0))),
        ("Fast travels", get(summary, "FastTravels", 0)),
        ("Distance", "{0:.0f} m".format(float(get(summary, "DistanceMetres", 0) or 0))),
        ("Grapples", get(summary, "Grapples", 0)),
        ("Time aiming", clock(get(summary, "AimSeconds"))),
        ("Maps", ", ".join(get(summary, "Maps", []) or []) or "-"),
    ]
    lines.append("| | |")
    lines.append("|---|---|")
    for name, value in rows:
        lines.append("| {0} | {1} |".format(name, value))
    lines.append("")

    movement = get(summary, "MovementSeconds", {}) or {}
    total = sum(float(v) for v in movement.values()) or 1.0
    if movement:
        lines.append("### Time in each movement state")
        lines.append("")
        lines.append("| State | Time | Share |")
        lines.append("|---|---|---|")
        for key, value in sorted(movement.items(), key=lambda kv: -float(kv[1])):
            lines.append("| {0} | {1} | {2:.0f}% |".format(key, clock(value), 100.0 * float(value) / total))
        lines.append("")


def notes_section(notes_file, folder, lines):
    lines.append("## Notes")
    lines.append("")
    if notes_file is None:
        lines.append("No notes.json: no note was taken this session.")
        lines.append("")
        return
    if "_error" in notes_file:
        lines.append("notes.json could not be read: " + notes_file["_error"])
        lines.append("")
        return
    notes = get(notes_file, "Notes", []) or []
    if not notes:
        lines.append("No notes.")
        lines.append("")
        return
    lines.append("Each note is where Cameron pressed F12 (or held Menu). Match it to what he describes by time,")
    lines.append("place and state; the log lines are the ones just before the press.")
    lines.append("")
    for note in notes:
        index = get(note, "Index", "?")
        state = get(note, "State", {}) or {}
        pos = get(note, "Position", {}) or {}
        lines.append("### Note {0}, {1} into the session ({2})".format(index, clock(get(note, "SessionSeconds")),
                                                                   get(note, "WallClock", "?")))
        lines.append("")
        lines.append("- Where: {0}, pos ({1:.0f}, {2:.0f}, {3:.0f}), yaw {4:.0f}".format(
            get(state, "Map", "?"), float(get(pos, "X", 0) or 0), float(get(pos, "Y", 0) or 0),
            float(get(pos, "Z", 0) or 0), float(get(note, "Yaw", 0) or 0)))
        lines.append("- Doing: " + state_summary(state))
        lines.append("- State line: `{0}`".format(get(note, "StateText", "")))
        shot = get(note, "Screenshot", "")
        if shot:
            exists = os.path.isfile(os.path.join(folder, shot))
            lines.append("- Screenshot: [{0}]({0}){1}".format(shot, "" if exists else " (missing)"))
            if exists:
                lines.append("")
                lines.append("  ![note {0}]({1})".format(index, shot))
        log = get(note, "RecentLog", []) or []
        if log:
            lines.append("")
            lines.append("  LogHawkeye before the note:")
            lines.append("")
            lines.append("  ```")
            for line in log:
                lines.append("  " + line)
            lines.append("  ```")
        lines.append("")


def photos_section(folder, lines):
    photos = sorted((name for name in os.listdir(folder) if re.match(r"photo_\d+\.png$", name)),
                    key=lambda name: int(re.findall(r"\d+", name)[0]))
    lines.append("## Photos")
    lines.append("")
    if not photos:
        lines.append("No photos.")
    for name in photos:
        lines.append("- [{0}]({0})".format(name))
    lines.append("")


def log_section(folder, lines):
    path = os.path.join(folder, "Hawkeye.log")
    lines.append("## Log")
    lines.append("")
    if not os.path.isfile(path):
        lines.append("No Hawkeye.log copy (it is copied at a normal exit; a crash leaves it in Saved/Logs).")
        lines.append("")
        return
    warnings = {}
    playtest = []
    with open(path, encoding="utf-8", errors="replace") as handle:
        for raw in handle:
            line = raw.rstrip("\n")
            if "LogHawkeye: Warning:" in line or "LogHawkeye: Error:" in line:
                text = re.sub(r"^\[[^\]]*\]\[[^\]]*\]", "", line).strip()
                warnings[text] = warnings.get(text, 0) + 1
            elif "LogHawkeye: NOTE #" in line or "LogHawkeye: Playtest:" in line or "LogHawkeye: Photo mode:" in line:
                playtest.append(re.sub(r"^\[[^\]]*\]\[[^\]]*\]", "", line).strip())
    lines.append("[Hawkeye.log](Hawkeye.log)")
    lines.append("")
    if warnings:
        lines.append("LogHawkeye warnings and errors (count, text):")
        lines.append("")
        lines.append("```")
        for text, count in sorted(warnings.items(), key=lambda kv: -kv[1])[:30]:
            lines.append("{0:4d}  {1}".format(count, text))
        lines.append("```")
    else:
        lines.append("No LogHawkeye warnings or errors.")
    lines.append("")
    if playtest:
        lines.append("Kit lines (notes, photo mode, verbose bumps, fights, summaries):")
        lines.append("")
        lines.append("```")
        lines.extend(playtest[:200])
        lines.append("```")
        lines.append("")


def build_report(folder):
    notes_file = load_json(os.path.join(folder, "notes.json"))
    summary = load_json(os.path.join(folder, "summary.json"))
    session = (get(summary, "Session") if isinstance(summary, dict) else None) \
        or (get(notes_file, "Session") if isinstance(notes_file, dict) else None) \
        or os.path.basename(os.path.normpath(folder))
    lines = ["# Playtest {0}".format(session), ""]
    lines.append("Folder: `{0}`".format(os.path.abspath(folder)))
    lines.append("")
    summary_section(summary, lines)
    notes_section(notes_file, folder, lines)
    photos_section(folder, lines)
    log_section(folder, lines)
    return "\n".join(lines).rstrip() + "\n"


def latest_session():
    if not os.path.isdir(PLAYTEST_DIR):
        return None
    folders = [os.path.join(PLAYTEST_DIR, name) for name in os.listdir(PLAYTEST_DIR)
               if os.path.isdir(os.path.join(PLAYTEST_DIR, name))]
    return max(folders, key=os.path.getmtime) if folders else None


def main(argv):
    parser = argparse.ArgumentParser(description="Markdown report for a playtest session folder.")
    parser.add_argument("folder", nargs="?", help="Saved/Playtest/<session>")
    parser.add_argument("--latest", action="store_true", help="use the newest session under Saved/Playtest")
    parser.add_argument("--stdout", action="store_true", help="print the report instead of writing report.md")
    args = parser.parse_args(argv)

    folder = latest_session() if args.latest or not args.folder else args.folder
    if not folder or not os.path.isdir(folder):
        print("No session folder: {0}".format(folder or PLAYTEST_DIR), file=sys.stderr)
        return 2
    report = build_report(folder)
    if args.stdout:
        sys.stdout.write(report)
        return 0
    out = os.path.join(folder, "report.md")
    with open(out, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(report)
    print(out)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

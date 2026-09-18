# Stage 6: Polish, test, ship

Goal: a packaged Windows build a stranger can install and finish without you in the room.

## Playtesting, properly
- Five to eight testers, one at a time, on a machine that isn't yours if possible.
- You watch, you say nothing, you take notes. Where they got stuck, where they died
  more than twice, where they looked bored, where they laughed or swore.
- After: three questions. What was the story? (Tests whether the lie-then-truth landed.)
  Which part would you replay? Which part would you cut?
- Fix the top five problems. Test again with new people. Two rounds minimum.

## Difficulty
One slider, three positions, changes guard damage and boss health. Default in the middle.
Tune so a first-time player dies 2 to 4 times per mission, 5 to 8 times on the Warden.

## Settings menu
Resolution, fullscreen, VSync, overall quality (Low/Medium/High/Epic via scalability
groups), mouse sensitivity, invert Y, subtitles on/off, master/music/SFX/voice volume,
and a full key rebind screen (Enhanced Input makes this straightforward). Save to config.

## Accessibility, the cheap wins
Subtitle size option. Colour-blind safe alert indicators (shape plus colour). Hold vs
toggle for aim and crouch. Skip flashback is already in. Add "replay flashbacks" from
the main menu once unlocked, because people will want to rewatch them after the ending.

## Performance and stability
- Profile every map with `stat unit` and `stat gpu`. Target 60 fps at 1080p High on the
  4070, 30 fps at 1080p Low on something like a GTX 1060.
- Fix every crash you can reproduce. Turn on crash reporting in the packaged build so
  testers send you logs.
- Load times under 15 seconds per mission on an SSD.
- Play the whole game once with the memory profiler open. Leaks show up over 2 hours.

## Packaging
- Project Settings, Packaging: Shipping config, Full Rebuild, Pak file, exclude editor
  content. Windows 64.
- Build once, install it on a clean Windows machine or VM, play it through. Things that
  work in the editor and break in a package: hard references to editor-only assets,
  missing plugins, asset manager paths, anything using `#if WITH_EDITOR` wrongly.
- Final build is a zip or an installer (Inno Setup is free). Around 5 to 15 GB.

## Legal, before anyone outside sees it
This is Marvel's character. Sharing a free fan game with friends or on a portfolio page
is low risk. Selling it, or putting it on Steam or itch.io for money, is off the table
with the Punisher name and skull. Two paths:
- Keep it a free portfolio piece and say so plainly wherever it's posted.
- Rename before release. Frank Castle becomes an original name, the skull becomes a
  different mark, nothing else changes. Do a find-and-replace in the dialogue table
  and swap one decal.
Decide this at the start of stage 6, not the night before posting.

## Release checklist
- [ ] Two rounds of playtesting done, notes in `docs/playtests/`
- [ ] Packaged build finishes on a machine you didn't develop on
- [ ] Settings persist across restarts
- [ ] No known crash in a full playthrough
- [ ] Credits screen: you, asset sources, voice actors, music, fonts, and the Marvel
      disclaimer if the name stayed
- [ ] A 90 second gameplay video and 6 screenshots for the page
- [ ] Build uploaded (itch.io for a free game, or a Drive link for friends)
- [ ] Tag the commit `v1.0` and back the repo up somewhere off this machine

## After
Ship it, take two weeks off, then read the playtest notes again. The list of what you'd
do differently is the design doc for the next game.

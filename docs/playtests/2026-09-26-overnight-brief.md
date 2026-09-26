# Overnight brief, 2026-09-26

For Cameron, to read before playing. Everything below is pushed to main and passed the QA
gate: build, 171 automation tests, content scripts a no-op on rerun, verify scripts, and
renders from the standalone game process with a clean log.

## What got built, in order
1. **Grapple arrow.** Q zips to a marked rooftop anchor. 1,691 anchors on the block.
2. **Motion-matched locomotion** from Epic's animation sample. Kate runs, stops, turns,
   and strafes like a person. Her speeds are the sample's for now.
3. **Parkour.** Vault and mantle through the sample's traversal system; ledge grab, hang,
   climb, and drop are ours. Every roof edge is a ledge. Camera fixed for looking up.
4. **Bow.** Hold to draw, release to fire, arrows arc and stick, ring reticle shows spread,
   perfect-release bonus. The quiver replaced the weapon slots: 1 to 6 and the wheel pick
   arrow types. Chapter 1 starts you with 30 standard and 6 grapple arrows.
5. **Combat.** Four tracksuit thugs on the block: a pair on a roof, a pair patrolling by
   the park. They rush, telegraph, and swing. Kate has a light and heavy strike on V, a
   dodge on Ctrl tap, takedowns on F, hit stop, and a low-health tint.
6. **Lap test and map size.** A scripted lap measures the stage 2 checklist. Ledges and
   anchors moved out of the map: 72 MB down to 2.8 MB.
7. **Ways down.** Fire escapes on 487 tenements, drop-to-hang, chained drops, grapples
   that clear the parapet and go level or downward, zip cancel.

## Numbers
| Metric | Value |
|---|---|
| Commits on main | 303 |
| Automation tests | 171, all passing |
| Buildings / anchors / ledges / fire-escape landings | 526 / 1,691 / 3,723 / 2,010 |
| Scripted lap | 61.75 s, 0 wall contacts, every move first try |
| Roof fight (scripted) | won in 8.2 s, 85 health left, 1 hit taken |
| Frame time, RTX 4070, 1280x720 | about 6 ms average |
| District map | 2.8 MB |

## Stage 2 checklist, honestly
Two items pass, three are partial, one needs you. Details in `docs/plans/02-prototype.md`.
The partials: grapple chains are touch-and-go rather than mid-air redirects; the landing
roll is still a placeholder dip; only the roof pair was fought by script.

## How to play it
Play Hawkeye on the desktop. You start on Avenue A by Tompkins Square Park with the
objective "Get to a rooftop."

| Key | Action |
|-----|--------|
| WASD, mouse | Move, look |
| Shift | Sprint. Hold it into the 90 cm and 150 cm blocks by the start to vault and mantle |
| Space | Jump. Jump at a wall 2 to 2.6 m tall to grab the ledge; Space again climbs |
| Ctrl | Tap while moving: dodge. Hold: crouch. While sprinting: slide. At a roof edge, moving toward it: drop to hang. Hanging: drop |
| Q | Grapple to the green diamond on a rooftop anchor. Works mid-zip past 70% to chain |
| Left click | Draw the bow; release to fire. Longer draw, straighter and harder |
| Right click | Aim over the shoulder |
| 1 to 6, wheel | Arrow type (1 standard, 2 grapple) |
| V | Strike. Tap light, hold heavy |
| F | Takedown from behind a thug |
| Tab | Quiver and inventory |
| Esc | Pause, Settings (sensitivity), Restart, Quit |

Suggested first ten minutes: run a lap of the park block at street level. Then get on a
roof by fire escape (mantle onto the bottom landing, jump to grab each rail, Space to
climb) and by grapple. Run the roofs. Come down a fire escape by hanging and dropping.
Then find the pair on the roof at 94 Avenue A and fight them.

## What I want from you
Plain notes on how moving feels. Camera distance, run speed, whether vault and mantle
fire when you expect, whether the grapple feels fast enough, whether the descent is
readable. Whether the fight is fun or annoying. Anything you tried to do and couldn't.
Quit the game before you report so the next build can run.

## Known rough edges
- Kate is a pink mannequin with a bow floating near her shoulder when drawing. There's no
  bow animation yet; that's a stage 3 animation layer.
- Strikes are procedural lunges, no attack animation. Knocked-down thugs snap upright.
- The bat on a walking thug can look like a spear.
- Arrows stuck in things read as a small dot from behind.
- Load hitch on the first frames of the district (about 400 ms once).
- The Game Animation Sample assets are not in git (2 GB, Epic's). A fresh clone needs the
  sample installed and one content run. Documented in CLAUDE.md.

## Late addition: the polish sweep landed too
After this brief was first written, one more pass passed QA (318 commits, 173 tests):
- Grapple chains redirect in the air now. The lap dropped to 33.71 s with a real mid-air
  redirect and no touch-downs.
- A real landing roll when you land moving from above 4 m, a stumble when still.
- Thugs' bats hang by the leg and only come up to swing. Knocked-down thugs get back up
  smoothly. Arrows have real shafts, vanes, and a purple nock.
- The bow stays held out in front while drawing; the in-hand version looked worse from
  the camera and was rejected.

## Next in the queue
Your notes decide the rest of stage 2. Stage 3 starts with trick arrows.

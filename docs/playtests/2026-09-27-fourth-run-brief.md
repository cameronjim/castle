# Fourth run brief, 2026-09-27

For Cameron. Everything below is on main and passed the QA gate. Story is still untouched:
every line, title, and name in the game is a bracketed placeholder. Cameron's verdict so
far: traversal and movement feel okay. Combat is untested by a human and is the question
for this session.

The project folder was renamed from `fps-game` to `hawkeye` today. The three desktop
shortcuts and the docs were repointed. `Play Hawkeye` (the editor `-game` launch) has all
of this. `Play Hawkeye (Packaged)` was built before items 1 to 6 and is stale; repackage
with `Tools\package.ps1` when you want the fast-loading build again.

## What landed, in order
1. **Melee depth.** Three-hit light combo (15, 15, 25, the third knocks back), a heavy
   anywhere in the chain knocks down. Combo counter on the HUD, purple at x5 with a 20%
   bonus. Parry: tap strike during a thug's wind-up. Finisher on F against a knocked-down
   or parried thug, with a camera push and a slow-motion beat. Strikes and dodges turn
   toward the nearest thug. Hit lean on both sides.
2. **Side challenges.** Six purple-lit pedestals on the block: three rooftop archery
   ranges (twelve glowing targets, some moving, sixty seconds) and three traversal routes
   (eight floating rings across roofs and a fire escape, gold under sixty seconds). Medals,
   best scores, a results card with Retry and Leave. Thugs nearby go calm for the run.
3. **Difficulty and accessibility.** Story / Normal / Hard, asked at New Game and in
   Settings. Hard tightens the parry window and speeds the archers; Story adds health
   regen sooner and more trick arrows. Four colour palettes, subtitle size and backing,
   hold or toggle for aim and crouch, reduced shake and flashing, HUD scale, flashback
   replay from the pause menu.
4. **Street crimes.** Twelve crime spots. While you roam, a crime spawns every couple of
   minutes at a spot 25 to 40 m away with a marker: a mugging (get there before the third
   hit), a robbery (down the runner, pick up the loot), an ambush, or a rooftop crew with
   an archer. Stopping one refills five arrows.
5. **Safehouses and fast travel.** A second safehouse on Avenue B at East 10th, 278 m
   from the first on East 7th. Walk into each once to discover it; then fast travel between
   them from the safehouse menu in about a second and a half with no reload. House icons on
   the compass.
6. **Interiors.** A JSON room layout format, a generator, and a sample auction house:
   lobby, a two-storey hall with a gallery, an office, a stair, and a locked vault. Its door
   is on a building facing the park; the gallery has a roof exit back into the district.
   Wood and carpet footsteps, a capped camera indoors, save and return through the door.

## Numbers
| Metric | Value |
|---|---|
| Commits on main | 643 |
| Automation tests | 331, all passing |
| Maps | district, sample interior, placeholder scene |
| Scripted fights (Normal) | roof 6.5 s; archer duel 12.8 s; street 24.2 s; mugging 12.3 s |
| Scripted challenges | archery 12 of 12 in 17 s; traversal 18 to 19 s, gold |
| Fast travel, fade to fade | about 1.5 s |

## Controls
| Action | Keyboard | Pad |
|---|---|---|
| Move, look | WASD, mouse | Sticks |
| Sprint, auto vault and mantle | Shift | L3 |
| Jump, climb from a ledge | Space | A |
| Dodge tap, crouch hold, slide, drop to hang | Ctrl | B |
| Grapple; again mid-zip to chain | Q | RB |
| Draw, release to fire | Left click | RT |
| Aim | Right click | LT |
| Strike; hold for heavy; tap during a wind-up to parry | V | X |
| Finisher or takedown; otherwise interact | F | Y |
| Interact: doors, pedestals, pickups, chapter-end object | E | Y |
| Arrow type; hold for the wheel | 1 to 6, wheel; hold Tab | D-pad; hold View |
| Switch to Clint and back | X | LB |
| Send Clint to a point | T | none yet |
| Phone | P | Hold D-pad down |
| Pause, settings, quit | Esc | Menu |

## How to play this one
Start a New Game and pick Normal. Then, in this order:

1. **The roof pair.** Follow the marker up to the second roof. Fight with hands only:
   three lights, then a heavy. Try to parry the bat. Finish one on the ground with F.
   This is the fight the whole game depends on, so give it three or four tries.
2. **The archer duel and the heavy.** Duck the purple glint, shoot between draws. Then the
   shield heavy on the park's south corner: light strikes bounce, heavies and bola stagger.
3. **A crime.** Roam the park block for two minutes and take whatever spawns.
4. **A challenge.** The nearest pedestal is a street-corner traversal route. Press E.
5. **The safehouses.** East 7th first, then Avenue B at 10th, then fast travel back.
6. **The auction house.** The door on the park-facing building. Up to the gallery, out
   by the roof.
7. **Hard.** Change difficulty in Settings and redo the roof pair.

## What I want from you
Is the fight fun or annoying, and specifically: does the combo read, does the parry feel
fair, is the finisher worth doing, does the camera stay useful when three thugs close in.
Then whether a crime interrupting a roam is welcome or a nuisance, whether the challenge
rings are visible enough, and whether going indoors feels like the same game.

## Known rough edges
- **No attack, kick, dodge, hit, or bow-draw animations.** Strikes are still lunges posed
  by IK and the bow floats in front of Kate while drawing. This is the biggest gap in how
  combat feels and is being fixed next: the import and retarget pipeline is being built
  tonight, and needs one download session from you (free Mixamo clips and Epic's free
  Paragon Sparrow bow set). The list will be in `Tools/Data/Anims/README.md`.
- Challenge pedestal cap and icon read white instead of purple.
- Distant archery targets are small at 30 to 40 m.
- No alley crime spots yet; robbery, ambush, and rooftop crimes are covered by headless
  tests only, not a scripted lap.
- Chapter select is a stub.
- Thug hit spark competes with the hit flash. Footstep snow kick is faint.

## Next
Animations first. Then, in parallel with your notes: the art direction question (what
Kate, the thugs, and the tenements should look like beyond greybox and mannequins) and the
story session (`docs/DESIGN.md` open questions, `docs/chapters/CH01.md` beats).

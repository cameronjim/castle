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

## Morning note, 2026-09-28: combat and bow animations are in
Overnight, three passes landed the clip pipeline and the clips you downloaded. Everything
passed the gate: both build targets, 337 tests, the import script a no-op on rerun, and a
standalone screenshot pass whose images were read by eye.

What plays now:
- **Kate's chain** is Mixamo's cross, hook, and uppercut-jab, trimmed so each lands 0.25 s
  after the press; a three-hit chain is done in about 1.4 s. Heavy is the surprise uppercut
  (0.41 s), kick is the front kick, parry is the block, dodges are the four standing dodges,
  finishers are the leg sweep and the flying kick with the bow up.
- **The bow** is Paragon Sparrow's draw, aim loop, and fire on Kate, Clint, and the archers.
  The bow arm extends, the string hand anchors at the cheek, and the IK keeps the hand on
  the string. This is the shot that looks right first time.
- **Thugs** swing with the standing melee punch and the horizontal bat swing, fitted to
  their telegraphs at 0.8x to 1.3x so parry timing is unchanged. Hit reactions, knockdown
  to the floor, and the sweep-fall finisher victim are clips too.
- **Fallback rule:** any role without a clip keeps the old procedural move, so nothing broke.

Three bugs found and fixed by looking at the images: Mixamo clips sank everyone to their
knees (hip height was being written into the root), Mixamo strikes took up to 1.2 s to
land (now trimmed per clip in the manifest), and the retargeted clips never moved the IK
foot bones so the leg IK glued Kate's feet to the floor mid-kick (now pinned).

Try first: the roof pair, hands only, and tell me whether the chain reads as three
different punches. Then a parry on the bat. Then a kick and a heavy on purpose.

Still rough: no get-up clip (a knockdown holds then blends up; search Mixamo for
"Getting Up"), the heavy reads as a swing more than an uppercut, the thug's fist punch
holds its first frame for 0.29 s to fill the telegraph, Kate never dodges the heavy's
bash in the scripted street fight, and no bow nock clip. Pipeline docs are in
`claude-docs/animation.md`; the clip list and folder layout in `Tools/Data/Anims/README.md`.
Clips and retargets are not in git; the script rebuilds them from your downloads.

## Afternoon note, 2026-09-28: Cameron's first combat notes, answered
Cameron played: traversal okay, punching the air always threw the same punch and heavy,
the bow draw looked unnatural, jumping at low walls and ledges often did nothing, and the
grapple stopped showing its key while diamonds still showed. He asked for a day setting.
All five landed, each with builds, the full suite (357 tests) and screenshots read by eye:
- **Air combos.** A missed light advances the chain, so the air throws cross, hook,
  uppercut-jab. Every strike role rotates through its downloaded clips with no repeat.
- **Bow.** The draw plays at its own pace to the anchor (Sparrow's shot, 0.5 s) and holds
  the aim loop; an aim offset built from Sparrow's poses pitches the torso with the
  camera; the string-hand IK only nudges toward the arrow line. Re-draws run straight on.
- **Jump at walls.** A fan of seven probes out to 180 cm, tops 40 to 260 cm, angled
  approaches turn her to the wall, the probe keeps running 0.8 s through the jump, and a
  press during a move is buffered. A running jump at a 90 cm wall used to arrive with her
  feet just below the probe; that was the "hop and nothing".
- **Grapple.** The key hint used to hide itself after five uses. Gone. The diamond goes
  grey with "No grapple arrows" when the quiver is out. Blocked lines are refused before
  the shot, and a blocked zip or a chain reels the arrow back (this makes chains free;
  Cameron to confirm he wants that). Clint no longer takes Kate's stuck arrows.
- **Day and night.** Settings, World, Time of day. `-TimeOfDay=Day` on the command line
  or `hawkeye.TimeOfDay Day` in the console for testing. Day is a cool overcast; night is
  unchanged.

## Known rough edges
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

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

## Second night, 2026-09-29: the thirteen notes, answered
Cameron's second set of notes, plus the standing-jump one, all landed with the same gate
(builds, 401 tests, screenshots read). Kate pass, lap, fights and bow sequence all green.
- **Standing jump at walls.** The failed presses in the log were on tenement roofs: every
  "88 cm top" was a parapet, and the vault refused because its one landing spot sat inside
  the neighbour's lower parapet. The landing now searches 75 cm further; a 486-press sweep
  of those roofs went from 181 moves to 248 with no blocked landings.
- **Roof-edge guard.** One jump at a parapet over a drop mantles onto it and stops. Double
  tap to leap on purpose.
- **Melee assist.** A strike picks the thug within 350 cm and 60 degrees of where you
  meant, turns onto him, closes to 90 cm, forgives a near miss.
- **Bow aim assist.** Settings, Controls, Aim assist Off / Normal / Strong: reticle pull,
  snap on aim, a 2 degree bend on release. Never pulls a head aim down.
- **Archers and gunners.** Aim from where you were 0.3 s ago, cone widens with your speed,
  a sharp turn or dodge makes the shot miss, archers wait 1.5 to 2.5 s between shots.
  Sprinting sideways past an archer at 8 m: about 6 hits in 20 on Normal, 3 on Story.
- **Arrows embed** in the body they hit, on Kate and thugs. Kate's and Clint's arrows fly
  through each other (one of your downs was Clint's arrow through a dead thug).
- **Downed.** 8 s ring, "Clint is coming" or "Any key", thugs stop hitting you and circle,
  then a fade and a reload with placed thugs reset and crime thugs gone. Both grey
  screens in your log were you playing as Clint with AI Kate unable to reach you.
- **Finding places.** Both safehouses always on the compass (hollow until found), "Mark
  nearest safehouse" works before discovery, pedestals on the compass within 150 m with
  a world marker within 80 m, "Mark nearest challenge" in the pause menu, toasts nearby.
- **Infinite grapple arrows.** The slot shows an infinity sign; nothing spends them.
- **Sprint toggle.** One press of the left stick sprints until you stop, aim, crouch, or
  press again. Settings, Sprint: Hold / Toggle / per device.
- **Combat readability.** A purple ring under the thug you're swinging at, a big
  red-orange telegraph "!" with a cream line while a parry would work, hit stop and a lens
  punch on heavies, a spark at the contact point, a thud and dust on knockdowns, and a
  fight camera that pulls back 70 cm when two thugs close in. Heavy is the roundhouse.
- **Day look.** A real winter afternoon now: warm sun 30 degrees up, white snow, red-brown
  brick, blue shadow. Lamp buzz off by day, EMP and time of day reconciled, beacons and
  signs scale up by day.
- **Air combos and variants** from the first note set are still in: a missed light
  advances the chain, every strike rotates through its clips.
Still waiting on you: Mixamo roll and get-up clips (the landing roll is the placeholder
dip until then). Chapter 1 still ends at the arrow, the slides and the grey room; that is
the story session, not a bug.

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

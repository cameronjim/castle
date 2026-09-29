# Gameplay semantics

What each system promises. Tests in `Source/Hawkeye/Tests` assert these. If you change a
rule here, change the test and the code in the same commit.

**Pivot note (2026-09-25).** The game is now third person with bows. Sections marked
LEGACY describe first-person or pistol behaviour that still exists in code until the
stage 2 cleanup removes it; don't extend them. Sections marked PLANNED are the contract
for code not yet written; write the tests from them. Everything unmarked stands.

## Third-person camera and look (built 2026-09-26)
- Spring arm behind the character: hip length 350, socket offset (0, 70, 60), FOV 90.
  Aim (right mouse) blends over 0.15 s to length 180, offset (0, 70, 65) (moved out past the bow arm on 2026-09-27), FOV 70. The
  70 cm lateral offset puts the character around 40% of screen width so the right side
  of the frame is open. Position lag 10, rotation lag 12. Sensitivity from the settings
  subsystem. `ComputeCameraTargets(bAiming)` is pure and tested.
- The arm probes on the Camera channel with a 12 cm radius and pulls in against walls,
  never inside geometry. Closer than 100 cm the character is hidden from the camera so
  the view isn't inside her shoulder.
- Fight camera (2026-09-29): with two or more alerted thugs near her (one within 9 m, the rest within 15 m; flat, 3 m up or
  down), the hip boom lengthens 70 cm and the lens tips down 4 degrees, over 0.5 s; back over 1 s
  when the fight thins out. See "Combat readability".
- Movement input rotates the character toward the input direction; while aiming she
  faces the camera yaw. Look input never rotates her while idle.

## Movement (built 2026-09-26; every number is a property on BP_Kate)
- Locomotion animation is motion matching from the Game Animation Sample (see
  infrastructure.md). Kate's speeds are currently set by the sample's character graph
  (run 500, stop 200, aimed strafe 180, plus its walk and sprint gaits chosen from the
  bridged `WantsToWalk` / `WantsToSprint` flags). The native speeds below apply to thugs
  and to any character not derived from the sample. Reconciling the two is a stage 2
  TODO once traversal feel is settled.
- Native speeds: walk 250, run 500, sprint 700, crouch 200. Gamepad: stick at 0.9 or
  more runs immediately, 0.4 or more runs after 0.2 s, lighter walks. Keyboard always
  runs; Shift sprints.
- Sprint toggle (2026-09-29, after "sprint on controller should be pressing the left joystick once
  and the character continues to sprint the whole time"). On a pad one L3 press turns sprint on
  (`FHawkeyeSprintToggle`); it stays on with the stick let go of L3, through turns and stops of up to
  0.6 s, and turns off on a second press, when the stick has sat in its dead zone for more than
  0.6 s (counted from the raw stick, so a zip or a vault with the stick held keeps it), on aim (a
  toggled sprint gives way to the aim; a held Shift still refuses it), on a bow draw, on a crouch (a
  crouch at speed slides, and the toggle ends when the slide does, so she comes up at a run), and
  when a menu opens (every pause-style screen, through `ApplyPauseInputMode`). Shift stays a hold.
  The Sprint row in Settings (Controls) reads "Hold on keys, toggle on pad" (the default), Hold or
  Toggle; Hold or Toggle apply to both devices. Which device pressed is the controller's last input
  (`IsUsingGamepad`). The toggle's on and off log at Log with the reason ("sprint toggle off (stick
  centred for 0.6 s)"). The sample's `WantsToSprint` is still fed from the sprint gait. No HUD hint
  shows the sprint key.
- Jump height 90 cm, air control 0.3.
- Slide: crouch while sprinting; 0.7 s, speed eases 750 to 200, capsule half-height 50,
  restored after.
- Landing from above 400 cm halves speed and dips the camera 30 cm for 0.3 s (a
  placeholder for the roll and stumble). Fall damage starts at 900 cm at 10% of max
  health, rises to a 60% cap at 2500 cm, and never kills: it stops at 1 health.
- `hawkeye.DebugMovement 1` draws state, speed, and fall height on the HUD. Landing
  height, fall damage, and slide start are logged at Log; sprint start and stop at Verbose.

## HUD (third person)
- No crosshair. A 4 px reticle dot appears only while aiming and flashes white for 0.1 s
  on a hit. Hotbar slots are Hands, Bow, Reserved until arrows exist.
- Grapple marker: a green hollow diamond with the key under it means a press zips there, always
  (no use limit, and grapple arrows have no count); grey means it would not (an arrow still in
  flight); a smaller grey diamond marks an anchor in view whose line is blocked.
  Rules in the traversal section; the look per state is `UHawkeyeHudWidget::GetGrappleMarkerLook`.
- No minimap: the compass is the HUD. The world map is its own paused screen (M, or hold D-pad up); see
  "World map".

## PLANNED: traversal
- Parkour (built 2026-09-26). Vault and mantle run through the Game Animation Sample's
  traversal (`AC_TraversalLogic` on the sample character, called by reflection from
  `GaspTraversal.cpp`); ledge grab, hang, climb, and drop are ours in `UParkourComponent`,
  which is also the fallback when the sample finds no traversable block or montage.
  Vault: obstacle top 60 to 110 cm with a back edge within 120 cm and a floor beyond that
  fits the capsule. Mantle: up to 200 cm with room to stand, or over a thin top such as a
  parapet onto a floor no more than 120 cm below. Ledge grab: 200 to 260 cm; hang with
  feet 145 cm below the edge and the capsule 36 cm off the wall; jump climbs, crouch
  drops. Falling past a ledge 150 to 260 cm above the feet catches it. Auto triggers
  within 120 cm while sprinting at 300 cm/s or more, and refuses a vault whose far floor
  is more than 150 cm down (the jump key allows up to 400), so sprinting at a parapet
  never throws the character off a roof. Durations: vault 0.5 s, mantle 0.8 s, jump to
  hang 0.35 s, catch 0.15 s, climb 1.0 s; input is locked only that long, except that the
  sample's montages hold input until they blend out.
- The jump key (revised 2026-09-28 after "spamming jump doesn't help me get over the mini walls").
  It probes a fan of 7 rays from -35 to +35 degrees around the stick direction (the facing when
  the stick is centred), middle ray first, each up to 180 cm from the capsule, and takes tops from
  40 cm (the sprint trigger still starts at 60) up to 260 cm, choosing vault, mantle, climb-over or
  ledge grab by height and depth exactly as above; an angled press turns her to the wall before
  the move (and before the sample's traversal, so it finds the same wall). Standing still against
  a wall, the press vaults, mantles or grabs it when the height fits, at any speed and at any angle
  in the fan (revised 2026-09-29 after "if you try to jump moving forward with no momentum, it still
  takes a hundred tries to hurdle over"). A vault lands 25 cm past the back edge or, when the capsule
  does not fit there (the neighbour's lower parapet right behind a party wall), up to 75 cm further
  out, and never on something level with the top; heights are compared 3 cm short of their limits
  because a walking capsule floats that far over the floor. The sample refusing (no montage at a
  standstill) hands the same move to our own vault or mantle. When nothing fits and a plain jump
  starts, the late catch re-probes every tick while airborne, rising or falling, for 0.8 s, 100 cm
  ahead, probing from 5 cm above the feet: a top up to 200 cm above the feet with room to stand is
  mantled (one less than 40 cm above them only when it is 40 cm above where she took off, so a
  running jump at a waist-high wall pops onto it instead of sliding down its face; a second press in
  the air keeps the take-off where she left the ground) (a thin one vaulted only
  within the sprint trigger's 150 cm drop), a ledge 150 cm or more above the feet is grabbed (up to
  260 while rising, 330 while falling; a rising grab takes the 0.35 s jump-to-hang, a falling one the
  0.15 s catch). Landing, a hang, a move, a zip or a slide ends the window. A press in the air probes
  the late catch's way, 180 cm ahead. A press during a move
  is held 0.3 s and fires when the move ends (a press into a hang climbs), so mashing chains
  obstacles. Never a mantle onto a top the capsule does not fit on. Every press that does not
  become a move logs why at Log (`jump: no parkour move: too tall (280 cm, up to 260) at +0
  degrees, 66 cm away, on <actor>`, `no obstacle in the 35 degree fan within 180 cm`, `no room to
  stand on the 88 cm top and no vault over it (no floor within 800 cm below the feet beyond it: a roof
  edge)`), and a late catch that found nothing says so when its window closes. A roof-edge parapet
  and a parapet backed by a taller neighbour are refused on purpose.
- Roof-edge guard (2026-09-29, after a 1679 cm fall in Cameron's log: the refusal was right, but the
  plain jump that followed carried her over the parapet and off the roof). When the jump key's fan
  refuses a parapet (a thin top with a back edge, from the jump key's 40 cm up to 160 cm) whose far
  side drops more than 400 cm below her feet (`FarSideDrop`, measured just past its back edge, 800
  when there is no ground within the probe) and she is moving toward it (50 cm/s at it, or the stick
  within 60 degrees of it), the press does not become a plain jump. If the capsule fits on the
  middle of the parapet's top she mantles onto it with our own procedural mantle (never the
  sample's, which picks its own move) and stays there: while she stands on it, walking off is
  allowed only back toward the roof (`bCanWalkOffLedges` is off otherwise), so the stick still held
  at the drop does not walk her off; from there crouch drops to the hang and the grapple works as
  anywhere. A press during that mantle is dropped rather than buffered (mashing would otherwise jump
  her off the top the moment she stood on it). With no room on the top the plain jump goes ahead
  with her speed toward the drop held at zero until she lands, so she lands short of it. A second
  press within 0.3 s of the guarded one means "I know": from the mantle or the held jump, a quick
  hop (0.35 s, arched to clear the top) takes her just past its back edge and lets her go at 450
  cm/s, or her speed before the press if faster. An open roof edge (no parapet), a parapet over a
  drop of 400 cm or less (the jump key's vault), a standstill with the stick centred, a hang (jump
  climbs) and the sprint trigger (which never vaults off a roof) are unchanged. Every step logs at
  Log ("jump: roof-edge guard: a 88 cm parapet with a 800 cm drop beyond, on ...: mantle onto its
  top and stop there", "on the parapet top ...", "a double tap during the mantle, so she leaps
  over"). `roof_edge_guard.png` in the Kate pass shows her on a tenement's parapet after one press.
- Traversable ledges: the sample detects `LevelBlock_Traversable` actors on the
  `GameTraceChannel1` sweep (our Weapon channel) with four `Ledge_1..4` splines whose up
  vector is the ledge's outward normal. The generator places `BP_TraversableBlock`
  (a child with the sample's level-visual lookups off) along every roof edge of 1 m or
  more, hidden, blocking only that channel, 2 cm proud of the facade; 3,723 on the
  district. Verify checks every spline end against the parapet corners within 5 cm.
- Ledges and grapple anchors are not saved in the map. `generate_city.py` writes their
  transforms into `DA_EastVillage_CityProps` (`UCityLedgeData`) and one `ACityLedgeSpawner`
  spawns them at load: all anchors and the ledges within 100 m immediately (about 180 ms),
  the rest nearest-first at 4 ms per frame, done about 5 s after load. The grapple grid is
  rebuilt after the spawn. The district map is 2.8 MB (was 72 MB). Street lamps are still
  saved actors.
- Fire escapes (built 2026-09-26): every building 10 to 30 m tall gets one zig-zag
  escape on its longest road-facing edge (487 of 494 qualify; the rest would hit a
  neighbour, a lamp, or another escape). Landings 240 x 90 cm with 90 cm rails at 330 cm
  and every 330 cm above, the top one at least 180 cm under the roof, ladders alternating
  between them. The outer rail carries a traversable ledge so mantle and vault work onto
  it. 2,010 landings, stored in the props data asset and spawned nearest-first. Black
  iron `M_SteelPainted`.
- Drop to hang: within 60 cm of an edge with more than 150 cm of drop beyond it (lips up
  to 130 cm), crouch while moving toward the edge or jump while facing away transitions
  to the hang on that edge. From a hang, crouch drops. The catch window is 150 to 330 cm
  above the feet so floor-spaced landings chain: hang, drop, catch, repeat. A drop never
  re-catches the ledge it let go of. Walking off an edge, letting go of a hang, or
  cancelling a zip is a controlled drop; landings over 150 cm get the dip.
- Grapple launch: the zip starts from a launch point 120 cm above the feet, reached by a
  0.15 s hop, and ignores her own roof, parapet, and fire escape until 250 cm plus the
  capsule radius clear of the start (plus the 150 cm allowance at the anchor). Level and
  downward lines are allowed. Jump or crouch mid-zip lets go with the catch active. 62 of
  63 anchored roofs within 150 m of the start have a clear roof-to-roof zip.
- Chaining (revised 2026-09-26): the chain input is allowed from 40% of the line; a chain
  arrow fired mid-zip arrives instantly and the direction of travel turns onto the new
  line over 0.1 s. After a mid-air chain the old anchor's building counts as a start
  support until she is clear of it. The scripted lap shows a true mid-air redirect with
  zero touch-downs inside the chain.
- Landing above 400 cm while moving is a roll: 0.5 s, 200 cm along the stick, capsule at
  half-height 50, camera pitched down 8 degrees and back, input locked 0.35 s, a single
  forward tumble of the body. Landing while still is a 0.4 s stumble from 0 back to run
  speed. Both procedural; the sample has land clips but no roll. Grapple landings never
  trigger either. Fall damage is still zero below 900 cm. Fall damage begins at 900 cm and never kills from a
  rooftop you can reach by grapple.
- Camera looking up: between +20 and +60 degrees of pitch the arm shortens 350 to 220,
  the socket Z rises 60 to 110, and the pivot lifts 0 to 150 cm in world space (without
  the lift the lens sat on the pavement). Pitch is clamped to [-70, +75]. Above about
  53 degrees Kate drops out of the bottom of the frame rather than filling it.
- Grapple arrow (built 2026-09-26): valid anchors are within 2500 cm, farther than 300 cm
  (so the anchor just landed beside doesn't stay lit), within 30 degrees of the camera
  forward, and in line of sight from the camera; the closest by angle wins and shows a
  marker (a green diamond with the key under it, Q or RB, every time; see the next bullet). Fire spawns a straight, gravity-free
  arrow at 6000 cm/s; on arrival the character zips in Flying mode along a straight line
  to the anchor's landing point at 1800 cm/s, input locked except the camera, and lands
  in Walking with no fall damage. Chaining is allowed once past 70% of the line, when
  the marker returns. A zip is cancelled by static geometry on the path except within
  150 cm of the anchor (corner anchors share walls with neighbours). Grapple arrows are
  unlimited (revised 2026-09-29, "grapples should be infinite - it is definitely part of
  traversal"): a shot costs nothing and is never refused for want of one; the arrow stays in
  the anchor until she is within 200 cm of it and is then tidied away (nothing goes back in the
  quiver, and nothing logs a recovery).
- Grapple marker and arrows (revised 2026-09-28 after "there are green diamonds but it no longer
  tells you what button to press"; the key hint used to vanish after the fifth arrow, the diamond
  stayed green with an empty quiver, and anchors whose line was blocked were marked). The target
  also needs a clear zip line from where she is (`IsZipClear`, the same sweep and start and anchor
  allowances as the zip; mid-zip, the line she is on counts as her start), checked nearest the
  middle first, at most 3 per 0.1 s refresh. `GetTargetState()` is what a press would do: Ready
  (green diamond, key hint), ArrowInFlight (grey, no hint), TooEarlyToChain (no marker), None
  (the NoArrows state and its "No grapple arrows" line went with the count, 2026-09-29). A press
  fires only when Ready. An anchor nearer the
  middle than the target that passes everything but the line is drawn as a smaller dim grey
  diamond and is never fired at; `hawkeye.DebugGrapple 1` writes why under it ("the line hits
  FireEscapeLanding_34") and the state next to the key. A zip blocked on the way,
  or left by a chain, reels its arrow back out of the anchor; letting go with jump or crouch leaves it
  in the anchor. Stuck arrows belong to who shot them: the partner never tidies Kate's. Only the
  player's pawn keeps a target (the partner's AI does not refresh one).
- Anchors sit on the parapet centre (15 cm in from the roof edge) at every roof corner
  and at mid-edge on edges over 25 m, on buildings over 8 m, none within 4 m of another.
  Landing points are 60 cm further inboard so the capsule clears the parapet. The
  generator places them; verify asserts every landing point is above a building roof.
  Anchor lookup uses a 25 m spatial grid.

## Bow and arrows (built 2026-09-26; the rules below hold, with these notes)
- Data: `UBowDefinition` (DA_Bow_Kate 0.8 s draw, DA_Bow_Clint 1.0 s) and
  `UArrowDefinition` (DA_Arrow_Standard 40 damage, cap 30, recoverable; DA_Arrow_Grapple
  hit effect Grapple, which makes it unlimited: `UInventoryComponent::IsUnlimitedArrow`; its cap 6
  is unused). `UBowComponent` on the player; `AArrowProjectile`
  with gravity, sticks into what it hits for 30 s, recovered within 150 cm.
- The quiver replaced the weapon slots: `Bow` (null means fists on left click), six
  arrow slots with standard always in slot 1, `ActiveArrowSlot`, keycards. Keys 1 to 6
  and the wheel skip empty slots. The mission grants `StartingBow` and `StartingArrows`;
  CH01 gives Kate's bow, 30 standard, and the grapple (the grant's count, 6, is ignored). `Clear()`
  returns to the grant.
- The grapple slot has no count (2026-09-29): the hotbar, the quiver wheel and the inventory screen
  show an infinity sign where a trick arrow shows "2/4"; it is never spent (`ConsumeArrow` always
  succeeds and keeps it), a grapple pickup, a refill or a reward adds nothing (`AddArrows` only fills
  the slot, returning 0), and it cannot be set to zero. Trick arrows keep their counts and caps.
- Q fires a grapple arrow at the marked anchor without changing the nocked slot, spending
  nothing. Damage scales with draw from 40% to 100% of the arrow's damage.
- Arrows collide as WorldDynamic, not on the Weapon channel, because the invisible
  traversal ledge blocks sit on that channel along every roof edge.
- Reticle while drawing: a ring whose radius is the current spread projected at 1500 cm,
  plus a draw bar that flashes in the perfect window. Drawing forces aim mode and walk
  speed.
- Arrows: 100 cm shaft, 2.5 cm thick, light wood colour with three vanes at 120 degrees
  and a purple nock, so Kate's arrows read as hers when stuck in things. A nocked arrow
  sits on the string while drawing.
- Hands (built 2026-09-26): a post-process animation Blueprint, `ABP_BowIK_Post` (and a
  `_Thug` variant for the old mannequin), built headless by editor-only C++ from
  `create_bow_ik.py`. It links the mesh's own post-process graph, then in component space
  turns the spine 15 degrees side-on plus up to 45 toward the aim (neck counter-turned),
  then two-bone IK on the left hand to the grip and the right hand to the string point,
  alphas blended over 0.15 s. Applied at BeginPlay as a post-process override so the
  motion-matching graph keeps its state. Targets in the head frame (forward, right, up,
  cm): grip (50, -10, -4); string hand from (34, -2, -6) at rest to (-2, 10, -8) at full
  draw. Hands land within 0.8 cm of target. While the bow is up it rides in the IK hand;
  the string nock follows the right palm. `bHolsterWhenIdle` (default on) puts it on the
  back when idle. Fingers do not curl (no finger posing). Kate, Clint, and archers use it.
  A native anim-instance proxy was rejected because a post-process instance can't receive
  the input pose without a Blueprint graph.
- Superseded note: before IK, the bow blended from the left palm socket to a
  point in front of the left shoulder while drawing and rides across the back when
  holstered. Upper-body bow clips now layer over locomotion, with an aim offset on top (see
  "Combat animation clips").

## Trick arrows (built 2026-09-26; effects are custom actors, no GAS; smoke, EMP ring and fireball are placeholder shapes until Niagara systems exist)
- Slot order is fixed: 1 standard, 2 grapple, 3 putty, 4 bola, 5 smoke, 6 EMP, 7
  explosive. Keys 1 to 6, the mouse wheel, and the radial wheel select; explosive is
  reached by wheel or radial. Holding Tab (View on a pad) opens the radial with game time
  at 0.2; release selects the highlighted segment. Tapping Tab still opens the inventory.
- Putty: 10 damage; a thug hit, or within 200 cm of the impact, is staggered then held
  4 s (no movement, AI paused, a blob on him). On a wall it leaves a blob for 10 s. Cap 4.
- Bola: 10 damage; trips a thug for 2.5 s through the ragdoll-and-recover path. Cap 4,
  recoverable.
- Smoke: a 500 cm cloud for 8 s. Thugs inside it, or whose line of sight to the player
  crosses it, are blinded; Kate crouching inside it is undetectable. Cap 3.
- EMP: a 600 cm pulse. Street lamps in the radius go out for 20 s, gunners' pistols jam
  for 6 s. Cap 3.
- Explosive: 80 damage at the centre falling linearly to 0 at 400 cm, knocks thugs down
  2 s, camera shake, a flash. Kate takes the damage too if she is inside. Cap 2.
- Thug AI honours `bHeld`, `bBlinded`, and `bJammed`. CH01 grants 2 putty, 2 bola,
  1 smoke, 1 EMP for testing.

## Bow and arrows, original rules
- Draw is a hold: 0 to `FullDrawSeconds` (0.8 Kate, 1.0 Clint). Release below 25% draw
  cancels. Power scales damage from 40% to 100% and spread from 4 degrees to 0.5.
- Arrows are projectiles with gravity, 6000 cm/s at full draw, penetration off. Headshot
  bones as today. A perfect release (within 0.1 s of full draw) adds 25% damage.
- Each arrow type is a `UArrowDefinition`: projectile class, on-hit effect, damage, cap,
  and whether it is recoverable. Standard arrows cap at 30 and are always slot 1. Trick
  arrow caps are small (3 to 6). Counts refill at safehouses and from pickups.
- Focus: a meter that fills on hits and takedowns; while airborne, holding aim slows time
  to 0.3x and drains it. Empties in 3 s of use.

## Partner and switching (built 2026-09-26)
- The partner runs a StateTree (`ST_Partner`, built headless by editor-only C++ through
  the StateTree editor API; no GUI needed) with states in priority order: Revive, GoToMark,
  Cover, Attack, Follow. Without a tree (tests) the controller picks the same order in C++.
- Follow: 400 to 800 cm band, settles at 600, sprints beyond 1500. If the height gap is
  over 300 cm and he has been out of range for 5 s, he zips to the anchor nearest the
  player, or teleports to the landing point if the line is blocked; both logged.
- Attack: the enemy the player hit most recently within 3 s (nearest on a tie), with his
  own bow (1.0 s draw, released just after full) at range and bow strikes under 200 cm.
- Cover: a ring of candidate points, keeping those where the shooter's trace is blocked.
- Revive: at 0 health the player is downed, not restarted; after 3 s beside her she is
  back at 30%. Once per fight; a fight ends 10 s after the last contact. He has 4.5 s to
  reach her before he is put beside her (was 12), so the revive lands inside the 8 s down
  (see "Health and damage", downed). (Changed 2026-09-28.)
- The partner can't drop below 1 health, self-heals under 50%, can be staggered. T marks
  a point for him to go to; on a pad it is R3 (press the right stick, which nothing else
  used; added 2026-09-29). Either way the mark is where the reticle sits: a trace straight
  down the camera's centre (the player camera manager's view point), up to
  `MarkTraceDistance`, projected onto the navmesh. The HUD's partner line ends with the
  hint, "Clint: following  [T] send" or "[R3] send" by the last input.
- `SwitchCharacter()` on X or LB: only when the chapter's `bAllowSwitching` is set (CH01
  true); refused if either character is mid-traversal, mid-zip, mid-takedown, or down.
  Swaps possession, hands the old pawn to the partner controller, rebinds the HUD
  (quiver and name), blends the camera over 0.3 s.
- Banter: `DT_Dialogue`, 48 lines, six per speaker per situation (idle roam, after a
  fight, objective near, low health). One every 45 to 90 s of roaming or on the event,
  speakers alternating, 4 s subtitle, no audio. Clint's arrows are picked up into the
  player's quiver for now. BP_Clint in the map adds PoseSearch index-build warnings at load.

## Mission
- A mission has 1 or more objectives. At least one must be non-optional.
- `StartMission` on a subsystem that already has an active mission ends the old one
  without firing `OnMissionComplete`, then starts the new one. Logs a warning.
- `StartMission` fires `OnMissionStarted(Definition)` once and nothing else. It does not
  fire `OnObjectiveUpdated` for the initial objectives; the HUD reads
  `GetActiveObjectives()` and `GetCurrentObjective()` on `OnMissionStarted`.
- `StartMission` with a definition that has no non-optional objectives logs an error and
  does nothing.
- `CompleteObjective(Id)` on an unknown id logs a warning and does nothing. On an
  already-completed objective it does nothing and fires no delegate.
- Completing an objective fires `OnObjectiveUpdated` exactly once for that objective.
- The mission is complete when every objective with `bOptional == false` is complete.
  Optional objectives completed after that point still fire `OnObjectiveUpdated` but
  never fire `OnMissionComplete` again.
- `OnMissionComplete` fires at most once per `StartMission`.
- If `FlashbackToPlay` is set, `OnFlashbackRequested` fires immediately after
  `OnMissionComplete`, in the same frame, with the loaded definition. If it is null,
  the controller proceeds straight to `NextLevel`.
- The current objective is the first incomplete non-optional objective in array order.
  HUD shows that one.
- Objective ids are `FName`, unique within a mission, lowercase snake_case
  (`find_weapon`, `reach_stairwell`). Reusing an id across missions is fine.

## Objective markers and compass (specified 2026-09-26)
- An objective trigger volume registers its centre with the tracker for its id at
  BeginPlay and unregisters at EndPlay; a thug group objective registers its location the
  same way; a data asset never needs coordinates. Priority: an explicit `WorldLocation`,
  then the actor named by `MarkerActorLabel`, then the registered point.
  `GetCurrentObjectiveLocation` returns the current objective's point if it has one.
  (Built 2026-09-26; 181 tests.)
- The HUD shows a cream diamond marker projected onto the current objective with the
  distance in metres under it, clamped to the screen edge with an arrow when off-screen,
  hidden within 3 m. A 400 px compass strip at the top centre shows ticks every 15
  degrees, N/E/S/W, and the objective's bearing.
- Completing an objective shows a 2 s "Objective complete" toast; the next objective
  shows a 2 s "New objective" toast.
- CH01's rooftop objectives each get a 1 m beacon with a purple emissive top and a 300 lm
  purple point light so the roof reads from the street at night.
- An objective may carry a `Hint`, a second line under its title in the HUD. CH01's
  reach_roof points at the safehouse door on East 7th (placeholder text). (2026-09-28)
- The compass also shows every safehouse (a filled house once found, a hollow one before)
  and every challenge pedestal within 150 m (a purple medal). A pedestal within 80 m gets
  a world marker (medal and distance) when the current objective is farther, not while a
  challenge runs. A marked safehouse's marker is a house with its distance and name, a "?"
  in it and "[Unknown safehouse]" until found; a marked pedestal's is a medal. (2026-09-28)

## Flashback
- `Play(Definition)` with a null definition or zero slides finishes immediately and
  fires `OnFlashbackFinished` once. It never leaves the game paused.
- Each slide occupies `HoldSeconds + CrossfadeSeconds`: it displays fully for
  `HoldSeconds`, then crossfades for `CrossfadeSeconds` into the next. The last slide
  crossfades to black instead. Total duration is the sum over all slides of
  `HoldSeconds + CrossfadeSeconds`, and `OnFlashbackFinished` fires when that elapses.
- Skip (any key, if `bSkippable`) finishes the whole flashback, not the current slide.
  Skip during the first 0.5 seconds is ignored so a held key from gameplay can't skip it.
- Playback pauses the game (`SetGamePaused(true)`) and restores the previous pause state
  and input mode on finish. If the world is torn down mid-playback, no crash and no
  dangling pause.
- Voice lines play at slide start. A voice line longer than `HoldSeconds` is cut off at
  the crossfade; the data should be tuned so this doesn't happen.
- Glitch slides (stage 3): if `ReplacedByImage` is set, the slide shows `Image` for
  `HoldSeconds - GlitchSeconds`, runs the glitch material for `GlitchSeconds`, then shows
  `ReplacedByImage` for a second full `HoldSeconds` before crossfading out.

## Health and damage
- `CurrentHealth` is clamped to `[0, MaxHealth]`.
- `ApplyDamage(Amount <= 0)` does nothing and fires nothing.
- While `bInvulnerable`, damage is ignored entirely: no `OnHealthChanged`, no `OnDeath`.
- `OnHealthChanged(Old, New, Instigator)` fires on every change, including heals. The
  dead flag is set before `OnHealthChanged` broadcasts on the killing blow, so a listener
  that checks `IsAlive()` inside that callback already sees false.
- `OnDeath` fires exactly once when health first reaches 0, after `OnHealthChanged`. Further damage after death
  does nothing. `Heal` after death does nothing; revive is a separate explicit call
  (`Revive(NewHealth)`) that resets the dead flag.
- Damage types (stage 3): `DT_Bullet`, `DT_Melee`, `DT_Explosion`. Melee damage also
  fires `OnStaggered` on the health component if the amount is above
  `StaggerThreshold`. Bullets never stagger. Only the player is affected by
  `DT_Explosion` falloff; guards take full damage in the radius.
- Downed (built 2026-09-28, playtest note 9): at 0 health the player is down, never stuck.
  The down lasts `DownedMaxSeconds` (8) at most. With the partner's revive coming the HUD
  shows a ring emptying over the 8 s and "[Clint is coming]" (his name), "[Hold on]" once
  he is within 250 cm; with none coming (spent this fight, or no partner) the ring and a
  "[Any key]" placeholder, and any key but the pause keys ends it, ignored in the first
  0.5 s so a held attack cannot skip it. No revive by 8 s, or the key: she dies of it,
  "[You're down]" over a 1.5 s fade, and the last autosave loads (last checkpoint or
  safehouse). A revive after that does nothing.
- Nobody attacks a downed player: a thug whose target is down drops a burst or draw,
  lets a swing already under way finish (it cannot hurt her: the health is at 0 and
  dead-flagged, so no damage lands at all), then circles her at 450 cm. Back up, they come
  again.
- The reload resets the fight: placed thugs come back from the save (alive at full health
  and calm, or dead), crime thugs are not saved so they are gone, a challenge run is over,
  the input lock and the grey post process go with the old pawn. Dying (or Restart) in a
  flashback's playable scene now ends the scene too, as leaving it would (before, every
  later save was refused as "a playable scene is running").
- Player health regenerates after `RegenDelay` seconds without damage at
  `RegenPerSecond`, up to `MaxHealth`. Guards and bosses never regenerate.
- Headshots: `WeaponComponent` multiplies damage by `HeadshotMultiplier` when the hit
  bone name is in the `HeadBoneNames` set (`head`, `neck_01`). A guard with default
  stats dies to 1 headshot or 3 body shots from the pistol.

## Boss phases
- Phases are ordered by descending `HealthThresholdPercent`. Phase 0 is active from the
  start (threshold 100) and entering it is silent: no `OnPhaseChanged`. A boss with N
  phases has exactly N-1 transitions.
- A transition happens when health percent drops strictly below the next phase's
  threshold. Overkill that crosses two thresholds in one hit advances to the lowest
  matching phase and fires `OnPhaseChanged` once, with `NewPhase` being the final one.
- The killing blow never advances a phase, even if it also crosses a threshold. Health
  reaching 0 fires `OnDeath` from the health component; the phase component stays on
  whatever phase it was in. The boss Blueprint handles the kill.
- During a transition with `bInvulnerableDuringTransition`, invulnerability is raised
  first, then `OnPhaseChanged` broadcasts (so listeners see the boss already protected),
  then after `TransitionSeconds` invulnerability is restored to its previous state (so a
  boss that was already invulnerable for scripted reasons stays that way). With no world
  to run a timer, restore happens immediately after the broadcast.
- `BehaviorTag` on each phase is an `FName` written to the boss's blackboard key
  `Phase` by the boss Blueprint on `OnPhaseChanged`. The behavior tree branches on it.
- Boss health bars show one segment per phase, sized proportionally to the health range
  each phase covers.

## Weapon (hitscan pistol is LEGACY; the ammo, reload, and definition rules carry into the bow)
- Bullets trace on the `Weapon` channel (`ECC_GameTraceChannel1`, default Block), never
  `Visibility`: the engine's Pawn and CharacterMesh profiles ignore Visibility, which is
  why shots passed through guards in the first playtest. Every damageable character
  blocks `Weapon` on both its capsule and its mesh. The capsule guarantees the hit; a
  second trace against the mesh recovers the bone for headshots. `bTraceComplex` stays
  false.
- `OnHit(HitActor)` fires when the trace lands on an actor with a health component. The
  HUD flashes the crosshair white for 0.1 s on it.
- `Fire()` does nothing if `bHasWeapon` is false (Frank starts unarmed; `GiveWeapon()`
  from a pickup arms him).
- `Fire()` does nothing if `CurrentAmmo == 0`, if reloading, or if less than
  `1 / FireRate` seconds have passed since the last shot. An empty click sound plays in
  the first case.
- `Reload()` does nothing if the magazine is full, if reserve is 0, or if already
  reloading. After `ReloadSeconds`, moves `min(MagazineSize - CurrentAmmo, ReserveAmmo)`
  rounds from reserve to magazine. Firing during reload is blocked; sprinting cancels it.
- `OnAmmoChanged(Magazine, Reserve)` fires on every change.
- Range: a line trace of `Range` units from the camera. Past range, no hit. Damage
  falloff (stage 3) is a curve asset from 0 to `Range`.
- Spread (stage 3): base spread in degrees, multiplied while moving, reduced while
  aiming. Applied as a random cone on the trace direction.

## Takedown
- Valid target: actor with tag `Guard`, implements `ITakedownable`, within `Range`
  (default 150 units), and the vector from target to player, projected on the ground,
  is within `MaxAngleDegrees` (default 60) of the target's backward direction.
- A guard whose AI state is Alerted is not a valid target. Suspicious is fine. This is
  the stealth reward: you get the takedown if they haven't seen you yet.
- The player is locked out of movement and firing for `TakedownSeconds` (default 1.2)
  while the takedown plays. Guards can still shoot the player during this.
- `OnTakedownPerformed(Target)` fires once at the start of the takedown.

## LEGACY: player look and viewmodel (first person, to be removed)
- Look input is multiplied by the sensitivity from `UHawkeyeSettingsSubsystem` (default
  0.2, clamped to [0.02, 1.0]) and, while aiming, by `AimLookMultiplier` (0.7) as well.
  The character's own `LookSensitivity` property is only the fallback when no game
  instance subsystem exists (tests). Changing the setting takes effect immediately.

## Controller (Xbox layout; PlayStation pads map to the same keys)
| Control | Action |
|---|---|
| Left stick | Move. Radial dead zone 0.2. Light deflection walks, full deflection runs |
| Right stick | Look. Dead zone 0.25, eased (value^1.5) for fine aim, scaled by frame time at 180 deg/s yaw and 120 deg/s pitch times controller sensitivity; halved while aiming |
| A | Jump, ledge climb |
| B | Crouch hold, dodge tap, slide while sprinting, drop to hang at an edge |
| X | Strike (tap light, hold heavy) |
| Y | Takedown when a target is valid, otherwise interact |
| L3 (press left stick) | Sprint: one press toggles it on (the Sprint setting can make it a hold) |
| R3 (press right stick) | Send the partner to the point under the reticle (T on keys) |
| Right trigger | Draw and release |
| Left trigger | Aim |
| RB | Grapple |
| D-pad up / down | Tap: standard arrows / grapple arrows (on release). Hold 0.4 s: the world map / the phone. Left / right cycle slots |
| View | Inventory (tap), quiver wheel (hold); so the map is a hold of D-pad up, M on keys |
| Menu | Pause |

Stick up looks up by default (a double negation once inverted it; fixed 2026-09-27). `bInvertMouseY`
and `bInvertStickY` toggles live in Settings. Mouse look uses `LookSensitivity`; stick look uses `StickSensitivity` (default 1.0, clamp
0.2 to 3.0), both in the settings save. On-screen key hints switch to gamepad names when
the last input came from a pad.

## Settings
- `UHawkeyeSettingsSubsystem` (game instance) owns `FHawkeyeSettings` and persists it in the
  save slot `HawkeyeSettings` on every change. Missing or version-mismatched data yields
  defaults, never a crash. Only mouse sensitivity exists today; every future option (volume,
  subtitles, invert Y, key rebinds) goes in the same struct with a version bump.
- The Settings screen is reached from the pause menu. The game stays paused while it's
  open. Escape in Settings returns to the pause menu, not to the game. Back does the same.
- Sliders apply live: the value changes the game before the player leaves the screen, so
  they can feel it after resuming without a second trip.
- Time of day (built 2026-09-28): Night (default, the story's) or Day, a choice row under
  World in Settings, saved with the rest (settings version 6; a version 5 save migrates and
  comes up Night, anything older still yields defaults). `UTimeOfDaySubsystem` relights the
  district live on the change and at map load. Night is the level exactly as generated and
  is never touched on the default; Day is one row of its table (`GetPreset`), a clear winter
  afternoon (retuned 2026-09-29 from a first day that read blue and dim): the Moon light becomes
  a 16 lux sun, warm white with a little gold (1.0, 0.86, 0.68), 30 degrees up from the
  south-west; sky light 8.0 (from 3.0) tinted warm (1.0, 0.90, 0.80) against the blue sky it
  captures, so shade is blue but not black; fog 0.0015, slightly warm (0.18, 0.16, 0.13),
  starting 30 m out; PP_Global's exposure bias +2 to -0.3 (exposure stays pinned); the
  purple/cream grade neutral, vignette 0.2, stars hidden. Sunlit snow does not clip in
  day_street.png or day_park.png. Street lamps, their heads and the lamp buzz are off. Glows
  get the row's `GlowScale`, 3 by day (2.3 EV under the night makes every emissive about 5x
  dimmer): the level's M_Emissive materials (objective beacons, safehouse doors, the chapter-end
  arrow) by a swapped copy, lit windows at a quarter times that, and every glow a class sets
  through `UTimeOfDaySubsystem::SetGlow` (pedestal cap and icon, target faces, checkpoint rings),
  rescaled on every change. City_Ambience crossfades from the night street bed to the day bed
  over 2 s (the row's `DayBedWeight`, 0 at night and 1 by day; see "Audio"; this replaced the
  night bed turned down to 0.6 on 2026-09-29). Snow falls in both, unchanged (the snowfall component has no density
  setting). Going back to Night restores every recorded value exactly. Interiors and flashback scenes (no outdoor weather) and anything
  tagged Interior are left alone. `-TimeOfDay=Day|Night` overrides without saving, like
  `-Difficulty=`; the `hawkeye.TimeOfDay Day|Night` console command overrides both, also
  unsaved, until `hawkeye.TimeOfDay Saved` or the player changes the row. The engine's
  SunPosition plugin is the route to a real cycle later (more rows, blended by the clock).
- Lamps and the EMP share the lamps without fighting: the time of day owns each lamp's
  intensity, head glow and buzz; the EMP owns its lights' visibility. A lamp the EMP has dark
  keeps its head unlit and its buzz off through any time of day change, and when the outage
  ends it comes back as the time of day then in force says (off by day, lit at night). Before
  the first change the EMP restores what it recorded, as it always did.
  (`Hawkeye.TimeOfDay.EmpAndLampsDoNotFight`, `Hawkeye.TimeOfDay.GlowScalesByDay`.)
- Aiming blends FOV from `HipFOV` 90 to `AimFOV` 70 over 0.15 s, multiplies walk speed by
  0.6, and shrinks the crosshair gap from 8 px to 4 px. Sprinting cancels aim and reload
  and fades the crosshair to 40%.
- The crosshair is hidden while unarmed. Four green bars, 7 x 2 px, gap 3 px from centre
  (2 px while aiming), centred.
- Hip position is low and close, grip near the bottom edge. Aim brings the slide under the
  crosshair without pushing the gun far forward; the weapon never fills more than the
  bottom 40% of the frame and the left arm never crosses screen centre.

## Inventory and hotbar (planned next; semantics fixed now)
- Three hotbar slots: 1 Hands, 2 Pistol, 3 Rifle (rifle unused until a later mission).
  Number keys and mouse wheel switch slots. Switching takes `SwapSeconds` (0.4) during
  which Fire is ignored; the HUD shows the active slot.
- Hands are a weapon: left click punches (melee damage 15, stagger, 0.6 s cooldown, range
  120). Takedown (F) is unchanged and available in any slot when the target is valid.
- A slot is empty until its weapon is picked up. Selecting an empty slot does nothing.
  Picking up a weapon fills its slot and auto-switches to it only if Hands was active.
- Inventory (Tab) lists everything carried: weapons with ammo, keycards by id, spare
  ammo. Read-only for now. Opening it pauses the game like the pause menu does.
- Inventory clears at mission complete (before the end card) and on mission restart.
  Nothing carries between missions; each mission's data asset defines starting items
  (`StartingSlots`, default Hands only).
- Weapon stats move to `UWeaponDefinition` data assets (`DA_Weapon_Pistol`,
  `DA_Weapon_Rifle`): damage, magazine, reserve, fire rate, reload, spread, headshot
  multiplier, viewmodel mesh and pose name. `UWeaponComponent` holds the active
  definition and per-weapon ammo state keyed by definition.
- Viewmodel is `UFirstPersonArmsComponent`, a poseable mannequin attached to the camera,
  owner-only, no shadow, never affecting gameplay. UE 5.8 ships no arms asset, so poses are
  hand-authored as limb direction targets per bone and blended over 0.2 s. The pistol
  attaches to the arms' right hand (the template pistol is modelled barrel along +Y, hence
  the -90 yaw on the hand offset). Fire kicks the arms back 3 cm and up 2 degrees over
  0.05 s (return over 0.15 s) and flashes a muzzle light at the barrel end for 0.05 s.
  Reload dips them out of frame for `ReloadSeconds`. Sway scales with speed. Unarmed shows
  fists in a boxer's guard. A real arms pack from Fab would replace the poseable rig
  without changing any of these rules.
- The body mesh under the camera is the same mannequin, visible to everyone, with head,
  neck, and clavicles hidden for the owner so only one pair of arms is ever seen.

## Navigation
- Nav data is generated at runtime (`RuntimeGeneration=Dynamic`) and the game mode calls
  a build at BeginPlay if the map shipped with none. Maps are generated headlessly and no
  one presses Build Paths, so this is what makes guards able to move at all. The smoke
  test asserts a navmesh exists and at least one guard is moving after a few seconds.

## Melee and thugs (built 2026-09-26)
- Kate's strike is on V: tap for a light (15 damage, lands at 0.1 s, 0.3 s total, 120 cm
  sphere sweep, 20 cm lunge); hold 0.4 s for a heavy (35 damage after a 0.6 s wind-up,
  40 cm lunge) that knocks the thug down for 3 s, after which he blends from the ragdoll
  back to standing over 0.4 s (not yet seen in a full-length in-game fight). Takedown from
  behind on F is unchanged.
- A thug's bat hangs by his leg while walking, rises level behind him over the 0.6 s
  wind-up, swings across in 0.15 s, and drops back.
  Without clips, strikes are procedural lunges; with them, see "Combat animation clips".
- Dodge: tap Ctrl while moving and not sprinting dashes 300 cm in 0.4 s, invulnerable for
  the first 0.25 s, cooldown 0.8 s. Ctrl held crouches; Ctrl while sprinting slides.
- Thug melee: `EThugWeapon {Fists, Bat}` plus the gunner's pistol. Alerted and within
  250 cm they rush at 450 cm/s, close to 120 cm, and swing after a 0.6 s wind-up (a log
  line and a glowing mask) for 15 (fists) or 25 (bat) with a stagger; cooldown 1.2 s;
  after two swings they back off 2 m. Hitting a thug during the wind-up cancels the swing.
- Feel: 2-frame hit stop (time 0.1 for 0.033 s) on melee hits (4 frames on a landed heavy since
  2026-09-29, see "Combat readability"), camera shake when Kate is hit, a 0.1 s flash on a hit thug,
  and desaturation plus vignette under 40% health.
- Tracksuits: red suit, white stripes, black ski mask, all from one material on the old
  mannequin's single slot. No flashlight. Sight and hearing unchanged.
- CH01 placements: a roof pair (fists, bat) on the `cross_block` roof tagged RoofPair, whose
  deaths complete `clear_roof` (between `cross_block` and `find_arrow`); a street pair
  (bat, gunner) patrolling 40 m of the Avenue A sidewalk by the park. Verify projects every
  thug's feet onto the navmesh.

## Thug variety and squad alert (built 2026-09-26)
- Thugs run `ST_Thug`, a headless-built StateTree with states Stunned, Reposition, Cover,
  Attack, Investigate, Patrol in priority order; the C++ mode picker remains the fallback
  (tests). Cover points come from `EQS_CoverPoints`, an EnvQuery authored in C++ (never
  save it from the editor), with the C++ candidate ring as fallback.
- Gunner: 0.8 s telegraph (pistol raised, glint, log line), then a burst of 3 shots of 12
  damage 0.25 s apart at 4 degrees spread. Takes cover after a burst or when aimed at
  within 6 degrees: nearest blocked point within 800 cm on his level, hides 1.2 s, peeks,
  new cover every 6 s. Backs off to 600 cm if the player closes inside 300 cm. EMP jam
  holds fire. Any hit breaks the burst.
- Archer (`BP_Archer`, `EThugWeapon::Bow`, Barney's people): 1.2 s draw with a purple
  glint, 30 damage, 5000 cm/s, leads the chest (or the centre when she crouches, so a
  parapet covers her); keeps 1500 to 2500 cm, zips to an anchor when the player closes
  inside 800 cm, aggressive only within 3000 cm with a clear line; any hit breaks the draw.
  Arrows leave from cheek height. His purple arrows are picked up as standard, with a
  one-time "Trickshot's arrow" toast. Two on the roofs facing `find_arrow` (OSM W248142394
  and W248142314), tagged ArcherPair; their sightlines are kept clear of rooftop clutter.
  The scripted duel shows a ducking player breaks every draw; archers may need to hold a
  draw briefly after losing sight.
- Squad alert: 1.5 s after a thug goes Alerted, thugs within 1500 cm with a line to him
  turn Suspicious toward the player's last seen position.
- Heavy (built 2026-09-27): `BP_Thug_Heavy`, 200 HP, a 60 x 110 cm riot shield on the left forearm blocking arrows and light strikes in the front 120 degrees (arrows stick in it); staggered only by a heavy strike, bola, or explosive; putty holds him; shield bash with a 0.8 s telegraph, 30 damage, 250 cm knockback, plus a slow bat swing; walks at 300. One on his own 20 m patrol by the park corner, tagged StreetGroup. For the music he is a Fight, not a Duel, unless two or more others are alerted with him (see "Audio").
- Archer holds (built 2026-09-27): on losing line of sight mid-draw he holds up to 2.5 s and fires within 0.2 s of the player reappearing in his cone; a 0.6 s loose window after a shot leaves him open. Scripted duel: won with 1 hit taken.
- Readability: a cream "!" for 0.6 s over a thug going Alerted, "?" for Suspicious; a thin health bar over damaged thugs within 1500 cm, fading after 3 s (2026-09-29: a larger red-orange "!" while he winds up a strike, see "Combat readability"). Scripted street fight against bat, gunner, heavy: won, 0 hits, 0 untelegraphed hits, fairness metric 0% staggered time.

## Guard AI states (stage 2/3)
- `Calm`: patrol. Hearing radius `CalmHearingRadius`, sight cone `SightHalfAngle`.
- `Suspicious`: heard something or saw something briefly. Walks to the stimulus,
  waits `InvestigateSeconds`, returns to `Calm`. Takedown still valid.
- `Alerted`: has confirmed the player. Attacks, seeks cover, alerts squad after
  `SquadAlertDelay` (1.5 s). Returns to `Suspicious` after `LoseTargetSeconds` without
  perceiving the player, then to `Calm`.
- Noise: player sprinting emits a noise event every 0.5 s with loudness 1.0. Walking
  0.4. Crouching 0. Gunshots 3.0. Takedowns 0.6 (the body drop).
- Guards animate from animation assets, not an animation Blueprint: `IdleAnim` below
  20 uu/s ground speed, `WalkAnim` above, switched only on state change. Guards carry a
  head-mounted spotlight (3000 cd, 25/35 degree cone) that turns off on death. The beam is
  the visible read of where they're looking; stealth design should treat it as the sight
  cone's visual.
- Guard death logs the killer and cause at Log level (takedowns name the attacker);
  every bullet hit logs actor, bone, and damage at Verbose so a playtest can be
  reconstructed from `Hawkeye.log`.
- Guard death is always visible. `GoLimp(Killer)` ragdolls the mesh (the mannequin's
  physics asset from the High feature pack; the Standard pack's copy is an empty stub and
  must never be used). If the mesh reports it is not simulating afterwards, a procedural
  collapse runs instead: 0.6 s tip of 85 degrees away from the killer, 20 cm drop,
  animation stopped. Either way the flashlight turns off and the capsule stops colliding.
  Which path ran is logged once per guard at Warning.

## Save data (built 2026-09-26 on SPUD)
- SPUD (MIT, vendored at `Plugins/SPUD`, commit 12a30da) persists the world; it built on
  5.8 unchanged. One slot, `HawkeyeCampaign`; automation runs use `HawkeyeCampaignAutomation`.
- Saved: Kate and Clint (transform, controller rotation, health, bow, quiver slot by slot,
  active slot; components are mirrored into save-tagged fields before a save and restored
  after a load; the grapple slot's count is written but ignored on load, so a save from before
  the grapple went unlimited, even one at 0, loads with the grapple ready), thugs (position, dead flag; a dead thug goes down again on load without
  dropping loot twice), campaign state (version, mission path, completed objectives,
  safehouses found, play time, which character was controlled). Not saved: stuck arrows,
  pickups, EMP lamp state, spawner-created actors, the partner's fight state.
- Autosave on objective completion, safehouse entry, new game, and after 60 s of roaming
  (the clock runs only while the player is alive, on the ground, and no thug is alerted; a
  fight pauses it). Death (the down running out, see "Health and damage"), or Restart,
  fades to black and loads the last save; with no save it reloads the level.
- Continue reads the header first: version mismatch starts a new game with a warning; a
  missing mission asset starts a new game with an error. A migration hook exists.
- Safehouse: `City_Safehouse` on 140 East 7th Street (OSM W248142338) facing the park. E
  heals to full, marks it found, autosaves, and opens Refill arrows, Save, Fast travel
  (stub), Chapter select (stub), Leave.
- Main menu overlays the paused district on first boot: Continue (when a save exists),
  New Game, Settings, Quit. The pause menu has Quit to menu.
- Interaction now also works when the player stands inside an interactable's zone (the
  camera trace alone couldn't reach from 350 cm behind her).

## Save data, original contract (stage 3)
- One slot, `HawkeyeSave`, autosaved at mission complete and at checkpoints. Manual save
  is not exposed.
- Contents: mission id, completed objective ids, player health, weapon magazine and
  reserve, checkpoint id within the level, flashbacks seen, `bKilledDoctor`,
  `bKilledWarden`, settings blob, play time seconds.
- Loading a save whose mission asset no longer exists starts a new game and logs an
  error. Never crash on stale data.
- Version field on the save. A version mismatch runs a migration function or, if none
  exists, starts a new game.

## Choices
- Two flags only: `bKilledDoctor` (M5), `bKilledWarden` (M7). Both default false.
- `bKilledDoctor` changes one line in M8's dialogue table (row `m08_frank_doctor_alive`
  vs `m08_frank_doctor_dead`).
- `bKilledWarden` changes the final skull decal material (`MI_Skull_Blood` vs
  `MI_Skull_Paint`).
- Nothing else in the game reads these. Keep it that way.

## Audio (built 2026-09-27, all synthesized, nothing downloaded)
- 45 MetaSounds (37 one-shots, 8 loops) under `/Game/Audio`, authored headless by
  `Tools/Editor/create_audio.py` through the MetaSound builder API. Nodes: Sine, Saw,
  Noise, state-variable and one-pole filters, AD envelopes, Multiply/Add/Subtract,
  RandomFloat, LFO, Trigger Delay/Repeat. Every sound has a `PitchVariation` input; the bow
  creak takes `Draw`, the zip hum `Speed`, the landing `Intensity`. A changed recipe
  deletes and rebuilds the asset at the same path.
- Wired: bow draw loop, release, whistle, impacts by surface (wood, stone, flesh), pickup;
  grapple fire, zip loop, landing; footsteps every 70 cm walking and 55 cm sprinting, a
  landing scaled by fall height, roll, vault and mantle effort; melee hits and heavy hits;
  thug telegraphs, bat swing, hurt, death, shield block, stagger; gunner shots; every
  trick arrow effect with the smoke hiss looping for the cloud's life; menu hover and
  click, objective chimes and toasts.
- Ambience: `City_Ambience` crossfades street hum to rooftop wind above 10 m. The 20 lamps
  nearest the park carry a 120 Hz buzz that the EMP silences with the light and that is off
  by day with the lamps. Two street beds (2026-09-29): `MS_Amb_Street` by night (the mains
  drone, a traffic rumble, far horns) and `MS_Amb_StreetDay` by day (wind through the block,
  a wider and brighter traffic hum with tyre hiss, a far horn every 20 to 50 s, no mains
  drone). Both loops run; the time of day's `DayBedWeight` (Night 0, Day 1) splits the street
  level between them, `night = street x (1 - w)`, `day = street x w`
  (`HawkeyeAudioMath::ComputeStreetBeds`), and a change of the weight fades over 2 s instead
  of the usual 1 s (`Hawkeye.Audio.DayBedSelection`). The day bed's path is a C++ default on
  `AHawkeyeAmbience`, so the generated map needs no rebuild for it.
- Music (built 2026-09-29, a first pass for feel, not a score): `UHawkeyeMusicSubsystem`
  (world) plays `MS_Music_Score`, one looping MetaSound that holds every layer on one clock
  (90 bpm, D minor), and sets its five gain inputs every frame: `PadGain` (a low D minor pad),
  `PulseGain` (a low D pulse on each beat, a ghost on the off-beat), `PercGain` (an off-beat
  tick and a thump on 2 and 4), `MotifGain` (two notes, D3 then A3 a beat and a half later,
  every two bars), `DroneGain` (a dark low drone). One graph instead of separate loops keeps
  the layers sample-locked without the engine's Quartz clock (Quartz only earns its keep with
  separate sources; not used). States, from the thugs, the player, the crime and the
  challenge subsystems:
  - Roam: nothing, or the pad at half at night or while a challenge runs.
  - Alert: one thug alerted (or a crime on within 40 m of Kate): the pulse.
  - Fight: two or more alerted, or an alerted one within 8 m: pulse and percussion.
  - Duel: an alerted archer or anything with a boss phase component, or the heavy with two or
    more other thugs alerted beside him (2026-09-29): pulse, percussion and the motif. The heavy
    alone, or with one other, is an ordinary Alert or Fight (`HawkeyeMusic::HeavyDuelOthers`).
  - Win: every thug alerted in this fight is down (dead, limp or despawned) and at least one
    went down: all layers out in 1 s under `MS_Music_Win` (a 2 s D major resolve), then Roam.
    A thug alerted during the sting goes straight back to the fight; a crime starting nearby waits
    for the sting to end. Once per fight (2026-09-29): the won fight does not sting again until
    someone new is engaged or it is forgotten (the crime screenshot pass had a crime starting after a
    won fight flip Win and Alert every frame, `Hawkeye.Music.WinOncePerFight`).
  - Downed: Kate is downed: every layer out in 1 s, the drone in. On the revive the music
    goes straight to what the fight is.
  Rising is immediate. Falling waits: the level must be lower for 4 s before it drops, one
  step at a time (Duel to Fight to Alert to Roam), and any rise resets the wait. Layers fade in
  over 1.5 s and out over 2 s (1 s into Win and Downed). A thug counts toward "this fight"
  once he is alerted; the fight is forgotten when the music reaches Roam.
  A pause screen (map, inventory, pause menu and settings, safehouse menu, phone, results card,
  flashback: anything that pauses the world) holds the sound as well as the state machine
  (2026-09-29): the subsystem ticks under a pause only to notice it and pauses the score and any
  win sting (`UAudioComponent::SetPaused`); closing the screen resumes both where they were, in
  the same state. Logs `Music: paused` / `Music: resumed` (`Hawkeye.Music.PausesWithPauseScreens`).
  Every transition
  logs `Music: <from> -> <to>` at Log with the numbers behind it. The graph peaks near 0.35
  of full scale and `SCL_Music` defaults to 0.6 on the slider (0.36 gain), so the music sits
  well under the effects. Rules in `Source/Hawkeye/Audio/HawkeyeMusicRules.h`; tests
  `Hawkeye.Music.*`.
- Sound classes `SCL_Master`, `SCL_SFX`, `SCL_Ambient`, `SCL_UI`, `SCL_Music` and the mix
  `SMX_Settings` drive Master, Sound effects, Ambience and Music sliders in Settings
  (Music added in settings version 9, default 0.6; a version 5 to 8 save migrates and gets
  the default).
  Attenuations: `ATT_World` 300 to 3000 cm, `ATT_Lamp` 50 to 400 cm. Naming: `MS_` for
  MetaSounds, `SCL_` classes, `SMX_` mixes, `ATT_` attenuations.
- Verification: `Hawkeye.Audio.Smoke` plays every sound in a `-game` run and checks it
  plays; every trigger logs at Verbose as `Sound:`. Sound classes and the mix stay loaded
  for the whole run (a map change once spammed thousands of missing-class warnings).
- Open: the animation sample's own footstep events may double Kate's footsteps.

## Effects (built 2026-09-27, engine Niagara templates, nothing downloaded)
- 18 Niagara systems (33 emitters, 7 materials) under `/Game/VFX`, each a copy of one of
  the Niagara plugin's template emitters (`/Niagara/DefaultAssets/Templates/Emitters/`)
  with module settings changed headless by an editor-only C++ builder
  (`Source/Hawkeye/Vfx/HawkeyeVfxBuilder`) driven by `Tools/Editor/create_vfx.py` recipes.
  Python alone can copy templates but cannot edit module values, which is why the builder
  exists. The old placeholder shapes show only if a system is missing.
- Templates per effect: OmnidirectionalBurst for smoke, EMP ring, fireball and explosion
  smoke, putty splat, impact dust, landing snow, bow puff, muzzle smoke (and as a steady
  stream for the smoke cloud's 6 s tail, the bola whirl, chimney wisps); DirectionalBurst
  for every spark, stone chips, wood splinters, footstep kick; SimpleSpriteBurst for
  flashes; Fountain with a ribbon renderer for arrow and bola trails; DynamicBeam for the
  zip line and gunshot tracer; RecycleParticlesInView (GPU) for snowfall, 800 flakes
  following the camera. Scorch decal placed by a ground trace; EMP chromatic aberration is
  a post-process material.
- Chimney smoke on the 6 chimneys nearest the player at load. Frame time 7.4 to 7.7 ms.
- Flash lifetimes (2026-09-29): a `SimpleSpriteBurst` flash keeps the template's own lifetime unless its
  Lifetime Mode is set to Direct Set, and Lifetime Min/Max are then never read; `create_vfx.py`'s
  `flash(..., direct_life=True)` sets it. Every flash uses it: `NS_MeleeSpark`'s core and glow (0.08 s),
  `NS_HitSpark`'s star (0.2 s), `NS_ParryRing`'s flash (0.08 s; the ring itself, 0.22 to 0.3 s, is a burst and
  was always honoured), `NS_MuzzleFlash`'s flash (0.05 s), `NS_AnchorSparks`' bite (0.12 s, 120 cm and a hot
  warm core so it reads from the street 20 m off; a disc round the anchor block, not a fireball), `NS_EmpPulse`'s
  flash (0.14 s) and `NS_Explosion`'s core (0.1 s; the fireball, sparks, smoke and scorch still linger).
  Checked by particle count in the screenshot passes: `arrow_hit_spark_later.png` (0.5 s after the hit,
  nothing of `NS_HitSpark` left), `muzzle_flash_later.png` (0.3 s after the shot, no flash or sparks; the smoke
  puff may stay), `anchor_spark_later.png` (0.5 s after the bite, no bite flash), `parry_flash.png` (0.17 s
  after the parry, no flash, the ring out), `emp_flash_later.png` (0.5 s after the flash tick, `Flash` at 0, the
  ring may remain) and `explosion_flash_later.png` (0.6 s after, `Core` at 0, sparks and smoke may remain) fail
  if a flash is still alive. The `emp_flash` and `explosion_flash` shots stop time 0.05 s in; `vfx_explosion.png`
  is stopped 0.15 s in too, because as a timed capture it was sometimes taken from the editor's own level
  viewport instead of the game's (a night-sky frame with the axis gizmo) once the explosion's core lived only
  0.1 s. `Hawkeye.Screenshot.Vfx` runs the effect shots on their own.
- Open: the explosion still reads washed out (its light plus camera-shake blur); the footstep
  kick is faint. (The arrow's hit spark competing with the thug's hit flash: melee now has its own
  small `NS_MeleeSpark` and the flash dominates, see "Combat readability".)

## Narrative plumbing (built 2026-09-27; placeholder text only, story is written with Cameron)
- Phone (`UPhoneWidget`, P or hold D-pad down 0.4 s): contacts and threads from
  `DT_Messages` (Id, Sender, Text, Trigger: ObjectiveCompleted / ObjectiveStarted /
  ChapterStart / Event, TriggerObjectiveId, TriggerEvent, ChapterId, DelaySeconds,
  bRead). A message arrives when its trigger fires, with a 2 s HUD notification and an
  unread badge. Read state is saved. CH01 has four placeholder rows.
- Chapter title card (`UChapterTitleWidget`): number, `OpeningTitle`, `OpeningSubtitle`
  from the mission definition, 3 s hold then fade, skippable, input live. Seen-state saved.
- Chapter end: `AChapterEndInteractable` completes an objective on examine, plays a 2 s
  close-up camera push, then the end card (`EndCardLine` from the mission), then the
  flashback if set, then back to roaming with a "[Chapter complete]" toast. CH01's is the
  purple-fletched arrow in `City_ChapterEndTower`; the find_arrow trigger volume is gone.
- Flashback playable scene: `PlayableScene` and `ReturnPointLabel` on the definition.
  After the slides the scene map loads; the district is saved first, saves are refused
  inside the scene, and its mission completion returns to `City_SceneReturn_<id>` with
  state restored. `DA_FB00_Placeholder` with three "[Slide N]" slides leads to
  `L_Scene_Placeholder`, a 20 x 20 m room; CH01 currently plays it.
- `UDialogueSubsystem`: `PlayLine(Row)` shows speaker and subtitle and plays audio if set;
  a queue; `PlaySequence(Name)` from `DT_DialogueSequences` (Sequence, Order, Line,
  GapSeconds) survives traversal and pauses in combat. Banter routes through it.
  `DT_Dialogue` gained Audio, DurationSeconds, and a Scripted situation banter never picks.
  One placeholder sequence `seq_ch01_open` with three "[line]" rows.
- All CH01 text is bracketed placeholders: "[CH01 title]", "[CH01 subtitle]",
  "[End card line]", "[Grills text 1]" and so on. The 48 banter lines are the only
  written lines in the game.

## Small rules added 2026-09-27
- D-pad down: hold 0.4 s opens the phone; a tap selects arrow slot 2 only on release.
- Phone messages still waiting on their delay are saved with the time remaining.
- Snowfall runs only when the map's game mode sets `bOutdoorWeather`; the placeholder
  scene room turns it off and pins its exposure two stops down.
- Explosion blast light 400 cd; motion blur off for 0.3 s during the blast.
- A chained zip keeps ignoring the roof it just left until clear of it.

## Melee depth (built 2026-09-27)
- Combos: light, light, light do 15, 15, 25 (the third knocks back 150 cm), each input
  within 0.35 s of the previous light's hit landing or, if it hit nothing, of its hit window
  ending (2026-09-28: punching the air still runs Light1, Light2, Light3; before, a miss put
  the chain back at Light1). A heavy anywhere in the chain does 35 and knocks down, and ends
  the chain, hit or miss. The HUD counter and the x5 bonus count only landed hits: counter
  from x2, clears after 2 s; at x5 it turns purple and adds 20%.
- Parry: a strike tap during a telegraph from a thug within 250 cm and in front staggers
  him 1.5 s with no damage taken, a 4-frame hit stop, `NS_ParryRing`, and `MS_Parry`.
  Works on fists, bat, the heavy's bash and slow swing; cancels only the first shot of a
  gunner's burst; never on archers.
- Finisher: F on a staggered or knocked-down thug within 200 cm (the stealth takedown is
  tried first). 1.2 s: lunge, camera 40 cm in, time 0.5 for 0.4 s, lethal at 0.35 s,
  thrown ragdoll; input locked, invulnerable. With the bow up it's a sweep. Gate (2026-09-27):
  only a knocked-down thug, a parry-staggered thug, or one hit by a combo ender within
  the last 1.0 s qualifies; ordinary 0.5 s hit staggers do not. Fight times barely changed.
- Hit lean: 5 degrees for 0.2 s away from the hit direction, on thugs and Kate, through
  the IK post-process graph. Strike poses: light punches the right hand out over 0.1 s
  and back over 0.2 s; heavy both hands over 0.25 s; bow finisher sweeps the bow hand.
- Soft lock: dodges turn toward the nearest thug within 400 cm in front over 0.1 s (strikes use
  the melee assist below since 2026-09-28). A dodge during a telegraph within 300 cm gives 0.1 s
  of slow motion.

## Melee assist (built 2026-09-28, from playtest note "punching in their general direction")
- Where she means: the move stick's direction if it was pushed in the last 0.15 s, else the
  camera's forward. A strike (every light of the chain and the heavy) picks the best living thug
  within 350 cm (flat, 200 cm up or down) and 60 degrees of it: the lowest distance / 350 plus
  angle / 60, so one straight down the stick beats a nearer one off to the side. The log names the
  pick ("light assist picks BP_Thug_C_2 at 240 cm, 18 deg off where she meant") or says nobody.
- She turns to him over 0.1 s as before, then keeps facing him (up to 720 degrees per second)
  for the rest of the wind-up if he moves. The blow goes at him at the press.
- Closing the gap: a root-motion strike clip is warped to 90 cm short of him (the warp target
  follows him through the wind-up; inside 90 cm it only turns her). Without one (procedural, or an
  in-place clip) the lunge grows to end 90 cm short of him, never less than the attack's own lunge,
  never more than 260 cm, and takes the longer of its own time and 0.8 of the time to the hit.
- Kate's strikes sweep a 60 cm sphere (35 before) 120 cm ahead.
- Forgiveness: if the sweep finds nobody, the thug picked at the press is still hit when he is
  alive, within 1.3 times the attack's reach (Range + Radius, 234 cm for her strikes, measured to
  his capsule's edge) and within 30 degrees of the swing's direction when the hit lands (with a
  clip, at any tick of its hit window). So a thug stepping back mid-swing is still hit; one who
  was never a valid pick is not.
- Unchanged: the parry is tried first with its own rules (250 cm, 70 degrees); dodges keep the
  stick's direction and the 400 cm soft lock for facing; thugs' swings get no assist.
- A clip-timed wind-up's clock runs (2026-09-28 fix): `GetPhaseRemaining` counts down while the
  clip's hit window has not opened, so a thug's telegraph elapsed time is right and Hard's parry
  window (not the first 0.1 s) opens on clip swings. Before, it read 0 and Hard never parried.
- Scripted fights after these changes (standalone `-game`, seconds / hits taken / Kate's health):
  Story roof 4.4 / 0 / 100, duel 9.1 / 0 / 100, street 15.3 / 1 / 100; Normal roof 4.4 / 0 / 100,
  duel 8.9 / 0 / 100, street 17.0 / 3 / 100; Hard roof 4.4 / 0 / 100, duel 9.1 / 0 / 100, street
  17.1 / 3 / 63. All won, 0 untelegraphed hits.

## Combat readability (built 2026-09-29, from playtest note "combat isn't super clear at this stage")
Rules in `Source/Hawkeye/Combat/CombatReadability.h` (pure, tested by `Hawkeye.Melee.TargetMarker*`,
`Hawkeye.Camera.FightCamera*`, `Hawkeye.Melee.HitStopAndTelegraphGlyph`); drawn by
`UHawkeyeThugOverheadWidget`.
- Target read: the thug the melee assist picks for a swing that starts gets a thin ring on the ground
  round his feet (his capsule radius plus 12 cm, 2.5 px on a 1 px dark edge so it reads on snow, Kate's
  purple from the palette) and his
  health bar drawn 30% whiter. Full while that swing runs, fading to nothing over 0.3 s after it ends;
  a new pick moves it, a swing at nobody clears it, his death or removal clears it. A knocked-down
  thug's ring follows his body. One marker, Kate's only.
- Telegraph glyph: while a thug winds up a strike a parry answers (fists, bat, the heavy's bash and
  slow swing), the "!" over him is the telegraph's, not the alert's: 36 px bold (the alert glyph is
  20), a 2 px outline, the palette's danger colour (default red-orange 1.0/0.16/0.03 linear; deuteranopia hot
  pink, protanopia orange, tritanopia orange-yellow, each kept apart from that palette's other four
  accents), growing to 1.3x by the hit and pulsing 5 times a second by up to 0.12x (reduce flashing
  scales the pulse to 30%, the growth stays). A gunner's raised pistol gets it only within 600 cm of
  her; an archer never does (his purple glint is the tell).
- Glyph anchor (2026-09-29): the alert "!" and "?" and the telegraph "!" sit on his head bone plus 20 cm
  (`GlyphHeadClearance` on the thug, about the top of his head), not on his capsule's top plus 28 cm, which
  put the "!" about 60 px over his head at close range. The health bar sits on the same anchor (2026-09-29,
  second pass: it had kept the capsule's top plus 28 cm, about 65 px over his head close up, so a hurt thug's
  "!" still rode high). The stack, bottom up: the bar (its bottom on the anchor), then the parry line under a
  telegraph, then the glyph, each `StackGap` (4 px, dark edges included) over the one below; with no bar the
  glyph or parry line sits on the anchor itself. The glyph's gap is measured from its ink (its baseline less its
  outline), not its text box, which runs on below by the font's descent. They never overlap (`HawkeyeCombatReadability::ComputeGlyphBottom`,
  tested in `Hawkeye.Melee.HitStopAndTelegraphGlyph`). A thug with no head bone uses the capsule anchor.
- Parry line: under the telegraph glyph, an 18 x 3 px cream line on a dark edge, drawn only while a
  tap now would parry him: in the difficulty's window (Hard: not the first 0.1 s), within 250 cm and
  70 degrees of her view, 150 cm up or down (`AHawkeyeCharacter::CanParryNow`, the same test the
  parry itself uses).
- Hit read: a landed light keeps 2 frames of hit stop (0.033 s); a landed heavy gets 4 (0.067 s, as
  the parry) and a camera punch, the lens 2.5 cm in toward her and back over 0.1 s of real time (a
  half sine), times the camera shake scale (0.3 with reduce camera shake). Every landed strike puts
  `NS_MeleeSpark` on the thug's capsule surface toward her, at the height of whichever of her fists or feet
  is nearest him (clamped to his knees and his face): a hot-gold 26 cm core (blended, so it keeps its colour
  on his white flash) in a 46 cm additive glow, both 0.08 s, and 14 short streaks, scale 1 on a light and 1.4
  on a heavy (times 0.6 to 1 with reduce flashing). When her own body would hide that point from the
  player's lens (her axis within `SparkClearance`, 40 cm, of the line to it) it slides round his body to the
  side the lens sees, up to 0.9 of his radius across. The thug's own 0.1 s body flash is the dominant read;
  the spark only marks where. No damage numbers. (2026-09-29, from the first `hit_spark.png`: the old
  white-violet core was lost in the flash and behind her head, and its sprite outlived its 0.06 s by over a
  second because the template's own lifetime was never replaced; the spark's flashes now set it directly.)
- Ground thud: when a thug's body meets the ground, `MS_Roll_Thump` plays and `NS_KnockdownDust` throws
  dark grey slush up and out from the ground under his pelvis (`GroundThudSound`, `GroundDustVfx` at
  `GroundDustScale` 1 on the thug). A knockdown his clip plays (a heavy, a trip) thuds 1.3 s after he goes
  over (`KnockdownClipThudDelay`: the clip's pelvis is within 35 cm of the ground by then); a ragdoll
  (a blast, a death standing) 0.45 s after (`GroundThudDelay`). Killed mid-fall: no later than 0.45 s more.
  Killed while already on the floor: no second thud. (2026-09-29, from `knockdown_dust.png`: at 0.45 s
  the clip still had him half-way down, and the white `NS_LandingSnow` puff it used did not show on
  snow; `M_Vfx_Smoke` is also faded out within 60 cm of geometry, so the slush uses `M_Vfx_Snow`.)
- Seen in `Hawkeye.Screenshot.Melee` (standalone, 2026-09-29), her own camera: `target_ring.png` 0.1 s into a
  light at a hurt thug (the ring at his feet, his bar), `hit_spark.png` with time stopped in the tick the
  light lands (the gold spark on his chest beside her head, 425 cm from the lens), `knockdown_dust.png`
  0.2 s after a heavy's knockdown thud (the slush round his shoulders). `heavy_strike_2.png` is skipped
  while her set has one heavy.
- Fight camera: two or more alerted, living thugs make a fight when one is within 900 cm (flat) and the
  others within 1500 cm (`FarRadius`, so a gunner hanging back counts), all within 300 cm up or down
  (counted every 0.1 s). With nobody within 900 cm nothing counts. The hip arm lengthens by 70 cm and the lens tips down 4 degrees, blended
  at a constant rate over 0.5 s in and 1 s out, smoothstepped. It is multiplied out by the aim blend
  (the bow's camera is untouched) and by a finisher's push-in (the push reads from the plain hip arm,
  as before; there is no push-in on ordinary strikes, and the fight camera holds through them).
  Indoors the hip arm still never passes `IndoorArmLength`. The spring arm's probe still pulls the
  longer arm in against walls. The log says "fight camera in (2 alerted thugs, one within 900 cm)" and "out".
- Measured (standalone `Hawkeye.Screenshot.Melee`, 2026-09-29): in the open street with two thugs at
  3.2 and 4.4 m the arm is 420 cm (hip 350), the lens 430 cm from the pivot, pitch -4 degrees, Kate's
  head and feet at -0.07 and +0.23 of the screen from its centre, both thugs in frame. With her back
  150 cm from a building the probe holds the lens at 147 cm; she is drawn (not hidden), head at -0.36,
  feet at +0.50 (the bottom edge), both thugs in frame. The shot fails if she is hidden or her head
  or feet leave the screen.
- Clip order for readability (2026-09-29): Kate's heavy is the Roundhouse only. The Surprise Uppercut
  (side on, its hit window shows her back, hips 47 degrees turned, the striking hand 46 cm out to her
  right at 101 cm; the roundhouse's foot is 61 cm out at 83 cm) is `"in_set": false` in the manifest
  until a better second heavy exists; `heavy_strike_2.png` is skipped while the set has one heavy. The
  thug's first fists swing is Jab To Elbow and Standing Melee Punch second (0.3 s into the telegraph
  the jab has his guard up, right hand at 160 cm; the punch is still holding its first frame, hands
  low at 107 to 112 cm, which reads as standing about).

## Arrows in bodies (fixed 2026-09-28, from playtest note "they're like a foot away from her")
- The cause: an arrow meets the capsule (34 to 42 cm radius), not the body, and was embedded at
  the capsule's surface plus 12 cm, then attached there; with the downward angle from a roof the
  capsule's top met it further out still. A mesh without physics bodies left it on the capsule.
- Now an arrow that hits anyone with a skeletal mesh walks its flight line (from 30 cm before the
  capsule impact to 150 cm past it) against the mesh's physics asset shapes at the current bone
  transforms, whatever the mesh's collision, until it enters one. The tip goes 5 cm into that
  surface, the arrow points along its flight, and it attaches to that bone, so it moves with the
  body and falls with a ragdoll. If the line passes through the capsule without touching a body
  (between the legs, beside the waist), it goes into the nearest body to the line at its closest
  point. A mesh with no physics asset takes the nearest bone to the line.
- Damage and headshots use only a real entry: a line that never entered the head is never a
  headshot, even when the nearest body was the head. Shields and walls are unchanged (12 cm).
- Pickups are unchanged: a stuck arrow in a thug or on the ground is recovered within 150 cm; one
  in Kate is not.
- Partner fire (2026-09-28, from Cameron's log: Kate downed by Clint's arrow after its thug died
  mid-flight): an arrow shot by Kate or Clint flies through the other of the pair, whoever is
  player-controlled; a hit on the partner that gets through anyway sticks and does nothing (no
  damage, no stagger, no trick effect). Bola and putty only ever act on thugs. The explosive's blast
  hurts the shooter's partner at half its falloff damage; the shooter still takes her own in full.

## Bow aim assist (built 2026-09-28, from playtest note "aim assist for the bows")
- One setting, Aim assist: Off / Normal / Strong (default Normal), a choice row under Controls in
  Settings, saved with the rest (settings version 7; versions 5 and 6 migrate and come up Normal).
- Three parts, all only for the player, only against living, standing thugs she has a clear line
  to, at up to 6000 cm, aimed at their chest (30 cm over the capsule centre):
  - Magnetism: while aiming or drawing, a thug whose chest is within the cone (4 degrees at
    Normal, 7 at Strong) of the view slows the look input toward the centre (to 0.6 of the rate
    at the chest on a pad, 0.85 with a mouse, easing back to 1 at the cone's edge), and pulls the
    view toward his chest at up to 6 degrees per second on a pad (10 at Strong), a third of that
    with a mouse, weaker toward the edge. No pull while the reticle is already over his body
    (within his capsule radius of the line from his feet to his head), so aiming at a head is
    never dragged to the chest.
  - Snap: pressing aim (LT, right click) with a bow turns the view over 0.1 s onto the chest of
    the thug nearest the reticle within 12 degrees. A draw with left click alone does not snap.
  - Bend: on release, if the arrow's line would miss every thug, its launch direction turns up to
    2 degrees (4 at Strong) toward the lead-and-drop aim point on the nearest thug's chest within
    the magnetism cone of the release. A line that already hits someone is left alone.
- Difficulty scales all three: Story 1.25, Normal 1.0, Hard 0.5 of every cone, pull and bend.
  Off turns everything off. AI archers and Clint get none of it.
- Headshots still need real aim: the pull and the bend go to the chest and never act on a line
  already on the body.

## Ranged thug accuracy (built 2026-09-28, from playtest note "they rapid fire arrows and don't miss")
- Tracking lag: an archer or a gunner sees Kate as she was a lag ago (0.4 s Story, 0.32 Normal,
  0.25 Hard): her position then and her velocity then (from 0.15 s of samples before it), taken
  every 0.05 s. He leads from that: her position then plus that velocity over the lag and the
  arrow's flight (with the drop). A steady runner is led right; a turn inside the lag is not seen.
- Accuracy cone: every shot is scattered in a cone of a base (archer 0.75 degrees, gunner 6,
  was a flat 4) plus the angle her sideways speed sweeps across his view in 0.2 s (her velocity
  across his line over the distance, times 0.2, in degrees), plus 3 degrees while she is in the
  air, all times the difficulty's cone scale (1.4 Story, 1.0 Normal, 0.7 Hard). At 8 m, sprinting
  sideways (700 cm/s) is about a 10.8 degree cone at Normal; standing still is the base.
- Erratic movement: a change of more than 90 degrees in her flat velocity (both above 150 cm/s)
  in the last 0.5 s, or a dodge started in the last 0.5 s, makes the shot miss unless she is
  within 600 cm: it goes her capsule radius plus 50 cm wide of her, on the side she came from.
  Each shot of a gunner's burst checks it for itself.
- Archer cadence: his next draw starts no sooner than a random 1.5 to 2.5 s after his last
  release at Normal (3.0 to 4.0 Story, 1.2 to 1.8 Hard), so shot to shot is that plus the draw.
- Every shot is still telegraphed: the archer's draw and glint, the gunner's raised pistol.
- Measured by `Hawkeye.Thug.StrafingKateHitRate` (the thugs' own aim functions and a flown arrow,
  or the pistol's line, against her capsule, 20 shots each): hits out of 20 at Story / Normal / Hard,
  archer at a 700 cm/s sideways sprint at 8 m 3 / 6 / 11, standing at 8 m 20 / 20 / 20, running
  500 cm/s at 20 m 7 / 11 / 11; gunner sprinting at 8 m 1 / 2 / 7, standing 10 / 9 / 14. The test
  holds Normal's sprint to 7 or fewer (about one in three) for both, Story at or under Normal, Hard
  at or over it, and a standing Kate hit at least 15 times by the archer.

## Side challenges (built 2026-09-27; names are placeholders)
- `AChallengeStart` pedestals (purple-lit, E to start) and one `UChallengeDefinition` per
  challenge under `/Game/Challenges`, planned by `create_challenges.py` and placed by the
  generator. Three archery ranges on rooftops 133 to 178 m from the start; three traversal
  routes from street corners (the third is 331 m out because routes needing a mantle are
  rare).
- Archery: 12 ringed glowing targets 12 to 40 m out on other roofs and fire-escape
  landings with clear lines, 3 or 4 moving on a 6 m track. Score 10 / 5 / 2 at radii 8 /
  18 / 30 cm, 60 s, medals at 36 / 60 / 84, reward an arrow refill.
- Traversal: 8 floating 200 cm checkpoint rings across roofs using grapple, mantle, and a
  fire-escape descent; 150 s limit; gold under 60 s, silver under 90, bronze under 120.
  Every planned grapple leg passes the grapple's own clearance check.
- A run fails on time-out, on leaving a 100 m radius, or if the player goes down. Thugs
  within 60 m go Calm for the run. Best score or time and medal per challenge are saved.
  HUD panel during a run; results card with Retry and Leave; next checkpoint or remaining
  targets use secondary objective markers.
- Scripted: archery clears 12 of 12 in about 17 s (bronze by score); traversal in 18 to
  19 s (gold). Distant targets are small at 30 to 40 m. The pedestal light was too strong
  and is being dimmed.
- Finding them (2026-09-28, playtest note 13): the compass and world markers above; the
  pause menu's "Mark nearest challenge" puts a marker on the nearest pedestal (cleared on
  reaching it or starting any challenge); the first time the player comes within 40 m of a
  pedestal a "[Challenge nearby]" toast names it (once a campaign, ids kept in the save as
  `NoticedPlaces`). The cap and ring icon are a deep purple (0.5, 0.06, 1) at 0.35 / 0.6
  night emissive, since 2.5 clipped to white under the night's +2 EV; by day the glows take
  the time of day's GlowScale, 3 (checked in challenge_pedestal.png at night and day_pedestal.png).

## Difficulty and accessibility (built 2026-09-27)
- One difficulty setting, Story / Normal / Hard (default Normal), asked once at New Game
  and changeable in Settings; `UDifficultySubsystem::GetScalar` holds the whole table.
  Thug melee and gunner damage 0.6 / 1.0 / 1.4; thug health 0.8 / 1.0 / 1.2; archer draw
  1.5 / 1.2 / 1.0 s; parry window +0.15 / 0 / -0.1 s (Story holds an early tap up to
  0.15 s; Hard ignores the first 0.1 s of a telegraph); regen delay 3 / 5 / 8 s at
  10 HP/s (regen didn't exist before and applies to Kate only); fall damage 0.5 / 1.0 /
  1.0; Story adds 2 to each trick arrow cap (never the grapple, which has no count). Added 2026-09-28 ("Ranged thug accuracy", "Bow aim
  assist"): ranged tracking lag 0.4 / 0.32 / 0.25 s; ranged cone scale 1.4 / 1.0 / 0.7; archer
  gap before the next draw 3.0-4.0 / 1.5-2.5 / 1.2-1.8 s; aim assist scale 1.25 / 1.0 / 0.5.
  `-Difficulty=Story|Normal|Hard` on the
  command line overrides without saving. Every scripted fight is won on all three.
- Accessibility, all persisted (settings version 5; 6 adds the time of day; 7 the aim assist; 8 the
  sprint mode, and 5 to 7 migrate with it at Default): subtitle size
  20 / 26 / 34 px with a
  background opacity slider; hold or toggle for aim (default hold) and crouch (default
  toggle); a 1 px outline on the alert glyphs; four colour palettes (default,
  deuteranopia, protanopia, tritanopia) for the reticle, markers, hotbar, glyphs, and
  health bars; reduce camera shake to 30%; reduce flashing to 30% for the EMP split and
  the explosion light (the parry ring shrinks to 30% instead, it has no brightness
  input); Replay flashbacks in the pause menu, slides only, seen-list stored with settings.
- HUD scale 0.8 to 1.4 applies to HUD text, hotbar, markers, and compass.
- Challenge pedestal light 135 lm over 400 cm; the cap and icon read white rather than
  purple until the 2026-09-28 fix in "Side challenges".

## Street crimes (built 2026-09-27; names are placeholders; rules also in Source/Hawkeye/Crime/CrimeRules.h)
- `City_CrimeSpot_` actors: 8 street corners (each with a robbery escape point 58 to
  62 m along the sidewalk), 4 rooftops with anchors, and 2 to 4 alleys (2026-09-29); none
  within 40 m of a safehouse, a pedestal or the interior entrance's doorstep, all at least
  35 m apart.
- Alleys (2026-09-29): `create_crimes.py` casts a ray off every wall of every footprint within
  250 m of the PlayerStart to the next building; a 3 to 6 m gap whose middle is 8 m past every
  carriageway's edge, 1.4 m from every wall, off the park and clear of street clutter is a
  candidate, and it must join a sidewalk by a walk never nearer than 1.4 m to a wall, at most
  50 m on a 50 cm grid. On the East Village block: 461 rays found gaps in 71 passages; 297 fell
  in a keep-out or within 35 m of a spot; of 30 passages tried, 22 were dead ends (rear yards
  with no way out but through a building) and 5 too deep; 3 were placed (`City_CrimeSpot_12` to
  `_14`, 5.8, 4.5 and 5.4 m wide by footprint, 5.5, 4.0 and 5.0 m by the level's walls). Alley
  spots face along the passage (`ACrimeSpot::bAlley`) and take the mugging and the ambush; an
  ambush there lines its four up along the passage (pairs 5 and 3 m either side, 60 cm across,
  `UCrimeRules::AlleyRosterOffset`) instead of on a 5 m ring in the walls. A block with fewer
  than two alleys would get two spots on the park's interior paths instead. `verify_city.py`
  checks the count, the crimes, the width by traces and a navmesh path to the nearest corner
  spot.
- A crime is due every 90 to 150 s of roaming (not in combat, a challenge, or a chapter
  beat), at a spot 25 to 40 m away and 60 m from the last, preferring spots out of sight.
  One at a time. `hawkeye.CrimeInterval` and `hawkeye.CrimeType` cvars control it;
  automation never gets a crime unless the interval is set.
- Mugging: fists and bat thugs around a grey `BP_Civilian`; the victim is hit at 15, 30,
  and 45 s while an unalerted thug is beside him, and the third hit fails it. Robbery:
  a runner (380 cm/s) who leaves when Kate is within 20 m, a thug is alerted, or 40 s
  pass, and drops the loot when downed; picking up the loot ends it. Ambush: four thugs
  alerted from the start. Rooftop: two thugs and an archer.
- Stopping a crime gives +5 standard arrows (never grapple arrows, which have no count), a toast,
  and a per-type count in the save.
  Crime thugs despawn 60 s after success once the player is 40 m away, 20 s after failure.
- Scripted laps (standalone `-game`, `Hawkeye.Lap.Crime*`, 2026-09-29), each started by the
  subsystem's own rules through `hawkeye.CrimeType`, each paying +5 arrows, the toast
  ("[Crime stopped] / [Crime: <type>]   +5 arrows"), the count and the campaign save:
  mugging at the corner `City_CrimeSpot_0`, 11.8 s, 0 hits, the victim unhurt; robbery at
  `_0`, 6.4 s, 0 hits, the runner tripped by a bola 8 m from her 63 m short of his escape, the
  loot walked over; ambush in the alley `_12`, 13.5 s, 0 hits, a bola on the gunner then
  strikes, a parry and three finishers; rooftop on `_10` (13.7 m), grappled up from the street
  27 m out in one press, 9.2 s, 0 hits, two arrows, six strikes and a finisher. A crime waits out
  a fight: with a thug still alerted or Clint's fight clock running (10 s after the last blow) the
  schedule does not run, so back-to-back laps start 5 to 10 s after set-up. The laps found: a thug
  walking to where she landed a zip by a parapet asked for a goal off the roof's navmesh and failed
  (fixed: `AThugAIController::RequestMoveToLocation` walks to the goal's nearest navmesh point within
  2 m); and their own slot change on the release frame loosed a standard arrow instead of the bola.

## Safehouses and fast travel (built 2026-09-27; rules also in Source/Hawkeye/World/SafehouseSubsystem.h)
- Two safehouses: "[Safehouse 1]" at 140 East 7th Street (OSM W248142338) and
  "[Safehouse 2]" on Avenue B at East 10th (OSM W250264779), 278 m apart. The generator
  picks the second as the nearest qualifying storefront at least 250 m from the first,
  skipping buildings whose fire escape would cut through the sign. Both must be entered
  once to be discovered; undiscovered ones show greyed as "[Undiscovered]". Refill arrows tops up
  standard and trick arrows only; the grapple has no count to refill.
- Fast travel from a safehouse menu to any other discovered one: fade out 0.5 s, hold
  0.35 s during which Kate and Clint are moved to the destination door and the game
  autosaves with them there (destination recorded as last used), fade in 0.6 s; about
  1.5 s fade to fade, no map reload. Refused with a "[Can't fast travel now]" toast during
  a crime or challenge.
- Both safehouses always show on the compass, found ones as filled houses, unfound ones
  hollow. The pause menu's "Mark nearest safehouse" marks the nearest one whether found
  or not (it only looked at found ones, so with none found it did nothing but toast "[No
  safehouse found yet]": playtest note 12). The first time the player is within 60 m of an
  unfound one, a "[Safehouse nearby]" toast and the marker on it (once a campaign, in
  `NoticedPlaces`). Chapter select is still a stub. Crime spots keep 40 m clear of both.

## World map (built 2026-09-29, from "couldn't find the safehouses or the pedestals")
- M on keys, a 0.4 s hold of D-pad up on a pad (View is already the inventory on a tap and the
  quiver wheel on a hold, and R3 the partner mark, so the map takes the D-pad hold the phone
  already uses on down; a tap of up is still the standard arrows, now on release). Refused, like
  the phone, under any menu, the inventory, the phone, a flashback, a close-up or a chapter beat.
  The game pauses under it. M, Escape, B, Menu or D-pad up close it; Escape never also opens the
  pause menu. No minimap: the compass is the HUD.
- `UHawkeyeMapWidget`, painted in `NativePaint` with the engine's Slate draw calls (`MakeCustomVerts`
  for filled footprints, ear-clipped once by GeometryCore; `MakeLines` for streets; `MakeBox` and
  `MakeText`); no plugin. It draws `DA_EastVillage_Map` (`UCityMapData`, written by
  generate_city.py): footprints dark grey, the park a muted green, streets lighter grey at their
  carriageway width, on a near-black ground. North up (world -Y), the district's bounding box fitted
  into the screen less a 16 px gutter and the legend row.
- Icons (`FHawkeyeMapIcons::Build`, drawn in this order): the interior door, other secondary markers
  (a small purple diamond), safehouses (the compass's house, filled once found, hollow before,
  "[Unknown safehouse]" until found), pedestals (the medal, gold, silver or bronze for the best medal
  earned, the palette's purple before one), the crime in progress (a danger-red diamond with "!"),
  the current objective (the cream diamond), Clint (a small grey arrow) and the player (a purple
  arrow along her heading) on top. A marked place gets a cream ring; its own secondary marker is
  not drawn again. Icons take the HUD scale and the colour palette. A legend row runs along the
  bottom with each glyph and the key hints (keyboard or pad, whichever was touched last).
- A cursor (the mouse, or the left stick at 700 px/s, pushing the map when it meets the edge) over
  an icon (within 16 px) shows its name, medal and distance from the player; markable icons win
  over nearer ones. Enter, Space, A or a click on a safehouse or pedestal marks it through the
  "Mark nearest" plumbing (`USafehouseSubsystem::MarkSafehouse`, `UChallengeSubsystem::MarkChallenge`),
  so the world marker and the compass follow; marking one kind takes the other kind's marker down
  (one marked place from the map), and marking the marked place again clears it. Fast travel is
  not on the map; it stays in the safehouse menu.
- The wheel (x1.25 a notch) or the right stick zooms 1x to 3x about the cursor; a drag pans. The view
  never leaves the district; at 1x it is centred and does not pan. The map opens at 1x with the
  cursor on the player. (`Hawkeye.Map.*`, `map_open.png` in the Kate pass.)

## Combat animation clips (built 2026-09-27/28; pipeline in claude-docs/animation.md)
- A character's clips come from a `UCombatAnimSet` data asset (`DA_AnimSet_Kate`, `_Clint`,
  `_Thug`, `_Archer` under `/Game/Blueprints/Animation`), one soft montage per role: Light1,
  Light2, Light3, Heavy, Kick, Parry, DodgeForward/Back/Left/Right, HitFront/Back/Left/Right,
  Knockdown, GetUp, FinisherAttacker, FinisherVictim, FinisherBow, BowDraw, BowAimIdle,
  BowFire, BowNock. `CombatAnimSet` on `AHawkeyeCharacter` and `AThugCharacter` picks it.
- **The fallback rule.** A role whose montage is empty, fails to load, or will not play (no
  anim instance, wrong skeleton) does exactly what the game did before clips existed: the
  strike pose and lunge, the procedural dash, the hit lean, the ragdoll knockdown and its
  0.4 s blend up, the IK bow. Every rule in the melee, dodge, finisher and bow sections holds
  either way; a clip changes how it looks and, for strikes only, when the hit lands.
- Only Light3 has a data fallback: with no Light3 clip it uses Kick.
- Variants (2026-09-28): a role can hold several clips. The first in `Tools/Data/Anims/manifest.json`
  order that exists for that character is the role's slot (variant 0); the rest go in the set's
  `MoreVariants` for that role, in manifest order (`create_combat_anims.py` writes both; a clip marked
  `"in_set": false` is imported but left out). Each strike (the melee component's swings: Light1-3,
  Heavy, Kick) takes the next variant of its role in order, round again after the last, so with two or
  more no clip plays twice running; a role with one clip plays it every time, as before. The cycle is
  per character and per role (a chain's Light1 and the next chain's Light1 differ). The log says which:
  "... variant 2 of 2 for ECombatAnimRole::Light1: AM_Light1_Punching". Every other role (dodges,
  reactions, knockdown, finisher, bow) plays its first clip only. A variant that does not load gives way
  to the role's first clip.
- Clips are retargeted, never used on their source skeleton: Kate and Clint's onto the UEFN
  mannequin, thugs' and archers' onto the UE4 mannequin they wear. Additive source clips (Paragon's
  hit reactions) are baked to full poses. Standing clips must keep the pelvis within 35 cm of the
  reference height with the root held where the game holds it (locked, or extracted as root motion);
  the import reports any that do not, and the self-test fails past 15 cm. The root track carries only
  horizontal travel on the ground, never the pelvis's height (2026-09-28: Mixamo clips had the hips'
  90 cm on the root, and everyone playing one knelt).
- Strikes: Kate's light chain is Light1, Light2, Light3; her hold is Heavy; a thug's fists
  are Light1, his bat and the heavy's bash and slow swing are Heavy. `FHawkeyeMeleeAttack.AnimRole`
  names the role. A montage whose slot is `UpperBody` plays on the post-process instance
  over locomotion; any other slot plays full body (`DefaultSlot` on the sample's AnimBP for
  Kate and Clint, the post-process graph's `DefaultSlot` for thugs, whose main instance is a
  single clip).
- Hit timing with a clip: the sweep runs when the montage's `ANS_HitWindow` begins, and
  again each tick until it finds someone or the window ends; if nobody was found by the end,
  the swing missed. A montage with no `ANS_HitWindow` keeps the attack's WindupSeconds and
  RecoverSeconds timers while it plays.
- Chain timing with a clip: after a light lands, or misses (its `ANS_HitWindow` ended on nobody),
  the chain stays open (the 0.35 s timer does not run) until the montage's `ANS_ComboWindow` ends; while that window is open the next
  strike starts at once, cutting the current montage. A press between the hit and the window,
  or while the hit window is still open on nobody, is held and goes when the window opens. A montage with no `ANS_ComboWindow` keeps the
  0.35 s timer and ends the swing at RecoverSeconds.
- Kate's strike timing with clips (2026-09-28; each clip's start, end, rate and windows are in the
  manifest, in the source clip's seconds): Light1, Light2 and Light3 land 0.24 to 0.26 s after the
  input with either variant, Kick 0.29 s (Kicking and Side Kick), Heavy 0.39 s (the Roundhouse, her only
  heavy since 2026-09-29; the Surprise Uppercut, 0.41 s, is imported but out of the set). A light's montage lasts 0.54 to 0.81 s; its combo window opens
  about 0.1 s after its hit and closes 0.3 to 0.5 s after it, so the 0.35 s chain rule holds to within
  about 0.15 s either way. Pressed as each lands, three lights land at about 0.25, 0.6 and 1.0 s and the
  chain is over by about 1.4 s. The procedural lights still land at 0.1 s.
- Thugs keep their telegraphs: the telegraph decides when the hit lands, the clip only how it looks.
  A thug's strike montage opens its `ANS_HitWindow` exactly at the attack's WindupSeconds (0.6 s
  fists and bat, 0.8 s bash, 1.0 s slow swing), so parry and dodge windows are unchanged. It plays
  at a rate between 0.8x and 1.3x (`ClipFitMinRate`, `ClipFitMaxRate` on the melee component): a
  wind-up too long for 1.3x starts part way in, one too short for 0.8x holds its first frame (he
  squares up) for the difference. Kate's clips play at rate 1.
- Thug clip timings (2026-09-28; trims in the manifest, measured on the district's thugs by
  `Hawkeye.Smoke.ThugClipsStrikeOnTheTelegraph`): fists (Jab To Elbow first since 2026-09-29, 0.92x,
  no hold; then Standing Melee Punch, 0.25 s of wind-up, which holds 0.29 s then plays at 0.8x) land
  at 0.60 s; bat (Standing Melee Attack Horizontal, 0.87 s of
  wind-up, one clip for the bat, bash and slow swing) plays at 1.3x from 0.09 s in and lands at 0.60 s;
  bash 1.09x, 0.80 s; slow swing 0.87x, 1.00 s. Each swing is over (montage ended) 0.48 to 0.72 s after
  its hit. A clip knockdown falls within 1.4 s and holds on the floor; the finisher's victim clip
  starts at the sweep, so he is off his feet before he is thrown at 0.35 s.
- Motion warping (the engine's MotionWarping plugin, the component the sample already puts on
  Kate): a full-body strike clip with root motion is warped toward the soft-lock target, the
  warp target `CombatTarget` placed 90 cm short of him and facing him; with no target the warp
  target is removed and the clip's own root motion plays. Such a clip replaces the lunge; an
  in-place clip (no root motion) keeps the lunge.
- Dodge, finisher, hit reactions: the rules (300 cm in 0.4 s, 0.25 s invulnerable; 1.2 s
  finisher, lethal at 0.35 s) stay in C++. A dodge clip for the stick's direction relative to
  her facing plays fitted to DodgeSeconds, pose only (the dash moves her). The finisher plays
  FinisherAttacker (FinisherBow with the bow up) fitted to DurationSeconds and FinisherVictim on
  the thug at rate 1 until he is thrown. Kate's stagger and a thug's hit reaction play the Hit
  clip for the side the blow came from, on top of the lean.
- Knockdown with clips: a heavy strike or a bola trip plays Knockdown, which holds its last
  frame, instead of the ragdoll; after KnockdownSeconds GetUp plays and he is getting up for
  its length (with no GetUp clip the knockdown clip blends out over GetUpSeconds). The
  explosive's blast always ragdolls. A clip knockdown counts as knocked down for the finisher
  gate exactly as the ragdoll does.
- Bow with clips (upper body, over locomotion; rewritten 2026-09-28 after "the bow animation is not
  natural"): BowDraw plays at its own pace, rate 1, whatever the bow's draw time or DrawRate, and holds
  its last frame (the anchor); once it is at its end BowAimIdle takes over and loops until the release.
  The draw fraction (damage, speed, spread, the perfect window, the HUD) still counts on its own clock,
  FullDrawSeconds over DrawRate, so the pose never waits on the number or the number on the pose.
  Release plays BowFire; a new draw during it starts BowDraw at once. A let-down, cancel or a release
  below MinDrawFraction blends the clip out over 0.25 s. Bow clips blend in over 0.15 s and out over
  0.25 s on the UpperBody slot (spine_01 up); the legs stay on locomotion.
  Sparrow's clips as trimmed in the manifest: BowDraw is Primary_Fire_Med 0.43-0.93 s (the hand from the
  arrow rest back to the anchor, 0.50 s; the pull itself 0.33 s), BowAimIdle Sparrow's drawn idle (a
  10 s loop that starts on the anchor pose), BowFire Primary_Fire_Med 0.00-0.43 s (the snap, then the
  hand back to the string, which is where BowDraw starts). RMB_Drawback is not used: it is a 2.5 s
  charged draw, and fitted to 0.8 s it rushed and blended out before the aim clip came in.
- A clip holds the arms exactly as much as it is blended in: the IK's clip alpha is the UpperBody slot's
  weight (UBowComponent::GetBowClipWeight), so the bow hand's IK is off by that weight and the bow rides
  in the clip's bow hand. The bow stays in the hand while a bow clip is still blended in above 0.05,
  even after the follow-through; it goes on the back only once the arms are the locomotion's again.
  A shot on a fire clip follows through only until that clip starts to blend out (its length less its
  blend-out, 0.18 s for Sparrow's), never longer than FollowThroughSeconds, so the IK does not lift the
  arm back up while the clip lowers it. Without a fire clip the 0.6 s follow-through stands.
- The string hand under a clip stays on the clip. The IK only corrects it toward the arrow line: the
  line back from the bow through where the arrow will fly (the launch direction, from the bow hand
  socket to the aim point), at the clip's own draw length, so the correction never fights the pull. Its
  alpha is the draw alpha times the correction: 0 within ClipCorrectionDeadZone (3 cm) of the line,
  rising smoothly to ClipCorrectionMaxAlpha (0.5) at ClipCorrectionFullDistance (15 cm). The draw
  elbow's pole is the clip's own elbow, so the elbow never swings to a hint. The nock and the arrow
  follow the string hand as before, so the arrow is always on the string.
- Aim offset (2026-09-28): `BS_BowAimOffset_Sparrow` per skeleton, an AimOffset blend space built by
  the import script from Sparrow's AO_idle layout and its nine idle_AO poses retargeted (additive in
  mesh space on the centre pose). It plays on the UpperBody branch after the slot, at alpha clip alpha
  times bow alpha, so only while a bow clip holds the arms (the IK-only bow never gets it). Its inputs:
  yaw is the aim's yaw off the actor's, pitch the aim's pitch (the control rotation for the player, the
  aim override for the AI), each clamped to the blend space's own axis range and to
  MaxAimOffsetYaw/Pitch (90). Under it the spine's turn toward the aim is not added (the aim offset
  turns the body); with no aim offset asset the old turn stays.
- Aim offset pitch (2026-09-29): the pitch input is the aim's pitch times `AimOffsetPitchScale` (1.57)
  above level or `AimOffsetPitchScaleDown` (1.85) below, plus `AimOffsetPitchBias` (5), then clamped
  as above. Sparrow's up and down poses aim far less than their +-90 on the axis, not alike, its centre
  pose holds the arrow 4 degrees low, and the string hand's correction takes back part of any change.
  Measured by `Hawkeye.Screenshot.BowDraw` (nocked arrow pitch, standalone): before, 23.9 at 30 up,
  -25.2 at 30 down, -4.0 level; after, 30.6, -30.2 and -1.5. Looking down, the launch line from the
  bow hand to the aim point is shallower than the view (-19.3 at 30 down), so there the arrow points
  with the view rather than along its flight; the arrow still flies to the reticle. Clint shares
  Kate's skeleton and settings; the archers take the same scales on their own aim offset.

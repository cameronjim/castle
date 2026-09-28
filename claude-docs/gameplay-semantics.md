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
  marker with a Q hint for the first five uses. Fire spawns a straight, gravity-free
  arrow at 6000 cm/s; on arrival the character zips in Flying mode along a straight line
  to the anchor's landing point at 1800 cm/s, input locked except the camera, and lands
  in Walking with no fall damage. Chaining is allowed once past 70% of the line, when
  the marker returns. A zip is cancelled by static geometry on the path except within
  150 cm of the anchor (corner anchors share walls with neighbours). Costs one grapple
  arrow; the arrow stays in the anchor and is recovered within 200 cm of it.
- Anchors sit on the parapet centre (15 cm in from the roof edge) at every roof corner
  and at mid-edge on edges over 25 m, on buildings over 8 m, none within 4 m of another.
  Landing points are 60 cm further inboard so the capsule clears the parapet. The
  generator places them; verify asserts every landing point is above a building roof.
  Anchor lookup uses a 25 m spatial grid.

## Bow and arrows (built 2026-09-26; the rules below hold, with these notes)
- Data: `UBowDefinition` (DA_Bow_Kate 0.8 s draw, DA_Bow_Clint 1.0 s) and
  `UArrowDefinition` (DA_Arrow_Standard 40 damage, cap 30, recoverable; DA_Arrow_Grapple
  cap 6, recoverable, hit effect Grapple). `UBowComponent` on the player; `AArrowProjectile`
  with gravity, sticks into what it hits for 30 s, recovered within 150 cm.
- The quiver replaced the weapon slots: `Bow` (null means fists on left click), six
  arrow slots with standard always in slot 1, `ActiveArrowSlot`, keycards. Keys 1 to 6
  and the wheel skip empty slots. The mission grants `StartingBow` and `StartingArrows`;
  CH01 gives Kate's bow, 30 standard, 6 grapple. `Clear()` returns to the grant.
- Q fires a grapple arrow at the marked anchor from the quiver without changing the
  nocked slot. Damage scales with draw from 40% to 100% of the arrow's damage.
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
  holstered. Upper-body bow clips now layer over locomotion (see "Combat animation clips"); an
  aim offset is still stage 3 work.

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
  back at 30%. Once per fight; a fight ends 10 s after the last contact.
- The partner can't drop below 1 health, self-heals under 50%, can be staggered. T marks
  a point for him to go to (no pad button yet).
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
| L3 (press left stick) | Sprint hold |
| Right trigger | Draw and release |
| Left trigger | Aim |
| RB | Grapple |
| D-pad up / down | Standard arrows / grapple arrows; left / right cycle slots |
| View | Quiver and inventory |
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
- Feel: 2-frame hit stop (time 0.1 for 0.033 s) on melee hits, camera shake when Kate is
  hit, a 0.1 s flash on a hit thug, and desaturation plus vignette under 40% health.
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
- Heavy (built 2026-09-27): `BP_Thug_Heavy`, 200 HP, a 60 x 110 cm riot shield on the left forearm blocking arrows and light strikes in the front 120 degrees (arrows stick in it); staggered only by a heavy strike, bola, or explosive; putty holds him; shield bash with a 0.8 s telegraph, 30 damage, 250 cm knockback, plus a slow bat swing; walks at 300. One on his own 20 m patrol by the park corner, tagged StreetGroup.
- Archer holds (built 2026-09-27): on losing line of sight mid-draw he holds up to 2.5 s and fires within 0.2 s of the player reappearing in his cone; a 0.6 s loose window after a shot leaves him open. Scripted duel: won with 1 hit taken.
- Readability: a cream "!" for 0.6 s over a thug going Alerted, "?" for Suspicious; a thin health bar over damaged thugs within 1500 cm, fading after 3 s. Scripted street fight against bat, gunner, heavy: won, 0 hits, 0 untelegraphed hits, fairness metric 0% staggered time.

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
  after a load), thugs (position, dead flag; a dead thug goes down again on load without
  dropping loot twice), campaign state (version, mission path, completed objectives,
  safehouses found, play time, which character was controlled). Not saved: stuck arrows,
  pickups, EMP lamp state, spawner-created actors, the partner's fight state.
- Autosave on objective completion, safehouse entry, new game, and after 60 s of roaming
  (the clock runs only while the player is alive, on the ground, and no thug is alerted; a
  fight pauses it). Death with no revive, or Restart, fades to black and loads the last
  save; with no save it reloads the level.
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
- 39 MetaSounds (33 one-shots, 6 loops) under `/Game/Audio`, authored headless by
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
  nearest the park carry a 120 Hz buzz that the EMP silences with the light.
- Sound classes `SCL_Master`, `SCL_SFX`, `SCL_Ambient`, `SCL_UI` and the mix `SMX_Settings`
  drive Master, Sound effects, and Ambience sliders in Settings (settings version 4).
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
- Open: the explosion still reads washed out (its light plus camera-shake blur); the hit
  spark competes with the thug's hit flash; the footstep kick is faint.

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
  within 0.35 s of the previous hit landing; a heavy anywhere in the chain does 35 and
  knocks down. HUD counter from x2, clears after 2 s; at x5 it turns purple and adds 20%.
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
- Soft lock: strikes and dodges turn toward the nearest thug within 400 cm in front over
  0.1 s. A dodge during a telegraph within 300 cm gives 0.1 s of slow motion.

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

## Difficulty and accessibility (built 2026-09-27)
- One difficulty setting, Story / Normal / Hard (default Normal), asked once at New Game
  and changeable in Settings; `UDifficultySubsystem::GetScalar` holds the whole table.
  Thug melee and gunner damage 0.6 / 1.0 / 1.4; thug health 0.8 / 1.0 / 1.2; archer draw
  1.5 / 1.2 / 1.0 s; parry window +0.15 / 0 / -0.1 s (Story holds an early tap up to
  0.15 s; Hard ignores the first 0.1 s of a telegraph); regen delay 3 / 5 / 8 s at
  10 HP/s (regen didn't exist before and applies to Kate only); fall damage 0.5 / 1.0 /
  1.0; Story adds 2 to each trick arrow cap. `-Difficulty=Story|Normal|Hard` on the
  command line overrides without saving. Every scripted fight is won on all three.
- Accessibility, all persisted (settings version 5): subtitle size 20 / 26 / 34 px with a
  background opacity slider; hold or toggle for aim (default hold) and crouch (default
  toggle); a 1 px outline on the alert glyphs; four colour palettes (default,
  deuteranopia, protanopia, tritanopia) for the reticle, markers, hotbar, glyphs, and
  health bars; reduce camera shake to 30%; reduce flashing to 30% for the EMP split and
  the explosion light (the parry ring shrinks to 30% instead, it has no brightness
  input); Replay flashbacks in the pause menu, slides only, seen-list stored with settings.
- HUD scale 0.8 to 1.4 applies to HUD text, hotbar, markers, and compass.
- Challenge pedestal light 135 lm over 400 cm with a stronger emissive; the cap and icon
  still read white rather than purple.

## Street crimes (built 2026-09-27; names are placeholders; rules also in Source/Hawkeye/Crime/CrimeRules.h)
- 12 `City_CrimeSpot_` actors: 8 street corners (each with a robbery escape point 58 to
  62 m along the sidewalk) and 4 rooftops with anchors; none within 40 m of a safehouse
  or pedestal, all at least 35 m apart.
- A crime is due every 90 to 150 s of roaming (not in combat, a challenge, or a chapter
  beat), at a spot 25 to 40 m away and 60 m from the last, preferring spots out of sight.
  One at a time. `hawkeye.CrimeInterval` and `hawkeye.CrimeType` cvars control it;
  automation never gets a crime unless the interval is set.
- Mugging: fists and bat thugs around a grey `BP_Civilian`; the victim is hit at 15, 30,
  and 45 s while an unalerted thug is beside him, and the third hit fails it. Robbery:
  a runner (380 cm/s) who leaves when Kate is within 20 m, a thug is alerted, or 40 s
  pass, and drops the loot when downed; picking up the loot ends it. Ambush: four thugs
  alerted from the start. Rooftop: two thugs and an archer.
- Stopping a crime gives +5 standard arrows, a toast, and a per-type count in the save.
  Crime thugs despawn 60 s after success once the player is 40 m away, 20 s after failure.
- Scripted mugging: rescued in 12.3 s with no hits. No alley spots yet; robbery, ambush,
  and rooftop are covered by headless world tests only.

## Safehouses and fast travel (built 2026-09-27; rules also in Source/Hawkeye/World/SafehouseSubsystem.h)
- Two safehouses: "[Safehouse 1]" at 140 East 7th Street (OSM W248142338) and
  "[Safehouse 2]" on Avenue B at East 10th (OSM W250264779), 278 m apart. The generator
  picks the second as the nearest qualifying storefront at least 250 m from the first,
  skipping buildings whose fire escape would cut through the sign. Both must be entered
  once to be discovered; undiscovered ones show greyed as "[Undiscovered]".
- Fast travel from a safehouse menu to any other discovered one: fade out 0.5 s, hold
  0.35 s during which Kate and Clint are moved to the destination door and the game
  autosaves with them there (destination recorded as last used), fade in 0.6 s; about
  1.5 s fade to fade, no map reload. Refused with a "[Can't fast travel now]" toast during
  a crime or challenge.
- Discovered safehouses show as house icons on the compass; the pause menu can mark the
  nearest one. Chapter select is still a stub. Crime spots keep 40 m clear of both.

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
- A role with several clips takes the first in `Tools/Data/Anims/manifest.json` order that exists for
  that character; `create_combat_anims.py` writes it into the set.
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
- Chain timing with a clip: after a light lands, the chain stays open (the 0.35 s timer does
  not run) until the montage's `ANS_ComboWindow` ends; while that window is open the next
  strike starts at once, cutting the current montage. A press between the hit and the window
  is held and goes when the window opens. A montage with no `ANS_ComboWindow` keeps the
  0.35 s timer and ends the swing at RecoverSeconds.
- Kate's strike timing with clips (2026-09-28; each clip's start, end, rate and windows are in the
  manifest, in the source clip's seconds): Light1, Light2 and Light3 land 0.24 to 0.26 s after the
  input, Kick (the Kicking front kick) 0.29 s, Heavy 0.41 s. A light's montage lasts 0.57 to 0.81 s; its combo window opens
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
  `Hawkeye.Smoke.ThugClipsStrikeOnTheTelegraph`): fists (Standing Melee Punch, 0.25 s of wind-up)
  hold 0.29 s then play at 0.8x and land at 0.60 s; bat (Standing Melee Attack Horizontal, 0.87 s of
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
- Bow with clips (upper body, over locomotion): BowDraw fitted to the draw time
  (FullDrawSeconds over DrawRate), BowAimIdle looping from full draw until release, BowFire on
  release, BowNock after it while not drawing again; a let-down or cancel blends them out over
  0.2 s. While a bow clip plays the clip holds the bow arm (the bow rides in that hand), the
  string hand IK is anchored to where the bow hand actually is, and the spine keeps only the
  turn toward the aim (the clip supplies the side-on stance).

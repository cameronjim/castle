# Gameplay semantics

What each system promises. Tests in `Source/Castle/Tests` assert these. If you change a
rule here, change the test and the code in the same commit.

**Pivot note (2026-09-25).** The game is now third person with bows. Sections marked
LEGACY describe first-person or pistol behaviour that still exists in code until the
stage 2 cleanup removes it; don't extend them. Sections marked PLANNED are the contract
for code not yet written; write the tests from them. Everything unmarked stands.

## Third-person camera and look (built 2026-09-26)
- Spring arm behind the character: hip length 350, socket offset (0, 70, 60), FOV 90.
  Aim (right mouse) blends over 0.15 s to length 180, offset (0, 45, 55), FOV 70. The
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
- `castle.DebugMovement 1` draws state, speed, and fall height on the HUD. Landing
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
- Landing above 400 cm triggers the placeholder dip (roll and stumble not built).
  Grapple landings never trigger it. Fall damage begins at 900 cm and never kills from a
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
- Known gap: there is no draw animation. The bow blends from the left palm socket to a
  point in front of the left shoulder while drawing and rides across the back when
  holstered. A layered upper-body aim animation is stage 3 work.

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

## PLANNED: partner and switching
- The AI Hawkeye follows at 400 to 800 cm, takes cover when shot at, attacks enemies
  the player has hit within the last 3 s, and goes to a marked point on command. Never
  blocks doors. Revives the player once per fight.
- `SwitchCharacter()` swaps possession, hands the previous pawn to a partner AI
  controller, swaps HUD context, and blends the camera over 0.3 s. Only allowed where the
  chapter data asset permits, and never mid-traversal-move or mid-takedown.

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
- Look input is multiplied by the sensitivity from `UCastleSettingsSubsystem` (default
  0.2, clamped to [0.02, 1.0]) and, while aiming, by `AimLookMultiplier` (0.7) as well.
  The character's own `LookSensitivity` property is only the fallback when no game
  instance subsystem exists (tests). Changing the setting takes effect immediately.

## Settings
- `UCastleSettingsSubsystem` (game instance) owns `FCastleSettings` and persists it in the
  save slot `CastleSettings` on every change. Missing or version-mismatched data yields
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
  40 cm lunge) that knocks the thug down for 3 s. Takedown from behind on F is unchanged.
  No attack animation exists yet; strikes are procedural lunges.
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
  reconstructed from `Castle.log`.
- Guard death is always visible. `GoLimp(Killer)` ragdolls the mesh (the mannequin's
  physics asset from the High feature pack; the Standard pack's copy is an empty stub and
  must never be used). If the mesh reports it is not simulating afterwards, a procedural
  collapse runs instead: 0.6 s tip of 85 degrees away from the killer, 20 cm drop,
  animation stopped. Either way the flashlight turns off and the capsule stops colliding.
  Which path ran is logged once per guard at Warning.

## Save data (stage 3)
- One slot, `CastleSave`, autosaved at mission complete and at checkpoints. Manual save
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

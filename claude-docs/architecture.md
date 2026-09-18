# Architecture

One game module, `Castle`, four subsystems that talk through delegates. Nothing holds a
hard pointer across subsystem boundaries; everything that crosses one is either a data
asset reference or an event.

## The systems

```
Player input
    │
    ▼
ACastleCharacter ──► UWeaponComponent ──► ApplyPointDamage ──► UHealthComponent (on target)
    │                                                               │
    ├──► UTakedownComponent ──► ITakedownable::OnTakedown           ├──► OnHealthChanged ──► UBossPhaseComponent ──► OnPhaseChanged
    │                                                               │                                                  │
    │                                                               └──► OnDeath                                       └──► BP arena controller, behavior tree
    │
    ▼
World: AObjectiveTriggerVolume, doors, pickups
    │
    ▼
UMissionSubsystem (world subsystem)
    │  StartMission(UMissionDefinition*)
    │  CompleteObjective(FName)
    ├──► OnObjectiveUpdated ──► WBP_Hud
    ├──► OnMissionComplete ──► end card, save
    └──► OnFlashbackRequested(UFlashbackDefinition*)
              │
              ▼
       ACastlePlayerController ──► creates UFlashbackWidget ──► Play()
                                          │
                                          └──► OnFlashbackFinished ──► open NextLevel
```

### Mission (`Source/Castle/Mission`)
- `UMissionDefinition` (primary data asset): the mission's identity, its ordered
  objectives (instanced `UMissionObjective` objects), the flashback to play after, the
  next level. One asset per mission in `Content/Missions`.
- `UMissionObjective` (instanced UObject): title, description, optional flag, completed
  flag, an `ObjectiveId` FName used by the world to complete it. Blueprintable so a mission
  can subclass it for special objectives, but most are the base class with data.
- `UMissionSubsystem` (world subsystem): the runtime. Duplicates the definition's
  objectives on `StartMission` so the asset never gets dirtied. Owns "which objective is
  current" and "is the mission done". Fires the delegates above. Lives per world, so a
  level transition resets it; the save system (stage 3) restores state on load.
- `AObjectiveTriggerVolume`: the common way levels complete objectives. Overlap by the
  player pawn, calls `CompleteObjective(ObjectiveId)`. Doors, pickups, and scripted beats
  do the same call from Blueprint.

### Flashback (`Source/Castle/Flashback`)
- `UFlashbackDefinition` (primary data asset): ordered `FFlashbackSlide` array (texture,
  caption, hold seconds, crossfade seconds, voice line), ambient loop, skippable flag.
  Stage 3 adds `ReplacedByImage` and `GlitchSeconds` per slide for the lie-breaking effect.
- `UFlashbackWidget` (UUserWidget): two stacked `UImage`s crossfaded by opacity, a caption
  block, timing driven from `NativeTick` because the game is paused during playback and
  paused worlds don't run timers. Audio is created as UI sound so it survives the pause.
  Fires `OnFlashbackFinished`. Designers subclass it as `WBP_Flashback` to lay out the
  visuals; the logic stays in C++.

### Combat (`Source/Castle/Combat`)
- `UHealthComponent`: `MaxHealth`, `CurrentHealth`, `bInvulnerable`. Hooks the owner's
  `OnTakeAnyDamage`. Fires `OnHealthChanged(Old, New, Instigator)` and `OnDeath`. Every
  damageable thing has one: player, guards, bosses, shootable lights.
- `UBossPhaseComponent`: requires a `UHealthComponent` on the same actor. An ordered
  `FBossPhase` array with health-percent thresholds. Listens to `OnHealthChanged`, advances
  the phase index when health drops below the next threshold, optionally sets
  `bInvulnerable` for `TransitionSeconds`, fires `OnPhaseChanged(Old, New, Phase)`. It does
  not know what a phase means; the boss Blueprint and behavior tree react.
- `UWeaponComponent`: hitscan weapon. Magazine, reserve, damage, range, fire rate, reload
  time. `Fire()` line-traces from the owning pawn's view and applies point damage.
  `Reload()` is a timer. Fires `OnAmmoChanged`. Stage 3 moves stats to a `UWeaponDefinition`
  data asset so a shotgun is data, not a subclass.
- `UTakedownComponent`: on the player. `TryTakedown()` sphere-traces for actors tagged
  `Guard`, checks the player is behind the target within `MaxAngleDegrees`, calls
  `ITakedownable::OnTakedown` on it. Fires `OnTakedownPerformed`.
- `ITakedownable`: one BlueprintNativeEvent, `OnTakedown(AActor* Attacker)`. Guards
  implement it in Blueprint (ragdoll, drop pickup, notify AI).

### World (`Source/Castle/World`)
- `IInteractable`: `Interact(Interactor)`, `GetInteractPrompt()`, `CanInteract(Interactor)`.
  Anything the player presses E on implements it.
- `UInteractionComponent`: on the player. Sphere-sweeps from the camera every 0.1 s out
  to `InteractRange` (250), tracks the focused interactable, pushes its prompt to the HUD,
  `TryInteract()` is bound to IA_Interact. Falls back to a takedown prompt when the
  takedown component has a valid target.
- `APickupActor`: `EPickupType {Weapon, Keycard, Ammo}`. Weapon calls `GiveWeapon()` on
  the player's weapon component and sets ammo; Keycard adds `KeycardId` to the player's
  keycard set; Ammo adds reserve. Optional `CompletesObjectiveId`. Destroys itself.
- `ADoorActor`: frame and door meshes, `bLocked`, `RequiredKeycardId`, lerps open over
  `OpenSeconds` in Tick, completes `CompletesObjectiveId` the first time it opens.
- `AGuardCharacter`: 100 HP, tag `Guard`, its own `UWeaponComponent` at 12 damage,
  `EGuardAlertState {Calm, Suspicious, Alerted}` with `OnAlertStateChanged`. Implements
  `ITakedownable`: `CanBeTakenDown` is false when Alerted. On takedown or death, drops
  the `DropOnDeath` pickups once and goes limp.
- `AGuardAIController`: perception (sight 1500 / lose 1800 / half angle 35, hearing
  1200) feeding a C++ state machine on a 0.25 s think timer. Calm patrols
  `PatrolPoints`, Suspicious investigates the stimulus, Alerted faces and shoots with a
  `AimSpreadDegrees` cone and loses the target after `LoseTargetSeconds`.
  `ReportStimulus(Kind, Location, bSuccessful)` is the single entry point both perception
  and tests use. No behavior tree yet; stage 3 decides whether one is needed.

### UI (`Source/Castle/UI`)
- `UCastleHudWidget`: objective title, ammo (`12 / 24`, blank when unarmed), interaction
  prompt, crosshair. Builds its own layout in `RebuildWidget` when the Blueprint has none.
  Subscribes to the mission subsystem, the pawn's weapon, and the takedown component.
  Hidden during flashbacks by the controller. Honours `UMissionDefinition::bShowObjectiveText`.

### Player and framework
- `ACastleCharacter`: first-person camera on the capsule, health, takedown, interaction,
  weapon (starts with `bHasWeapon` false), noise emitter (1.0 sprinting, 0.4 walking, 0
  crouched, every 0.5 s; 3.0 on fire), keycard set. Enhanced Input action properties
  assigned in `BP_CastleCharacter`. Movement tuning lives on the Blueprint's
  CharacterMovement component, not in C++.
- `ACastlePlayerController`: the glue for UI. Creates `HudWidgetClass` on BeginPlay,
  subscribes to the mission subsystem, owns `FlashbackWidgetClass`, hides the HUD during a
  flashback, opens the next level when it finishes. Stage 3 adds end cards, subtitles,
  and the pause menu here.
- `ACastleGameMode`: default pawn and controller classes, `StartingMission` which it
  starts at BeginPlay, `RestartMission()` which reopens the level 2 s after player death.
  Stage 3 replaces the restart with checkpoints.

## Boundaries
- C++ never references a specific mission, boss, or level by name. If you find
  `"M01"` in a .cpp file, that's a bug.
- Blueprints never implement rules that a test should cover. Health math, objective
  ordering, phase thresholds, ammo counts: C++.
- Widgets: layout in `WBP_*`, behaviour in the C++ parent. A widget Blueprint with more
  than a screenful of graph nodes is a sign the parent needs a function.
- AI: behavior trees and blackboards are content (`Content/Blueprints/AI`). C++ provides
  perception setup, a base `AGuardCharacter` (stage 2), and BT tasks/services only where
  a Blueprint task is measurably too slow (the yard riot with 30 guards is the likely case).

## Lifecycle of a mission, end to end
1. Level `L_M0X` loads. Its Level Blueprint (or a `AMissionStarter` actor, stage 2) calls
   `MissionSubsystem->StartMission(DA_M0X)`.
2. Subsystem duplicates objectives, marks the first as current, fires `OnObjectiveUpdated`.
3. Player plays. World actors call `CompleteObjective` by id. Order is not enforced by
   default; `UMissionDefinition::bEnforceOrder` (stage 2) can require it.
4. When every non-optional objective is complete: `OnMissionComplete`. Controller shows
   the end card. If `FlashbackToPlay` is set: `OnFlashbackRequested`.
5. Controller creates `WBP_Flashback`, pauses, plays. `OnFlashbackFinished` unpauses
   and, if `NextLevel` is set, calls `OpenLevel`.
6. Save (stage 3) writes progress at step 4 before the flashback so a quit during the
   slideshow doesn't lose the mission.

## Things deliberately not built
No inventory system (one weapon slot, ammo counters). No dialogue tree (linear lines from
a data table). No branching levels (two boolean choices that change a line and a decal).
No multiplayer, replication, or networking of any kind; don't add `Replicated` specifiers.

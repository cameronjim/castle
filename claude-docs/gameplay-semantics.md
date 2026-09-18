# Gameplay semantics

What each system promises. Tests in `Source/Castle/Tests` assert these. If you change a
rule here, change the test and the code in the same commit.

## Mission
- A mission has 1 or more objectives. At least one must be non-optional.
- `StartMission` on a subsystem that already has an active mission ends the old one
  without firing `OnMissionComplete`, then starts the new one. Logs a warning.
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
- Each slide displays for `HoldSeconds`, then crossfades for `CrossfadeSeconds` into the
  next. The last slide holds, then fades to black over its `CrossfadeSeconds`, then finishes.
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
- `OnHealthChanged(Old, New, Instigator)` fires on every change, including heals.
- `OnDeath` fires exactly once when health first reaches 0. Further damage after death
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
  start (threshold 100). A boss with N phases has N-1 transitions.
- A transition happens when health percent drops strictly below the next phase's
  threshold. Overkill that crosses two thresholds in one hit advances to the lowest
  matching phase and fires `OnPhaseChanged` once, with `NewPhase` being the final one.
- During a transition with `bInvulnerableDuringTransition`, the health component is
  invulnerable for `TransitionSeconds`, then restored to its previous invulnerability
  state (so a boss that was already invulnerable for scripted reasons stays that way).
- Death fires `OnDeath` from the health component; the phase component does nothing on
  death. The boss Blueprint handles the kill.
- `BehaviorTag` on each phase is an `FName` written to the boss's blackboard key
  `Phase` by the boss Blueprint on `OnPhaseChanged`. The behavior tree branches on it.
- Boss health bars show one segment per phase, sized proportionally to the health range
  each phase covers.

## Weapon
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

## Guard AI states (stage 2/3)
- `Calm`: patrol. Hearing radius `CalmHearingRadius`, sight cone `SightHalfAngle`.
- `Suspicious`: heard something or saw something briefly. Walks to the stimulus,
  waits `InvestigateSeconds`, returns to `Calm`. Takedown still valid.
- `Alerted`: has confirmed the player. Attacks, seeks cover, alerts squad after
  `SquadAlertDelay` (1.5 s). Returns to `Suspicious` after `LoseTargetSeconds` without
  perceiving the player, then to `Calm`.
- Noise: player sprinting emits a noise event every 0.5 s with loudness 1.0. Walking
  0.4. Crouching 0. Gunshots 3.0. Takedowns 0.6 (the body drop).

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

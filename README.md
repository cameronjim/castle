# Castle

Mission-based first-person shooter — a Punisher-origin prison escape. This repo is the C++ scaffold:
Blueprint-friendly base classes meant to be extended by designers in Blueprints. No content assets yet.

Design notes live in `docs/DESIGN.md`.

## Requirements

- Unreal Engine 5.4 or newer
- Visual Studio 2022 with the **Game development with C++** workload (include the "Unreal Engine installer" component)
- Git LFS (`git lfs install`) before adding any binary content

## Opening the project

1. Right-click `Castle.uproject` → **Generate Visual Studio project files**.
2. Open the generated `Castle.sln`.
3. Set the configuration to **Development Editor**, platform **Win64**, and build.
4. Launch from Visual Studio, or double-click `Castle.uproject` once the editor module is built.

First build also creates `Content/` — add a `Content/Missions` and `Content/Flashbacks` folder,
since the Asset Manager is configured to scan them for data assets.

## What each class does

**Mission**

- `UMissionDefinition` (`Source/Castle/Mission/`) — data asset for one mission: name, number, instanced
  objectives, an optional flashback and the level to open next. Create one per mission in the Content Browser.
- `UMissionObjective` — one objective (tag, title, description, optional flag). Subclass it in Blueprints
  to add custom completion logic; it is `EditInlineNew`, so objectives are authored inline on the mission asset.
- `UMissionSubsystem` — per-world tracker. Get it with **Get World Subsystem → MissionSubsystem**. Bind
  `OnObjectiveUpdated` in the HUD, `OnMissionComplete` for end-of-level flow. It duplicates objectives at
  runtime so the source asset is never dirtied.
- `AObjectiveTriggerVolume` — drop in a level, set `ObjectiveTag`, and the objective completes on player
  overlap. Override `OnObjectiveTriggered` in a Blueprint child for stingers and VFX.

**Flashback**

- `UFlashbackDefinition` (`Source/Castle/Flashback/`) — a slideshow: ordered `FFlashbackSlide`s (image,
  caption, hold time, crossfade time, voice line), an ambient loop and a skippable flag.
- `UFlashbackWidget` — reparent a UMG widget to this class. Name two images `SlideImageA` / `SlideImageB`
  (stacked in an Overlay) and a text block `CaptionText` to have them driven automatically. Pauses the game,
  shows the cursor, crossfades slides, plays voice lines, skips on any key, then restores everything.

**Combat** (`Source/Castle/Combat/`)

- `UHealthComponent` — max/current health, `ApplyDamage`, `Heal`, `bInvulnerable`, `OnHealthChanged` and
  `OnDeath`. Hooks the owner's `OnTakeAnyDamage`, so `ApplyDamage`/`ApplyPointDamage` just work.
- `UBossPhaseComponent` — add alongside a health component on a boss. Fill the `Phases` array (highest
  health threshold first) and react to `OnPhaseChanged` in the Blueprint or behaviour tree — e.g. write
  `BehaviorTag` to a blackboard key to swap the gun sub-tree for the melee sub-tree.
- `UWeaponComponent` — hitscan weapon: magazine, reserve, damage, range, fire rate, timed reload,
  `OnAmmoChanged`. Implement `OnWeaponFired` / `OnReloadStarted` / `OnReloadFinished` in a Blueprint child
  for muzzle flash, sound and animation.
- `UTakedownComponent` + `ITakedownable` — sphere-sweeps for an actor tagged `Guard`, checks the player is
  behind it, then calls `OnTakedown` on the target. Implement the `Takedownable` interface on a guard
  Blueprint to play the takedown animation; override `CanBeTakenDown` to veto (alerted, already dead).

**Framework**

- `ACastleCharacter` (`Source/Castle/Player/`) — first-person camera at eye height, health + takedown
  components, Enhanced Input actions (Move, Look, Jump, Sprint, Crouch, Fire, Reload, Takedown, Interact).
  Make `BP_CastleCharacter`, assign the Input Actions and a Mapping Context in the **Input** category, add a
  `WeaponComponent`, and implement `OnInteractPressed`.
- `ACastleGameMode` — starts `StartingMission` on begin play; `OnMissionCompleted` is a Blueprint hook.
- `ACastlePlayerController` — listens for `OnFlashbackRequested`, spawns `FlashbackWidgetClass`, plays it,
  and travels to the mission's `NextLevel` when it finishes.

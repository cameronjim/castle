# Testing

Three layers. Most effort goes into the first.

## 1. Automation tests (C++)

Where: `Source/Castle/Tests/*Test.cpp`. Compiled only in editor and development builds
(`#if WITH_DEV_AUTOMATION_TESTS`). Named `Castle.<System>.<Behaviour>`.

What gets tested: every rule in `gameplay-semantics.md`. Concretely:

| System | Tests |
|--------|-------|
| Health | clamp, invulnerable ignores damage, OnDeath fires once, heal after death is a no-op, Revive works, negative damage ignored |
| BossPhase | thresholds advance in order, overkill skips to correct phase with one broadcast, transition invulnerability set and restored, N phases produce N-1 transitions |
| Weapon | fire decrements magazine, fire at 0 ammo does nothing, fire rate blocks rapid fire, reload math with partial reserve, reload with full mag is no-op, OnAmmoChanged counts |
| Mission | completion when all non-optional done, optional never completes mission, duplicate CompleteObjective fires nothing, unknown id logs warning, OnMissionComplete once, OnFlashbackRequested fires only with a definition, current objective picks first incomplete non-optional |
| Flashback | null/empty definition finishes immediately, total duration equals sum of holds and fades, skip finishes whole thing, skip ignored in first 0.5 s, pause state restored |
| Takedown | angle check (behind passes, front fails, edge at MaxAngle), range check, alerted guard rejected, tag required |
| Save (stage 3) | round-trip every field, stale mission id starts new game, version mismatch handled |

Pattern for a component test (no world needed):

```cpp
#include "Misc/AutomationTest.h"
#include "Combat/HealthComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleHealthDeathFiresOnce,
	"Castle.Health.DeathFiresOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleHealthDeathFiresOnce::RunTest(const FString& Parameters)
{
	UHealthComponent* Health = NewObject<UHealthComponent>();
	Health->SetMaxHealth(100.f, /*bResetCurrent*/ true);

	int32 DeathCount = 0;
	Health->OnDeath.AddLambda([&DeathCount](AActor*) { ++DeathCount; });

	Health->ApplyDamage(150.f, nullptr);
	Health->ApplyDamage(10.f, nullptr);

	TestEqual(TEXT("Health clamped to zero"), Health->GetCurrentHealth(), 0.f);
	TestEqual(TEXT("OnDeath fired exactly once"), DeathCount, 1);
	return true;
}

#endif
```

Components that need an owner (Takedown, Weapon trace) get a test world:

```cpp
UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
FWorldContext& Ctx = GEngine->CreateNewWorldContext(EWorldType::Game);
Ctx.SetCurrentWorld(World);
// spawn actors, run the test
GEngine->DestroyWorldContext(World);
World->DestroyWorld(false);
```

Put that in a small RAII helper (`FCastleTestWorld`) in `Tests/CastleTestUtils.h` so
every test doesn't repeat it.

Dynamic multicast delegates can't take lambdas. Tests bind to a `UCastleTestListener`
UObject with `UFUNCTION()` counters (`OnHealthChangedCount`, `LastNewHealth`, ...), also
in `CastleTestUtils.h`.

Run all:

```powershell
& "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Users\camer\code\fps-game\Castle.uproject" -ExecCmds="Automation RunTests Castle; Quit" -unattended -nullrhi -nosplash -nop4 -stdout -FullStdOutLogOutput -ReportExportPath="C:\Users\camer\code\fps-game\Saved\Automation"
```

Run one group: replace `Castle` with `Castle.Health`. The report is JSON under
`Saved\Automation\index.json`; the console log shows `Test Completed. Result={Passed|Failed}`
per test. Exit code is 0 even on failure, so grep the log for `Result={Failed}` or read
the JSON. The helper `Tools\run-tests.ps1` does this and exits non-zero on failure.

Rules:
- A test per bullet in `gameplay-semantics.md`. When a rule changes, the test changes in
  the same commit.
- Tests never load Content assets. Build definitions in code with `NewObject`. Content
  can move or be renamed; tests shouldn't care.
- Tests must pass with `-nullrhi`. No rendering, no input, no audio playback assertions
  (assert that the call was made, not that sound came out).
- Under 100 ms each. The whole suite under 30 seconds including editor startup overhead.

## 2. Functional tests (in a map)

Where: `Content/Maps/Tests/L_Test_*.umap` with `AFunctionalTest` actors (Functional
Testing plugin). Run by the same automation command under the `Project.Functional Tests`
group.

Use for: things that need a real world tick and navigation. A guard patrols and reaches
its waypoints. A guard hears a sprinting player and goes Suspicious. A trigger volume
completes an objective when the pawn overlaps. A door opens with a keycard and not
without. A boss with a real behavior tree changes branches on phase change.

Keep to under 10 of these. They are slow (each loads a map) and brittle when levels
change. One per system that has world-dependent behaviour, not one per feature.

## 2b. Standalone game check (required for any visual change)

The screenshot tests run inside the editor process, where assets are already loaded and
material usage flags compile on demand. The standalone game (`-game`, what Play Castle
launches) is not that process. On 2026-09-19 the editor renders looked right while the
real game showed default-material arms and guards, an invisible pistol, and unlit rooms.

So every pass that touches materials, meshes, lighting, or the viewmodel must also:
1. Run the screenshot tests from a `-game` process:
   `UnrealEditor-Cmd.exe <proj> -game -windowed -ResX=1280 -ResY=720 -unattended -nosplash -log -ExecCmds="Automation RunTests Castle.Screenshot; Quit"`
2. Read `Saved/Logs/Castle.log` from that run and require zero `LogMaterial: Warning`
   lines and zero `LogCastle: Warning` lines other than the ragdoll path notice.
3. Load soft references synchronously in game code paths that need them immediately
   (viewmodel meshes, weapon definitions); never rely on an asset already being resident.

## 3. Playtest checklist (human)

Before any commit that touches player feel, AI, or a level, play through this in
`L_Sandbox` or the affected mission. Two minutes.

- [ ] Walk, sprint, crouch, jump. Nothing snaps or floats.
- [ ] Look sensitivity unchanged (or changed on purpose).
- [ ] Fire until empty, reload, fire again. Ammo HUD correct.
- [ ] Takedown a patrolling guard from behind. Prompt appears, guard drops, other guards
      don't react unless they should.
- [ ] Get spotted. Guard shoots, you take damage, screen effect shows.
- [ ] Die. Respawn at checkpoint (stage 3) or mission start.
- [ ] Complete the mission. End card, flashback, next level (or return to menu).
- [ ] Press Esc during gameplay and during a flashback. Correct menu, correct resume.
- [ ] Check `Saved/Logs/Castle.log` for new Warnings or Errors from `LogCastle`.

Per-mission and per-boss playtests are in `docs/plans/04-content.md`. Formal playtesting
with other people is `docs/plans/06-polish-ship.md`.

## What's not tested with code
Level layout, materials, lighting, animation feel, sound mix, slideshow imagery, story
pacing. These get the human checklist and the playtest rounds. Don't write brittle
screenshot tests.

## CI (later, optional)
No CI yet. If a remote and a second machine ever exist: a scheduled job that pulls,
builds `CastleEditor`, runs `Tools\run-tests.ps1`, and posts the result. The engine is
too big for a hosted runner, so it'd be a self-hosted box.

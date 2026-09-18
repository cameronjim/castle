# Castle (working title)

Mission-based first-person shooter in Unreal Engine 5.8. Punisher origin story: Frank
Castle escapes a black-site prison while chemically suppressed memories of his family
come back as slideshow flashbacks. Solo hobby project, Blueprint-friendly C++ base.

Read `docs/DESIGN.md` for what the game is and `docs/plans/00-overview.md` for the build
stages. Read `claude-docs/` before changing code. The short version of every rule is here.

## Repo map

| Path | What lives there |
|------|------------------|
| `Castle.uproject` | Project file. EngineAssociation must match the installed engine (5.8). |
| `Source/Castle/` | All C++. One module. Subfolders: `Mission`, `Flashback`, `Combat`, `Player`, plus GameMode and PlayerController at the root. |
| `Source/Castle/Tests/` | Automation tests (created in stage 2). |
| `Config/` | `DefaultEngine.ini`, `DefaultGame.ini`, `DefaultInput.ini`. Change deliberately, note why in the commit. |
| `Content/` | Unreal assets (binary, LFS-tracked). Folder layout in `claude-docs/asset-conventions.md`. |
| `docs/` | Design doc, stage plans, per-mission one-pagers, playtest notes. Human-facing. |
| `claude-docs/` | Engineering reference for anyone (human or agent) writing code here. |
| `Tools/` | Editor Python scripts and PowerShell helpers for headless asset creation and builds. |

## Toolchain

- Engine: `C:\Program Files\Epic Games\UE_5.8`
- Compiler: Visual Studio 2022 Community, NativeGame workload, MSVC 14.44
- Build tool wrapper: `Engine\Build\BatchFiles\Build.bat` (uses the engine's bundled .NET; the system `dotnet` is not needed)
- Machine: RTX 4070, i7-13700K, 16 GB RAM. RAM is the constraint. Never run two editor instances.

Exact commands are in `claude-docs/infrastructure.md`. The two you'll use constantly:

```powershell
# Build the editor target
& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" CastleEditor Win64 Development -project="C:\Users\camer\code\fps-game\Castle.uproject" -waitmutex
```

```powershell
# Run automation tests headless
& "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Users\camer\code\fps-game\Castle.uproject" -ExecCmds="Automation RunTests Castle; Quit" -unattended -nullrhi -nosplash -nop4 -stdout -FullStdOutLogOutput -ReportExportPath="C:\Users\camer\code\fps-game\Saved\Automation"
```

## Rules that matter

1. **C++ is the base, Blueprints are the content.** Systems, components, data asset
   types, and anything that needs to be tested go in C++. Per-mission behaviour, boss
   move sets, UI layout, and tuning values go in Blueprints and data assets. If a
   Blueprint is growing logic that a second mission would copy, move it to C++.
2. **Data over code.** A new mission, boss, weapon, or flashback should be new assets,
   not new classes. If it needs a new class, the framework is missing something; fix the
   framework.
3. **Every C++ change builds before it's committed.** Run the build command. If you
   changed a header, the editor must be closed or use Live Coding; a stale editor
   silently runs old code.
4. **Tests for anything with rules.** Mission completion logic, health and damage,
   boss phase thresholds, weapon ammo math, save round-trip. See `claude-docs/testing.md`.
   Pure visual or feel work (animation, materials, level layout) is not tested with code.
5. **Don't rename the project or the module.** Don't change the `ProjectID` GUID by hand.
   Don't reformat files you aren't otherwise changing.
6. **Binary assets go through Git LFS.** `.gitattributes` handles it. If `git status`
   shows a `.uasset` as a normal file, stop and fix tracking before committing.
7. **One commit per coherent change**, message says what and why. End every commit
   message with the co-author line the session provides.
8. **The editor is heavy.** Prefer headless commands (`UnrealEditor-Cmd.exe` with
   `-nullrhi`) for builds, tests, and Python asset scripts. Only launch the full editor
   when something visual has to be checked, and close it after.

## Naming, in one glance

C++: `ACastleCharacter`, `UHealthComponent`, `FFlashbackSlide`, `EAlertLevel`,
`ITakedownable`, `bIsInvulnerable`, `OnHealthChanged`. Log category `LogCastle`.
Assets: `BP_Guard`, `DA_M01_CellBlockD`, `DA_FB01_Sunday`, `IA_Fire`, `IMC_Default`,
`WBP_Hud`, `L_M01_CellBlockD`, `M_Concrete`, `MI_Concrete_Wet`, `SM_Wall400`, `T_Grime_01`,
`S_Pistol_Fire`, `BT_Guard`, `BB_Guard`. Full table in `claude-docs/asset-conventions.md`.

## Where to look

- How the systems fit together: `claude-docs/architecture.md`
- What each system promises (mission lifecycle, damage rules, boss phases, save contract): `claude-docs/gameplay-semantics.md`
- Code style: `claude-docs/code-style.md`
- Asset naming and Content layout: `claude-docs/asset-conventions.md`
- Build, headless editor, Python scripting, git: `claude-docs/infrastructure.md`
- Tests: `claude-docs/testing.md`
- How to work in this repo as an agent: `claude-docs/workflow.md`

# Infrastructure

Paths, commands, and the machine. Everything here is Windows.

Renamed from Castle to Hawkeye on 2026-09-26; CoreRedirects in DefaultEngine.ini map the old script package and classes.

Prison content removed 2026-09-26; the district is the only map.

## Paths

| Thing | Path |
|-------|------|
| Project | `C:\Users\camer\code\hawkeye` |
| Engine | `C:\Program Files\Epic Games\UE_5.8` |
| Editor | `...\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe` |
| Headless editor | `...\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe` |
| Build script | `...\UE_5.8\Engine\Build\BatchFiles\Build.bat` |
| UAT (packaging) | `...\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat` |
| Visual Studio | `C:\Program Files\Microsoft Visual Studio\2022\Community` |
| Logs | `C:\Users\camer\code\hawkeye\Saved\Logs\Hawkeye.log` |
| Crash dumps | `C:\Users\camer\code\hawkeye\Saved\Crashes\` |

Set these as variables at the top of any script:

```powershell
$UE = "C:\Program Files\Epic Games\UE_5.8"
$Proj = "C:\Users\camer\code\hawkeye\Hawkeye.uproject"
```

## Build

Generate project files (after adding or removing source files, or changing Build.cs):

```powershell
& "$UE\Engine\Build\BatchFiles\Build.bat" -projectfiles -project="$Proj" -game -rocket -progress
```

Build the editor target (what you run 95% of the time):

```powershell
& "$UE\Engine\Build\BatchFiles\Build.bat" HawkeyeEditor Win64 Development -project="$Proj" -waitmutex
```

Build the standalone game target (for packaging tests):

```powershell
& "$UE\Engine\Build\BatchFiles\Build.bat" Hawkeye Win64 Development -project="$Proj" -waitmutex
```

Exit code 0 is success. Errors look like `error C2065` (compiler) or `Error: ... UnrealHeaderTool`
(reflection). UHT errors come first and block compilation; fix them before reading further.

`Source/Hawkeye.Target.cs` and `HawkeyeEditor.Target.cs` must stay on
`BuildSettingsVersion.V7` and `EngineIncludeOrderVersion.Unreal5_8`. Older values make UBT
refuse to build against the launcher engine (it reports `OtherCompilationError` in under a
second with no compiler output). 5.8 also writes `Hawkeye.slnx` next to `Hawkeye.sln`; both are
ignored by git.

Build time: about 80 seconds for a full rebuild of the Hawkeye module, 20-40 seconds incremental.
If the editor is open, close it or use Live Coding (Ctrl+Alt+F11 in the editor) instead.
Building with the editor open and Live Coding off produces a DLL the editor won't reload.

## Config facts worth knowing
- `DefaultEngine.ini` declares the `Weapon` trace channel (`ECC_GameTraceChannel1`).
  Don't reorder or renumber custom channels; assets store the enum value.
- `RuntimeGeneration=Dynamic` on `RecastNavMesh` is what lets headlessly generated maps
  have a navmesh at all. Removing it silently freezes every guard.
- The Python plugin is enabled for headless asset scripts. Its stub, when present, is at
  `Intermediate/PythonStub/unreal.py` and is the fastest way to check property names.

## Running the editor

Full editor (visual work only):

```powershell
& "$UE\Engine\Binaries\Win64\UnrealEditor.exe" "$Proj"
```

Play a specific map in a standalone window without the editor UI (fast iteration on feel):

```powershell
& "$UE\Engine\Binaries\Win64\UnrealEditor.exe" "$Proj" /Game/Maps/L_District_EastVillage -game -windowed -ResX=1600 -ResY=900 -log
```

Day, to see the block: add `-TimeOfDay=Day` to that line or to any automation run (not saved), or
type `hawkeye.TimeOfDay Day` in the console (`Night` to go back, `Saved` for the player's own
setting; also unsaved). `Hawkeye.Screenshot.TimeOfDay` writes `night_street.png`,
`day_street.png`, `day_park.png` (from the park at the tenement row) and `day_pedestal.png`
(challenge_pedestal's framing) to `Saved/Screenshots/Kate/` without the rest of the Kate pass,
in about two minutes; tune the day against those, at the same cameras. Two cautions reading
them: the pedestal's floating target icon spins, so it can show its plain cream back, and a
flake near the lens is a big soft white disc.

## Headless editor

Use `UnrealEditor-Cmd.exe` with `-nullrhi` for anything that doesn't need a screen.
Common flags: `-unattended` (no dialogs), `-nosplash`, `-nop4`, `-stdout`,
`-FullStdOutLogOutput` (log to console), `-NoLogTimes` (cleaner diff-able logs).

Run automation tests:

```powershell
& "$UE\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "$Proj" -ExecCmds="Automation RunTests Hawkeye; Quit" -unattended -nullrhi -nosplash -nop4 -stdout -FullStdOutLogOutput -ReportExportPath="C:\Users\camer\code\hawkeye\Saved\Automation"
```

Run a Python script in the editor (asset creation, batch edits):

```powershell
& "$UE\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "$Proj" -run=pythonscript -script="C:\Users\camer\code\hawkeye\Tools\Editor\create_input_assets.py" -unattended -nullrhi -nosplash -nop4 -stdout -FullStdOutLogOutput
```

Requires `PythonScriptPlugin` enabled in the .uproject. Scripts live in `Tools/Editor/`.
Python has `import unreal`. Useful entry points: `unreal.AssetToolsHelpers.get_asset_tools()`
for creating assets, `unreal.EditorAssetLibrary` for save/load/rename,
`unreal.EditorLevelLibrary` / `unreal.LevelEditorSubsystem` for maps,
`unreal.BlueprintEditorLibrary` for Blueprint parents and reparenting. Some things
(Blueprint graph nodes, widget trees) are not scriptable; do those by hand in the editor
or in C++ instead.

Cook content (checks for broken references and packaging problems without a full package):

```powershell
& "$UE\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "$Proj" -run=Cook -TargetPlatform=Windows -unattended -nullrhi -stdout
```

## Game Animation Sample (not in git)

Kate's locomotion comes from Epic's Game Animation Sample, installed at
`C:\Users\camer\code\GASP`. `Tools/Editor/import_gasp.py` copies about 2,030 packages
(2 GB, mostly `Content/Characters/UEFN_Mannequin`) into `Content/` at the same paths.
Those paths are in `.gitignore`: the repo is public and Epic's sample content shouldn't be
republished, and 2 GB is past the free LFS quota. Consequences:

- `BP_Kate` (committed) derives from `SandboxCharacter_CMC` (ignored). **A fresh clone must
  have the sample installed at that path and run `Tools\create-content.ps1` once before
  `BP_Kate` will load.** `verify_gasp.py` checks the copy.
- Plugins enabled for it: PoseSearch, Chooser, AnimationWarping, MotionWarping,
  CurveExpression, DrawDebugLibrary, Mover (the AnimBP uses it), MovieSceneAnimMixer,
  GameplayCameras, SmartObjects, StateTree, GameplayStateTree, GameplayInteractions,
  FullBodyIK. The sample's console variables and gameplay tags are copied into `Config/`.
- Parent chain: `BP_Kate` → `SandboxCharacter_CMC` → `BP_HawkeyeCharacter` →
  `AHawkeyeCharacter`. The sample's own input context is emptied and its graph bindings are
  dropped on possession; C++ bridges `WantsToSprint`, `WantsToWalk`, `WantsToStrafe`,
  `WantsToAim`, `WantsToCrouch`, and `FullMovementInput` into it each tick. Its camera is
  switched off at runtime. Its graph sets movement speeds (run 500, stop 200, aimed strafe
  180), overriding ours on Kate only; thugs keep the native speeds.
- The full `Hawkeye.Screenshot` group has hung once mid-run; run `Hawkeye.Screenshot.Kate`
  and the others separately.

## Combat clips (not in git)

Mixamo FBX downloads go in `Tools/Data/Anims/Mixamo/`; Paragon Sparrow is read from the Game
Animation Sample's `Content/ParagonSparrow/` (Fab added it there). `.\Tools\import-anims.ps1`
imports or copies them, retargets them to Kate's and the thugs' skeletons and builds the `AM_`
montages (44 s for 29 Mixamo and 7 Sparrow clips), then fills the `DA_AnimSet_` assets.
`-SelfTest` runs the pipeline on clips that are always on this machine and asserts on the bones.
The downloads, the copies (`Content/AnimSources/`, `Content/ParagonSparrow/`,
`Content/Characters/UE5_Mannequins/`) and the results (`.../Animations/Combat/`) are git-ignored; a
fresh clone runs the script again. Details and the fallback rule: `claude-docs/animation.md`.

## Vendored plugins
- `Plugins/SPUD` (Steve's Persistent Unreal Data, MIT, commit 12a30da, source checked in,
  not a submodule). Saves the world state; see the Save section of gameplay-semantics.md.
  Builds on 5.8 unchanged. Update by copying a newer checkout over it and re-running the
  save round-trip test.

## City generation from OpenStreetMap

The district is generated, never hand-placed. Scripts under `Tools/`:

```powershell
.\Tools\fetch-osm.ps1          # downloads Overpass JSON into Tools\Data\osm\ (committed; re-run only to refresh)
.\Tools\test-geo.ps1           # 23 pure-python tests for projection and handedness, no engine needed
.\Tools\generate-city.ps1      # runs Tools\Editor\generate_city.py headless; -Verify also runs verify_city.py
```

Facts that matter:
- Bounding box for the East Village district: the grid-aligned rectangle between the
  centre lines of 1st Ave, Ave C, E 6th St and E 11th St (667 x 376 m plus 22 m margin).
  Origin 40.7264398, -73.9816394. Manhattan's grid is about 29 degrees off true north,
  so streets run diagonally in Unreal; that's correct, not a bug.
- Projection: local tangent plane in `Tools/Editor/_geo.py`. Unreal X = east, Y = south,
  Z up. Verified by tests that the avenues order 1st < A < B < C in X and E 6th is south
  of E 11th.
- Heights: from the OSM `height` tag (524 of 526 buildings have it), else levels x 3.2 +
  1.5, else 15 m. Parapets 90 cm high, 30 cm inset, skipped under 6 m.
- Meshes: one static mesh per building under `Content/City/EastVillage/Meshes/`, built
  with Geometry Script (`append_simple_extrude_polygon`, normals flipped if the signed
  volume is negative), complex-as-simple collision. Each actor carries a `CityHash`
  metadata tag; a rerun rebuilds only meshes whose footprint or height changed, and saves
  nothing when nothing changed.
- Streets and sidewalks from highway ways: default widths 25 m avenues, 12 m streets, 4 m
  sidewalks; OSM rarely tags widths here. Courtyard holes and park footpaths are not built.
- Verify: `verify_city.py` checks record count vs actors, collision, heights within 1 cm,
  handedness, lights, PlayerStart, nav volume, game mode. Screenshots via
  `Hawkeye.Screenshot.EastVillage` into `Saved/Screenshots/City/`.
- `overpass-api.de` rejects PowerShell's default user agent; the fetch script sends its
  own and falls back to `overpass.kumi.systems`.
- World Partition is off for the single block. Stage 3 turns it on.

## Packaging

```powershell
& "$UE\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="$Proj" -platform=Win64 -clientconfig=Shipping -build -cook -stage -pak -archive -archivedirectory="C:\Users\camer\code\hawkeye\Saved\Packaged" -nop4 -utf8output
```

Takes 15-40 minutes. Output under `Saved\Packaged\Windows\`. Stage 6 only.

## Git

- `git lfs install` has been run. `.gitattributes` tracks `*.uasset *.umap *.png *.jpg
  *.wav *.fbx *.mp3 *.tga *.psd *.exr`. Check a new binary type is listed before adding it.
- `.gitignore` excludes `Binaries/ Intermediate/ Saved/ DerivedDataCache/ .vs/ *.sln
  *.VC.db *.opensdf *.sdf`.
- Commit at the end of every working session. Push to a remote (stage 1 done-when
  includes creating one). A repo that only exists on this SSD is one drive failure from
  gone.
- Line endings: the repo will warn LF vs CRLF on text files. Set `git config core.autocrlf
  true` once and stop worrying.

## Machine constraints

- 16 GB RAM. The editor idles at 4-6 GB, climbs to 10+ during shader compiles and
  lighting builds. Visual Studio takes 1-2 GB. Do not run the editor and a full VS build
  at once. Close browsers before builds.
- Only the RTX 4070 (12 GB VRAM) should render. If the editor is slow, check it isn't on
  the Intel iGPU: Windows Settings, Display, Graphics, add UnrealEditor.exe, set High
  performance.
- Derived data cache lives at `%LOCALAPPDATA%\UnrealEngine\Common\DerivedDataCache`. It
  grows to tens of GB. Fine on this disk. Don't delete it casually; it's the shader cache.
- Reboot pending from the Visual Studio install as of 2026-09-17. Until then `dotnet`
  is not on PATH in new shells. The engine's own .NET handles builds, so this only
  affects running `dotnet` directly.

## Environment checks (run when something is weird)

```powershell
& "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.VisualStudio.Workload.NativeGame -property installationPath
Get-ChildItem "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
git lfs ls-files | Measure-Object -Line
Get-Content "C:\Users\camer\code\hawkeye\Saved\Logs\Hawkeye.log" -Tail 50 | Select-String -Pattern "Error|Fatal|Warning: .*Hawkeye"
```

## Load time (measured 2026-09-27)
- `Tools\measure-load.ps1` reads the game mode's "Playable after X s" line from a `-game`
  run. Warm load about 21 to 23 s, cold about 25 s. Roughly 12 s of that is uncooked
  editor data behind the animation sample's 1,300 animations; only a cooked build removes
  it. Mid-session reloads (death, playable scene and back) are 1 to 2 s because shared
  characters and databases stay loaded across level changes.
- Keep the Zen cache server running between launches (saves about 3 s). Python is off in
  `-game`. Motion-matching database indices build during load rather than on the first
  tick, so the first playable frame is under 80 ms.
- Ledges within 40 m spawn at load; the rest stream in nearest-first. Effects and sounds
  stream in after load. An async navmesh build was tried and reverted (3.4 s and thug
  warnings); the synchronous build is 0.5 s.
- Screenshot captures in tests run one at a time, write off the game thread, and fail the
  test if the file is missing, empty, or older than the request.

## Packaged builds (working since 2026-09-27)
- `Tools\package.ps1`: Development by default, `-Shipping`, `-NoZip`, `-NoIoStore`. Cooks
  both maps plus the folders the code loads by path (listed in `DefaultGame.ini`), pak and
  IoStore on, archives to `Saved\Packaged\Windows`, zips to
  `Saved\Packaged\Hawkeye-win64-<date>.zip`. A warm run takes 1 to 3 minutes. Log at
  `Saved\Logs\Package.log`. Nothing under `Saved/` is committed.
- Load time in the package: about 5.5 s on a fresh exe, 2.8 to 3.0 s after, versus 21 s
  in the editor's `-game` mode (uncooked animation data). Sizes: Development 1.21 GB
  (zip 0.64 GB; the exe and symbols are most of it), Shipping 0.87 GB (zip 0.54 GB).
- Automation runs in the packaged Development build (`-ExecCmds="Automation RunTests
  Hawkeye.Smoke.LoadEastVillage; Quit"`); smoke tests carry `ClientContext`.
- Runtime code must not include editor-only headers inside `#if WITH_EDITOR` blocks that
  are needed outside them; the VFX builder and a screenshot test broke the standalone
  target once. `GetActorLabel` is editor-only; use `GetActorNameOrLabel`.
- Open: the packaged log shows about 140 load errors from `CR_UEFN_Mannequin_FullBodyIK`
  (editor-only Control Rig classes referenced by the sample's Mover animation Blueprint,
  which Kate doesn't use). Harmless so far. The NNEDenoiser plugin adds 98 MB and could be
  disabled.
- Desktop shortcut: "Play Hawkeye (Packaged)".

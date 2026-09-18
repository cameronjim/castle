# Infrastructure

Paths, commands, and the machine. Everything here is Windows.

## Paths

| Thing | Path |
|-------|------|
| Project | `C:\Users\camer\code\fps-game` |
| Engine | `C:\Program Files\Epic Games\UE_5.8` |
| Editor | `...\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe` |
| Headless editor | `...\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe` |
| Build script | `...\UE_5.8\Engine\Build\BatchFiles\Build.bat` |
| UAT (packaging) | `...\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat` |
| Visual Studio | `C:\Program Files\Microsoft Visual Studio\2022\Community` |
| Logs | `C:\Users\camer\code\fps-game\Saved\Logs\Castle.log` |
| Crash dumps | `C:\Users\camer\code\fps-game\Saved\Crashes\` |

Set these as variables at the top of any script:

```powershell
$UE = "C:\Program Files\Epic Games\UE_5.8"
$Proj = "C:\Users\camer\code\fps-game\Castle.uproject"
```

## Build

Generate project files (after adding or removing source files, or changing Build.cs):

```powershell
& "$UE\Engine\Build\BatchFiles\Build.bat" -projectfiles -project="$Proj" -game -rocket -progress
```

Build the editor target (what you run 95% of the time):

```powershell
& "$UE\Engine\Build\BatchFiles\Build.bat" CastleEditor Win64 Development -project="$Proj" -waitmutex
```

Build the standalone game target (for packaging tests):

```powershell
& "$UE\Engine\Build\BatchFiles\Build.bat" Castle Win64 Development -project="$Proj" -waitmutex
```

Exit code 0 is success. Errors look like `error C2065` (compiler) or `Error: ... UnrealHeaderTool`
(reflection). UHT errors come first and block compilation; fix them before reading further.

Build time: 2-5 minutes for a full rebuild of the Castle module, 20-40 seconds incremental.
If the editor is open, close it or use Live Coding (Ctrl+Alt+F11 in the editor) instead.
Building with the editor open and Live Coding off produces a DLL the editor won't reload.

## Running the editor

Full editor (visual work only):

```powershell
& "$UE\Engine\Binaries\Win64\UnrealEditor.exe" "$Proj"
```

Play a specific map in a standalone window without the editor UI (fast iteration on feel):

```powershell
& "$UE\Engine\Binaries\Win64\UnrealEditor.exe" "$Proj" /Game/Maps/L_M01_CellBlockD -game -windowed -ResX=1600 -ResY=900 -log
```

## Headless editor

Use `UnrealEditor-Cmd.exe` with `-nullrhi` for anything that doesn't need a screen.
Common flags: `-unattended` (no dialogs), `-nosplash`, `-nop4`, `-stdout`,
`-FullStdOutLogOutput` (log to console), `-NoLogTimes` (cleaner diff-able logs).

Run automation tests:

```powershell
& "$UE\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "$Proj" -ExecCmds="Automation RunTests Castle; Quit" -unattended -nullrhi -nosplash -nop4 -stdout -FullStdOutLogOutput -ReportExportPath="C:\Users\camer\code\fps-game\Saved\Automation"
```

Run a Python script in the editor (asset creation, batch edits):

```powershell
& "$UE\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "$Proj" -run=pythonscript -script="C:\Users\camer\code\fps-game\Tools\Editor\create_input_assets.py" -unattended -nullrhi -nosplash -nop4 -stdout -FullStdOutLogOutput
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

## Packaging

```powershell
& "$UE\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="$Proj" -platform=Win64 -clientconfig=Shipping -build -cook -stage -pak -archive -archivedirectory="C:\Users\camer\code\fps-game\Saved\Packaged" -nop4 -utf8output
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
Get-Content "C:\Users\camer\code\fps-game\Saved\Logs\Castle.log" -Tail 50 | Select-String -Pattern "Error|Fatal|Warning: .*Castle"
```

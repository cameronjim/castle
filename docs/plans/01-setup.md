# Stage 1: Setup

Goal: the Castle project opens in the Unreal editor, the C++ compiles, and every change
is in git. Nothing about the game yet.

## Steps

1. **Finish the engine download.** Epic Games Launcher, Unreal Engine, Library tab. When
   the tile says Launch, you're good. Note the version (5.4, 5.5, whatever it landed on).

2. **Reboot.** Visual Studio's installer left a reboot pending. Do it before opening
   anything else or the build tool won't find `dotnet` on the PATH.

3. **Match the engine version in the project.** Open `Castle.uproject` in a text editor.
   `EngineAssociation` says "5.4". If the launcher installed 5.5 or 5.6, change it to match.
   A mismatch makes the editor ask to convert the project, which works but is annoying.

4. **Generate project files.** Right-click `Castle.uproject` in Explorer, pick
   "Generate Visual Studio project files." This creates `Castle.sln`. If the right-click
   option is missing, run the engine's `UnrealVersionSelector.exe` once (it's under
   `Epic Games/Launcher/Engine/Binaries/Win64`) and try again.

5. **Build.** Open `Castle.sln`. Set the config to "Development Editor" and platform
   "Win64". Build (Ctrl+Shift+B). Expect errors on the first pass. The scaffold has never
   been compiled. Paste the errors into chat and I'll fix them. Typical culprits: interface
   `_Implementation` signatures, `Instanced` arrays in data assets, and header include order.

6. **Launch the editor.** Once it builds, press F5 in Visual Studio (or double-click the
   .uproject). First launch compiles shaders for 5-15 minutes. That's normal, and it only
   happens once per engine version.

7. **Create the content folders.** In the Content Browser make `Missions`, `Flashbacks`,
   `Maps`, `Blueprints`, `Blueprints/Player`, `Blueprints/AI`, `Blueprints/UI`,
   `Blueprints/Bosses`. The Asset Manager config already points at Missions and Flashbacks.

8. **Set the default map.** Make an empty level, save it as `Maps/L_Sandbox`. Project
   Settings, Maps & Modes, set it as both Editor Startup Map and Game Default Map. Confirm
   the Default GameMode shows `CastleGameMode`.

9. **Commit.** `git add -A`, commit. From here on, commit at the end of every session.
   Binary assets go through LFS automatically because of `.gitattributes`. If git ever
   complains a `.uasset` is too large, LFS isn't tracking it; run `git lfs track` on the
   extension and re-add.

10. **Editor preferences worth setting now.** Editor Preferences, General, Source Code,
    set the editor to Visual Studio 2022 (or VS Code if you'd rather write there; the
    compiler is still MSVC either way). Turn on Live Coding (Ctrl+Alt+F11 rebuilds C++
    without closing the editor). Turn off Real-Time viewport when you're not looking at it.
    It saves RAM and GPU.

## Done when
- [ ] Editor opens from the .uproject with no dialogs
- [ ] `Castle.sln` builds Development Editor with zero errors
- [ ] `L_Sandbox` loads, Play in Editor works, you can look around with a default pawn
- [ ] `git status` is clean and `git lfs ls-files` lists at least the sandbox map
- [ ] You've closed and reopened the editor once and it came back in under 2 minutes

## Things that will bite you
- 16 GB RAM: the editor plus Visual Studio plus Chrome is about the limit. Close the
  browser while building.
- Don't put the project in OneDrive or any synced folder. `C:\Users\camer\code\fps-game`
  is fine.
- Don't rename the project. Renaming an Unreal C++ project is a half-day of pain.

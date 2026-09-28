# Combat clips: what to download and where

Everything in this folder except this file and `manifest.json` is git-ignored. Drop the downloads in,
run `.\Tools\import-anims.ps1`, and the clips become game animations (`claude-docs/animation.md`).
A clip that is not here yet is reported and skipped; the game uses its procedural fallback for it.

## Folder layout

```
Tools/Data/Anims/
  README.md                 this file (committed)
  manifest.json             which file is which role, for whom (committed)
  Mixamo/                   Mixamo FBX downloads (ignored)
    Punching.fbx            the one downloaded WITH skin: defines the source skeleton
    Cross Punch.fbx         every other file WITHOUT skin
    ...
```

Paragon Sparrow is not copied here: it stays in the project Fab added it to (the manifest points at
`C:/Users/camer/code/GASP/Content`, or set `HAWKEYE_SPARROW_CONTENT` to another project's `Content`).

## Mixamo (free, needs an Adobe account)

At <https://www.mixamo.com>, pick the **Y Bot** character (any humanoid works; the chain map matches
Mixamo's own rig names with or without the `mixamorig:` prefix). For each clip, search the name,
leave its sliders at the defaults unless noted, and Download with:

| Setting | First clip (`Punching`) | Every other clip |
|---------|-------------------------|------------------|
| Format | FBX Binary (.fbx) | FBX Binary (.fbx) |
| Skin | With Skin | Without Skin |
| Frames per second | 30 | 30 |
| Keyframe reduction | none | none |
| In Place (walk and run clips only) | off | off |

Save each under its search name (`Cross Punch.fbx`), in `Tools/Data/Anims/Mixamo/`. The names below
were downloaded on 2026-09-28 and are mapped in `manifest.json`; "search for" rows are optional extras
whose exact Mixamo name was not checked.

| Mixamo name | Role (manifest) | For |
|-------------|-----------------|-----|
| Punching (With Skin) | Light1 variant | Kate, Clint |
| Cross Punch | Light1 | Kate, Clint |
| Hook Punch | Light2 | Kate, Clint |
| Boxing | Light2 variant | Kate, Clint |
| Uppercut Jab | Light3 | Kate, Clint |
| Jab To Elbow Punch | Light3 variant; Light1 variant for thugs | Kate, Clint, thugs |
| Side Kick | Kick (stands in for Light3 when a set has no Light3) | Kate, Clint |
| Kicking | Kick variant | Kate, Clint |
| Surprise Uppercut | Heavy | Kate, Clint |
| Roundhouse Kick | Heavy variant | Kate, Clint |
| Block | Parry | Kate, Clint |
| Standing Dodge Forward / Backward / Left / Right | DodgeForward / Back / Left / Right | Kate, Clint |
| Leg Sweep | FinisherAttacker | Kate, Clint |
| Flying Kick | FinisherBow | Kate, Clint |
| Standing Melee Punch | Light1 (fists) | thugs |
| Standing Melee Attack Horizontal | Heavy (bat, bash) | thugs |
| Standing Melee Run Jump Attack | Heavy variant | thugs |
| Receive Punch To The Face | HitFront | thugs |
| Receive Uppercut To The Face, Receive Stomach Uppercut, Hit Reaction | HitFront variants | thugs |
| Knocked Out | Knockdown; FinisherVictim variant | thugs |
| Knocked Out (1) | Knockdown variant | thugs |
| Sweep Fall | FinisherVictim | thugs |
| Quad Punch, Knee Jabs To Uppercut, Standing Melee Kick, Body Block, Standing Block Idle, Dying, Standing Death Backward 01 | downloaded, not mapped (deaths are the ragdoll) | |
| search for "Getting Up" or "Stand Up" | GetUp (none yet: a knockdown blends back to locomotion) | thugs |
| search for "Hit Reaction" left, right, back variants | HitLeft / HitRight / HitBack for thugs (Sparrow's are used today) | thugs |

## Paragon: Sparrow (free, Epic)

1. In the Epic Games Launcher, Fab tab (or <https://www.fab.com>), search **Paragon: Sparrow**, add it
   to your library (free).
2. Add it to a project: Launcher, Library, the pack, "Add to Project", pick the Game Animation Sample
   project (`C:\Users\camer\code\GASP`) or any UE 5 project. It lands in that project's
   `Content/ParagonSparrow/`. The pack is about 3 GB; the import copies only what it uses (the clips
   and the Sparrow mesh with its materials, about 730 MB) into this project, git-ignored.
3. If the pack is in another project, set `HAWKEYE_SPARROW_CONTENT` to that project's `Content` folder.

Mapped from `/Game/ParagonSparrow/Characters/Heroes/Sparrow/Animations/`:

| Sparrow clip | Role | For |
|--------------|------|-----|
| RMB_Drawback | BowDraw | Kate, Clint, archers |
| RMB_Loop | BowAimIdle (loops) | Kate, Clint, archers |
| RMB_Fire | BowFire | Kate, Clint, archers |
| HitReact_Fwd / _Bwd / _Left / _Right | HitFront / HitBack / HitLeft / HitRight (baked from additive to full pose) | everyone |

Not mapped, worth a look: `Primary_Fire_Fast/_Med/_Slow` (quick shots), `AimOffset/AO_idle` and the
`idle_AO_*` poses (an aim offset over locomotion), `Knock_Fwd/_Up` (knock-backs; `Knock_Bwd` checked
and it never leaves its feet, so it is not a knockdown), `Death_Fwd/_Bwd`.

## After downloading

```powershell
.\Tools\import-anims.ps1            # import, retarget, build montages, fill the anim sets
.\Tools\import-anims.ps1 -Force     # redo all of it
.\Tools\run-tests.ps1               # then the suite
```

Then commit `manifest.json` and the `Content/Blueprints/Animation/DA_AnimSet_*` assets.

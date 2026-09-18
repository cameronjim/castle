# Asset conventions

## Content folder layout

```
Content/
  Maps/                 L_Sandbox, L_MainMenu, L_M01_CellBlockD ... L_M08_Exit
  Missions/             DA_M01_CellBlockD ... DA_M08_Exit   (UMissionDefinition)
  Flashbacks/
    Definitions/        DA_FB01_Sunday, DA_FB02_TheRoad, DA_FB03_ThePark, DA_FB04_ThreeCoffins
    Images/             T_FB01_01 ... (one folder per flashback if it gets big)
    Audio/              S_FB01_Frank_01 ...
  Blueprints/
    Player/             BP_CastleCharacter, BP_CastlePlayerController, BP_CastleGameMode
    Weapons/            DA_Weapon_Pistol, DA_Weapon_Shotgun, BP_WeaponPickup
    AI/                 BP_Guard, BP_Guard_Baton, BP_Guard_Rifle, BP_Inmate_Broken,
                        BT_Guard, BB_Guard, EQS_CoverPoints, BTT_*, BTS_*, BTD_*
    Bosses/             BP_Boss_Base, BP_Boss_Orderly, BP_Boss_Deacon, BP_Boss_Sniper,
                        BP_Boss_Warden, BP_ArenaController_*, BT_Boss_*
    World/              BP_Door_Keycard, BP_Door_Powered, BP_Vent, BP_Light_Shootable,
                        BP_Generator, BP_Switch, BP_Checkpoint, BP_MissionStarter,
                        BP_Pickup_Keycard, BP_Pickup_Ammo
    UI/                 WBP_Hud, WBP_Flashback, WBP_EndCard, WBP_Subtitle, WBP_BossHealth,
                        WBP_MainMenu, WBP_Pause, WBP_Settings
  Input/                IMC_Default, IMC_Flashback, IA_Move, IA_Look, IA_Jump, IA_Sprint,
                        IA_Crouch, IA_Fire, IA_Aim, IA_Reload, IA_Takedown, IA_Interact,
                        IA_Dodge, IA_Pause, IA_Skip
  Data/                 DT_Dialogue, DT_Settings, CU_DamageFalloff_Pistol (curves)
  Kit/                  Grey-box modular meshes and M_Greybox materials
  Mannequin/            UE mannequin feature pack, copied verbatim (assets hard-reference
                        /Game/Mannequin, so the path is fixed). Used for guards and the
                        first-person arms fallback.
  Weapons/Pistol/       UE template pistol (Meshes, Materials, Textures), copied verbatim
                        for the same reason. Weapons/Rifle/Materials/M_Weapon is its parent.
  Materials/            Procedural M_Concrete, M_ConcreteFloor, M_SteelPainted, M_Pistol,
                        M_KeycardBody, M_Emissive and its MI_ instances, M_FluorescentFlicker
  Characters/Guard/     M_GuardBody, M_GuardVisor
  Environment/          Final art, one subfolder per area: CellBlock, Infirmary, Yard ...
  Characters/           Frank/ (arms), Guards/, Bosses/
  Materials/            M_ master materials, MI_ instances, MF_ functions
  Audio/                SFX/ (S_), Music/ (S_Music_), Voice/ (S_VO_), Ambient/ (S_Amb_)
  VFX/                  NS_ Niagara systems
  Developers/           Scratch. Never referenced by a shipped asset. Not in packages.
```

`Developers/` is where experiments go. Anything referenced from a real asset must move out.

## Prefixes

| Prefix | Type | Example |
|--------|------|---------|
| `L_` | Level | `L_M03_YardRiot` |
| `BP_` | Blueprint class | `BP_Guard_Rifle` |
| `WBP_` | Widget Blueprint | `WBP_Hud` |
| `DA_` | Data asset | `DA_M01_CellBlockD` |
| `DT_` | Data table | `DT_Dialogue` |
| `CU_` | Curve | `CU_DamageFalloff_Pistol` |
| `IA_` / `IMC_` | Input action / mapping context | `IA_Fire`, `IMC_Default` |
| `BT_` / `BB_` | Behavior tree / blackboard | `BT_Guard`, `BB_Guard` |
| `BTT_` / `BTS_` / `BTD_` | BT task / service / decorator | `BTT_MoveToCover` |
| `EQS_` | Environment query | `EQS_CoverPoints` |
| `SM_` / `SK_` | Static / skeletal mesh | `SM_Kit_Wall400`, `SK_Guard` |
| `M_` / `MI_` / `MF_` | Material / instance / function | `M_Concrete`, `MI_Concrete_Wet` |
| `T_` | Texture, with suffix | `T_Concrete_D`, `_N`, `_ORM`, `_M` for mask |
| `S_` | Sound (wave or cue) | `S_Pistol_Fire`, `S_VO_Frank_M01_01`, `S_Amb_CellBlock` |
| `SC_` / `MS_` | Sound cue / MetaSound | `MS_Footsteps` |
| `A_` / `AM_` / `ABP_` | Anim sequence / montage / anim BP | `AM_Takedown_Front`, `ABP_Guard` |
| `NS_` | Niagara system | `NS_MuzzleFlash_Pistol` |
| `PM_` | Physical material | `PM_Concrete` |
| `DMG_` | Damage type | `DMG_Bullet` |

Mission-scoped assets carry the mission tag: `M01`, `FB03`. Flashback images are
numbered in slide order: `T_FB02_04` is the fourth slide of the second flashback.

## Data-driven rules
- A mission is: `L_M0X` + `DA_M0X` + a per-mission one-pager in `docs/missions/`. No
  per-mission C++ or per-mission Blueprint classes. Mission-specific scripting lives in
  the Level Blueprint of `L_M0X` and in placed `BP_World/*` actors.
- A boss is: `BP_Boss_X` (child of `BP_Boss_Base`) + `BT_Boss_X` + `BP_ArenaController_X`
  placed in the level. Phase data is set on the `BossPhaseComponent` in the Blueprint
  defaults.
- A weapon is: `DA_Weapon_X`. No Blueprint subclass unless it has a unique behaviour
  (the shotgun's multi-pellet trace is the one expected exception).
- A flashback is: `DA_FBXX_Name` + textures + audio. Nothing else.
- Dialogue: every spoken line is a row in `DT_Dialogue` with a stable row name
  `m05_doctor_03`, `vent_books_02`, `m07_warden_phase2_01`. Audio asset name matches
  the row name with the `S_VO_` prefix.

## Level dressing
- Every actor a room-art script places carries the `Art_` label prefix so it can be
  found, verified, and removed without touching gameplay actors.
- Dressing never sits on a patrol path: the scripts log any placed footprint within
  60 cm of an `ATargetPoint`, and the smoke test fails if guards stop moving.
- Rooms are built one at a time to a finished look before the next one starts. The
  cell and corridor 1 of M01 are the reference; new rooms match their material and light
  vocabulary (concrete, painted steel, fluorescent tubes, one red emergency light per
  corridor, green exit signs over doors).

## Blueprint hygiene
- Parent class is the C++ class when one exists. `BP_Guard` derives `AGuardCharacter`,
  not `ACharacter`.
- Child Blueprints for variants (`BP_Guard_Rifle` from `BP_Guard`); never duplicate a
  Blueprint to make a variant.
- Every instance-editable variable has a category, a tooltip, and a sane default.
- No asset references from `Content/Kit` or `Content/Developers` into final assets.
- Redirectors: run Fix Up Redirectors on the Content folder after any move or rename,
  before committing.

## Source control for assets
- `.uasset` and `.umap` are LFS. So are textures, audio, and FBX. `.gitattributes` lists
  them; add any new binary extension there first.
- One person edits a map at a time. This is a solo project so it's not a problem, but if
  a second person ever joins, split levels into sublevels or World Partition cells.
- Don't commit `Saved/`, `Intermediate/`, `DerivedDataCache/`, `Binaries/`, or `.vs/`.
- Commit assets with the code that needs them. A commit that adds `UBossPhaseComponent`
  changes should also carry the test boss Blueprint that exercises it, if one changed.

## Scale and units
- 1 Unreal unit = 1 cm. Frank's capsule is 96 cm half-height (192 cm tall), radius 34.
  Eye height 170 cm.
- Kit grid is 100 cm. Walls are 400 x 400 cm faces, 20 cm thick. Doors are 100 wide,
  220 tall. Corridors are 300 wide minimum so two guards can pass.
- Snap settings: location 10 cm for kit pieces, 1 cm for props. Rotation 15 degrees.

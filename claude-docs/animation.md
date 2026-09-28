# Combat animation

How strike, dodge, reaction, finisher and bow clips get from a download into the game, and what the
game does when a clip is missing. The rules themselves are in `gameplay-semantics.md`, "Combat
animation clips". Built 2026-09-27/28.

## The pipeline in one line

`Tools\import-anims.ps1` = `Tools/Editor/import_combat_anims.py` (import or copy, IK rigs, IK
retargeters, batch retarget, `AM_` montages) then `Tools/Editor/create_combat_anims.py` (fill the
`DA_AnimSet_` data assets and put them on the Blueprints). Everything reads
`Tools/Data/Anims/manifest.json`. `Tools\import-anims.ps1 -SelfTest` proves the whole path on clips
that are always on this machine.

## What was used, and why

| Need | Used | Why |
|------|------|-----|
| Retargeting | The engine's IK Rig and IK Retargeter, driven from Python: `IKRigController` for the rigs, `IKRetargeterController` (`add_default_ops`, `auto_map_chains`, a target retarget pose with `auto_align_all_bones`, the root motion op controller), `IKRetargetBatchOperation.run_batch_retarget` for the clips | It is Epic's supported path in 5.8 and fully scriptable headless. Nothing written from scratch. |
| Target rigs | Built by the script from the manifest's chain maps, on the project's own meshes (`IK_Kate_UEFN`, `IK_Thug_Mannequin`) | Epic's `IK_UEFN_Mannequin` from the Game Animation Sample was tried first. Its stored skeleton lacks this project's `attach` bone, and every retargeted track came out one bone down the hierarchy (pelvis at the origin, spine carrying the pelvis). The self-test's raw-bone checks now catch that. GASP's `RTG_UEFN_to_*` retargeters all go from UEFN outward, so none could be reused for clips coming in. |
| Montages | `UHawkeyeCombatMontageBuilder` (C++, editor-only): the engine's `UAnimMontageFactory` plus `UAnimationBlueprintLibrary`'s notify calls | Python cannot set a montage's slot name; C++ can, and the compiler checks it. |
| Aiming strikes at the target | The engine's Motion Warping plugin: the `MotionWarping` component the sample already puts on Kate (one is added only to a character without one), a skew-warp window in each strike montage, the warp target `CombatTarget` | GASP's traversal already uses it on Kate; a second component would warp every clip twice. |
| Upper body over locomotion | Slots in the bow-IK post-process graph (`ABP_BowIK_Post`, `_Thug`): `DefaultSlot`, cached, then `UpperBody` layered from `spine_01` up in mesh space, then the existing lean, spine turn and two-bone IK | The graph was already built headless by `UHawkeyeBowIKGraphBuilder`, runs on every character, and runs after the main AnimBP, so IK can correct a clip's hands. The sample's own AnimBP is left untouched. |
| FBX import | `AssetImportTask` with `FbxImportUI` options, which 5.8 converts for Interchange | The documented scripted import; the self-test runs it on the engine's own FBX automation file. |

## Where clips play

- A montage whose slot is `UpperBody` (the bow clips) plays on the post-process instance, over
  locomotion from `spine_01` up.
- Any other montage plays full body: on Kate and Clint in the sample AnimBP's `DefaultSlot` (the same
  slot the parkour clips use), on thugs and archers in the post-process graph's `DefaultSlot`
  (their main instance is a single looping sequence and has no slots).
- `HawkeyeCombatAnim::PickInstance` makes that choice; everything else calls
  `HawkeyeCombatAnim::Play` or `PlayRole` and treats a null return as "use the fallback".

## The fallback rule

Every role is optional. An empty slot in the set, a montage that does not load, or one that will not
play (no anim instance, another skeleton) sends the code down exactly the path it had before clips:
the strike pose and lunge, the procedural dash, the hit lean, the ragdoll knockdown, the IK-only bow.
The automation tests run with no content, so they exercise the fallback every time;
`Hawkeye.CombatAnim.NoClipKeepsProceduralBehaviour` fills every role with a montage that cannot play
and checks the old timings.

## Skeletons

| Who | Skeleton | Target rig | Clips land in |
|-----|----------|------------|---------------|
| Kate, Clint | `SK_UEFN_Mannequin` (the sample's) | `IK_Kate_UEFN` | `/Game/Characters/UEFN_Mannequin/Animations/Combat` |
| Thugs, the heavy, the gunner, archers | `SK_Mannequin_Skeleton` (the UE4 mannequin in `/Game/Mannequin`) | `IK_Thug_Mannequin` | `/Game/Mannequin/Animations/Combat` |

Note the thugs are not on the UEFN skeleton: they wear the UE4 mannequin, so every clip a thug uses is
retargeted twice, once per target.

Chain maps (in the manifest, `chain_maps`): all use the same chain names as Epic's UEFN rig (Root,
Spine, Neck, Head, Left/RightClavicle, Left/RightArm, fingers, Left/RightLeg, Left/RightToe), so the
retargeter maps them by exact name.

| Map | Source | Retarget root | Root bone | Notes |
|-----|--------|---------------|-----------|-------|
| `mixamo` | Mixamo's rig: Hips, Spine..Spine2, Neck, Head, LeftShoulder, LeftArm..LeftHand, LeftHandThumb1..3 (and Index, Middle, Ring, Pinky), LeftUpLeg..LeftFoot, LeftToeBase | Hips | none | Bone names match with or without a `mixamorig:` prefix. With no root bone the root motion op generates root motion from the target pelvis. |
| `ue4` | UE4 mannequin and Paragon heroes (Sparrow): root, pelvis, spine_01..03, neck_01, head, clavicle, upperarm..hand, three-joint fingers, thigh..foot, ball | pelvis | root | 22 chains on Sparrow. |
| `uefn` | UEFN and UE5 mannequins: spine_01..05, neck_01..02 | pelvis | root | Kate's target; the self-test's UE5 source. |

## Sources

- `mixamo_fbx`: a folder of FBX. The source's `mesh_file` (one clip downloaded With Skin) imports as
  a skeletal mesh and defines the skeleton; then every clip, that one included, imports as an
  animation onto it (`/Game/AnimSources/<Source>/A_Src_<File>`). Frames snap to 30 fps.
- `uasset_project`: clips in another project's `Content` folder. The clips, the source mesh and their
  hard-reference closure are copied to the same `/Game/` paths, never overwriting.
  `Tools/Editor/_packages.py` reads the import tables of both UE 5.8 and UE4 packages (the Paragon
  packs were saved by 4.19), so a copy takes what a package loads and nothing else: Sparrow's six
  clips are 3 MB, its mesh with materials 730 MB.

Paragon's hit reactions are additive over its idle. They are retargeted with additive off
(`retain_additive_flags=False`), which bakes idle plus hit into a full pose; kept additive they played
as garbage in a full-body slot and pointed at Sparrow's idle on Sparrow's skeleton.

## How to add a clip

1. Put the file in `Tools/Data/Anims/<Source>/` (or the pack in its project) and add a line to
   `manifest.json` `clips`: `source`, `file` or `asset`, `role`, `variant`, `characters`
   (`kate`, `clint`, `thug`, `archer`, `all`, or a list).
2. Optional per clip: `hit`, `combo` (fractions of the clip), `warp_end`, `loop`, `hold`,
   `root_motion`, `blend_in`, `blend_out`, `slot`. Leave them out to take `ROLE_DEFAULTS` in
   `import_combat_anims.py` (strikes: hit 25-45%, combo 45-80%, warp to the hit; heavy 40-55%, 60-85%;
   Knockdown holds its last frame; BowAimIdle loops; bow roles play `UpperBody`).
3. `.\Tools\import-anims.ps1` (add `-Force` to redo existing assets). Read the log: one `created`
   line per montage with its windows, one `pose` (or `check`) line per retargeted clip with its
   pelvis height at start, middle and end against the reference, and the head and hands.
4. When a role has several clips, the first one in manifest order wins; reorder the manifest to
   change it. Hand-made `AM_<Role>_*` montages in the folder come after, by name.
5. Commit the manifest and the `DA_AnimSet_` assets. The clips, the copies and the retargeted
   results are git-ignored (Mixamo's and Epic's content is not republished); a fresh clone rebuilds
   them with the script.

## Checking without looking

Nobody can watch a headless run, so the scripts read bones:

- `--selftest` retargets two of GASP's UE5 mannequin clips (another skeleton, a `uasset_project`
  source), the engine's `FbxEditorAutomation/AnimatedCharacter.fbx` (an FBX source with no root bone),
  and Sparrow's `RMB_Drawback` and `HitReact_Fwd` to both targets, then asserts: on the target
  skeleton, the source's length, only target bones in the tracks, full-pose not additive, the right
  slot and windows, standing clips standing (pelvis within 35 cm of the reference, hips and head
  where they belong), and for the bow draw: the bow arm as extended as the source's, the bow hand
  30+ cm in front, the string hand drawn back 40+ cm behind it at full draw. 98 checks.
- Poses are read from the raw bone tracks with `UHawkeyeCombatMontageBuilder::GetRawBoneLocation`.
  `AnimPoseExtensions` evaluation gave the pelvis as the origin on some UEFN clips while its children
  were right, so it is not trusted for this.
- `Hawkeye.Smoke.CombatClipsPlay` loads the district and plays each character's clips: the slot's
  global weight must be above 0 a quarter second later, and Kate's Light1 must still be winding up
  before its `ANS_HitWindow` and recovering after it (the notify firing from a real montage).

## Known limits

- Mixamo strike clips are long (Cross Punch 2.0 s, hit at 0.5 s with the default window). A clip-timed
  light lands much later than the procedural 0.1 s. Tune the clip's `hit` and `combo` in the manifest
  after playing it.
- Thug clips play at the rate that opens their hit window at the telegraph's end (fists 0.6 s), so a
  1.0 s Mixamo punch with its hit at 25% plays at about 0.4x. Tighten the clip's `hit` to fix it.
- There is no GetUp clip yet: a Mixamo knockdown holds its last frame on the floor, then blends back
  to locomotion over 0.4 s.
- No aim offset: the bow clips aim where the clip aims and the spine turn adds the yaw to the camera;
  pitch comes from the bow mesh only. Sparrow's AO set (`AO_idle` and the `idle_AO_*` poses) is
  there to build one from.
- Sparrow's `Knock_Bwd` is a standing knock-back (pelvis never drops), not a knockdown; it is not
  mapped.

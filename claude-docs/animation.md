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
| `mixamo` | Mixamo's rig: Hips, Spine..Spine2, Neck, Head, LeftShoulder, LeftArm..LeftHand, LeftHandThumb1..3 (and Index, Middle, Ring, Pinky), LeftUpLeg..LeftFoot, LeftToeBase | Hips | none | Bone names match with or without a `mixamorig:` prefix. With no root bone the root motion op generates root motion from the target pelvis: root straight under the pelvis, height snapped to the ground, reference rotation (see "Root motion from a rig with no root"). |
| `ue4` | UE4 mannequin and Paragon heroes (Sparrow): root, pelvis, spine_01..03, neck_01, head, clavicle, upperarm..hand, three-joint fingers, thigh..foot, ball | pelvis | root | 22 chains on Sparrow. |
| `uefn` | UEFN and UE5 mannequins: spine_01..05, neck_01..02 | pelvis | root | Kate's target; the self-test's UE5 source. |

## Root motion from a rig with no root

The IK Retargeter's root motion op, set to Generate From Target Pelvis, still defaults its height to
Copy Height From Source, which reads the source's root bone; Mixamo has none, so that is its first
bone, the hips. Every Mixamo clip came out with the root track at 85 to 92 cm and the pelvis's own
track about 2 cm above it (A_Light1_Cross: root z 89, pelvis 91 in component space at frame 0). The
import's pose line read the pelvis through the root and looked right; the game extracts the root as
root motion (Kate's strikes) or locks it (everything else), and the pelvis dropped to the floor. The
script now sets the op to Snap To Ground with no offset from the pelvis and no pelvis rotation, so the
root carries only the hips' horizontal travel (A_Light1_Cross after: root z 0, pelvis 91 to 94).

Two more things made that fix look like it did nothing: `IKRetargetFactory` can create the retargeter
with the default op stack already in, and a retargeter the script could not delete (still loaded)
came back with every earlier run's stack, so `add_default_ops` stacked more copies whose root motion
op sat at its defaults and ran last. The script clears the stack and adds exactly one. The import log
prints the op count ("6 ops": the default five and the Pin Bones op below).

## The IK bones

The UEFN and UE4 mannequins carry `ik_foot_l`, `ik_foot_r`, `ik_hand_l`, `ik_hand_r` and `ik_hand_gun`,
and the Game Animation Sample's AnimBP runs foot placement and leg IK toward the foot ones. A retarget
leaves them at their reference pose, so until 2026-09-28 every Mixamo clip had Kate's feet pulled back
to the reference stance in game: the kick's foot stayed on the floor at its hit and the strikes lost
their split stance. The retargeter now ends with a Pin Bones op that copies each from the foot or hand
it follows (translation and rotation, no offset: they coincide in both reference poses; the op's
"maintain offset" measures in the retarget pose, where the aligned legs have moved, and was 3 to 8 cm
off). The self-test checks the foot ones on the clip's keys.

## Timing a clip for play

Mixamo strikes are demo-paced: about a second of wind-up before the punch. A clip can say, in its
source's seconds, which stretch to play and how fast (`start_s`, `end_s`, `rate`) and where its windows
are (`hit_s`, `combo_s`, optional `warp_end_s`; the warp defaults to the hit). The montage builder puts
the stretch and rate into the montage's own segment (the montage editor's Start Time, End Time and
Play Rate), so the montage is short and plays at rate 1, and maps the windows into it. A hit lands
`(hit_s[0] - start_s) / rate` after the input. The numbers for Kate's clips come from the fist or foot's
forward reach from the pelvis per frame (read with `GetRawBoneLocation`): the hit window opens when
the striking hand is about two thirds out, and the stretch starts at or just before the cocked pose.

| Clip | Variant | Stretch (s) | Rate | Hit after input | Combo window | Montage |
|------|---------|-------------|------|-----------------|--------------|---------|
| AM_Light1_Cross (right cross) | 1 of 2 | 0.60-1.70 | 1.7 | 0.25 s | 0.34-0.62 s | 0.65 s |
| AM_Light1_Punching (left jab from a dip) | 2 of 2 | 0.13-1.00 | 1.5 | 0.25 s | 0.33-0.56 s | 0.58 s |
| AM_Light2_Hook | 1 of 2 | 0.70-1.75 | 1.3 | 0.25 s | 0.35-0.77 s | 0.81 s |
| AM_Light2_Boxing (jab, then the right cross that hits) | 2 of 2 | 0.25-1.05 | 1.4 | 0.25 s | 0.33-0.55 s | 0.57 s |
| AM_Light3_UppercutJab (the left uppercut) | 1 of 2 | 0.45-1.25 | 1.4 | 0.24 s | 0.34-0.55 s | 0.57 s |
| AM_Light3_JabElbow (the left jab; the elbow after it is cut) | 2 of 2 | 0.48-1.40 | 1.7 | 0.25 s | 0.34-0.52 s | 0.54 s |
| AM_Kick_Kicking (the front kick) | 1 of 2 | 0.20-1.35 | 1.2 | 0.29 s | 0.46-0.92 s | 0.96 s |
| AM_Kick_SideKick | 2 of 2 | 0.20-1.45 | 1.4 | 0.29 s | 0.46-0.87 s | 0.89 s |
| AM_Heavy_SurpriseUppercut | 1 of 2 | 0.65-1.80 | 1.1 | 0.41 s | 0.59-1.00 s | 1.05 s |
| AM_Heavy_Roundhouse (right roundhouse kick) | 2 of 2 | 0.40-1.65 | 1.2 | 0.39 s | 0.58-1.00 s | 1.04 s |

Variants (2026-09-28): each strike takes the next clip of its role in manifest order, so Kate's heavy
goes Surprise Uppercut, Roundhouse, Surprise Uppercut; the log line "heavy variant 2 of 2 for
ECombatAnimRole::Heavy: AM_Heavy_Roundhouse" says which. The second variants were measured the same
way (`GetRawBoneLocation` per frame of the retargeted `A_` clip): the hit opens when the fist or foot is
80 to 90% of its way out. The Roundhouse's standing leg straightens from her bent-knee idle: her pelvis
is 16 cm above standing at the hit (95 against 79 cm), the standing foot flat on the ground.
The Kick role plays Kicking first (2026-09-28): at the side kick's hit Kate's torso is near flat over her
standing hip, head at 123 cm (its import `check` line says so: head +23 at mid-clip); the front kick
stays upright (head 142 cm) and lands at the same time. Side Kick is the second kick variant; only the
Light3 fallback and the kick shot play the Kick role, so whether it stays is Cameron's call. The thugs'
fists alternate Standing Melee Punch and Jab To Elbow (both fitted to the telegraph); the run-jump attack
is `"in_set": false`.

Thug strikes are trimmed the same way but kept at rate 1 in the manifest; the melee component fits
them to the attack's telegraph when they play (`HawkeyeCombatAnim::FitHitToWindup`): the rate that
puts the hit window on the telegraph's end, held between 0.8x and 1.3x; past 1.3x the clip starts
later in its wind-up, below 0.8x it holds its first frame first. The log line says which ("plays
AM_Light1_MeleePunch at 0.80x from 0.00 s, held 0.29 s") and "hit window opens 0.61 s into the swing".
The numbers come from the striking hand's forward reach per frame, as for Kate's.

| Clip | Stretch (s) | Hit (s) | Swings | Fit, hit lands at | Hit to end |
|------|-------------|---------|--------|-------------------|------------|
| AM_Light1_MeleePunch | 0.00-0.75 | 0.25-0.35 | fists 0.6 s | held 0.29 s, 0.8x, 0.60 s | 0.63 s |
| AM_Heavy_MeleeHorizontal | 0.00-1.50 | 0.87-0.97 | bat 0.6 s, bash 0.8 s, slow swing 1.0 s | 1.3x from 0.09 s, 0.60 s; 1.09x, 0.80 s; 0.87x, 1.00 s | 0.48, 0.58, 0.72 s |
| AM_Light1_JabElbow (fists, second variant) | 0.30-1.40 | 0.85-0.95 | fists 0.6 s | 0.92x, 0.60 s | 0.60 s |
| AM_Heavy_RunJumpAttack (`in_set: false`) | 0.55-2.40 | 1.64-1.74 | | a jump attack; its pelvis leaves the ground | |

Standing Melee Punch has only 0.25 s of wind-up, hence the hold; the Jab To Elbow jab fits 0.6 s
with none, and since 2026-09-28 every second fists swing plays it. The thug's reactions: Knocked
Out plays 0.80-2.60 s (0.9 s of standing still cut; on the floor by 1.4 s, holding its last frame
there), Sweep Fall 0.60-2.00 s (starts as his feet go). Receive Punch To The Face starts reacting on
its second frame and is untrimmed.

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
   `root_motion`, `blend_in`, `blend_out`, `slot`; or the timing keys in seconds of the source clip,
   `start_s`, `end_s`, `rate`, `hit_s`, `combo_s`, `warp_end_s` (see "Timing a clip for play"). `"in_set": false` imports a clip but keeps it out of the anim set. Leave the timing keys out to take `ROLE_DEFAULTS` in
   `import_combat_anims.py` (strikes: hit 25-45%, combo 45-80%, warp to the hit; heavy 40-55%, 60-85%;
   Knockdown holds its last frame; BowAimIdle loops; bow roles play `UpperBody`).
3. `.\Tools\import-anims.ps1` (add `-Force` to redo existing assets). Read the log: one `created`
   line per montage with its windows, one `pose` (or `check`) line per retargeted clip with its
   pelvis height at start, middle and end against the reference, and the head and hands.
4. When a role has several clips, all of them go in the set in manifest order: the first in the role's
   slot, the rest in `MoreVariants`. Strikes cycle through them; every other role plays the first.
   Reorder the manifest to change the order, `"in_set": false` to leave one out. Hand-made
   `AM_<Role>_*` montages in the folder come after, by name.
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
  where they belong, and the pelvis within 15 cm of the reference with the root locked: the engine's
  FBX character has no root bone, so this is the check that fails if the root takes the hips' height), and for the bow draw: the bow arm as extended as the source's, the bow hand
  30+ cm in front, the string hand drawn back 40+ cm behind it at full draw; and `ik_foot_l` and
  `ik_foot_r` on the feet on every key. 134 checks.
- Poses are read from the raw bone tracks with `UHawkeyeCombatMontageBuilder::GetRawBoneLocation`
  (`bRootLocked` holds the root at its reference pose, as the game does; the `pose` line prints both).
  `AnimPoseExtensions` evaluation gave the pelvis as the origin on some UEFN clips while its children
  were right, so it is not trusted for this.
- `Hawkeye.Smoke.ThugClipsStrikeOnTheTelegraph` swings the district's street thug (fists, bat) and the
  heavy (bash, slow swing) with their clips and checks each hit window opens on the telegraph's end.
- `Hawkeye.Screenshot.Melee` shoots two heavies and the kick side on at their hit windows
  (heavy_strike.png, heavy_strike_2.png, kick.png) and fails on the wrong montage (the second heavy
  repeating the first's clip is wrong), or a pelvis more than 15 cm below standing or 20 cm above it.
- `Hawkeye.Smoke.CombatClipsPlay` loads the district and plays each character's clips: the slot's
  global weight must be above 0 a quarter second later, and Kate's Light1 must still be winding up
  before its `ANS_HitWindow` and recovering after it (the notify firing from a real montage).

## Known limits

- Kate's clip-timed lights land at 0.25 s, not the procedural 0.1 s (a clip needs a visible swing).
- The thug's fists hold their first frame for 0.29 s of the 0.6 s telegraph (Standing Melee Punch's
  wind-up is short). It reads as squaring up, not as slow motion; a clip with a longer wind-up
  (the Jab To Elbow jab) would need no hold.
- There is no GetUp clip yet: a Mixamo knockdown holds its last frame on the floor, then blends back
  to locomotion over 0.4 s.
- No aim offset: the bow clips aim where the clip aims and the spine turn adds the yaw to the camera;
  pitch comes from the bow mesh only. Sparrow's AO set (`AO_idle` and the `idle_AO_*` poses) is
  there to build one from.
- Sparrow's `Knock_Bwd` is a standing knock-back (pelvis never drops), not a knockdown; it is not
  mapped.

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/FirstPersonArmsComponent.h"

namespace
{
	FCastleArmBonePose MakeBonePose(
		const TCHAR* BoneName, const TCHAR* ChildBone, const FVector& Direction, float Twist = 0.f)
	{
		FCastleArmBonePose Pose;
		Pose.BoneName = FName(BoneName);
		Pose.ChildBone = FName(ChildBone);
		Pose.Direction = Direction;
		Pose.TwistDegrees = Twist;
		return Pose;
	}

	/** The mannequin's five finger chains. Each has joints 01, 02 and 03, per hand. */
	const TCHAR* FingerNames[] = { TEXT("thumb"), TEXT("index"), TEXT("middle"), TEXT("ring"), TEXT("pinky") };

	const TCHAR* ArmRootRight = TEXT("upperarm_r");
	const TCHAR* ArmRootLeft = TEXT("upperarm_l");
	const TCHAR* HandRight = TEXT("hand_r");
	const TCHAR* HandLeft = TEXT("hand_l");

	FName FingerBone(const TCHAR* Finger, int32 Depth, bool bRightHand)
	{
		return FName(*FString::Printf(TEXT("%s_0%d_%s"), Finger, Depth, bRightHand ? TEXT("r") : TEXT("l")));
	}
}

UFirstPersonArmsComponent::UFirstPersonArmsComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	SetOnlyOwnerSee(true);
	SetCastShadow(false);
	bCastDynamicShadow = false;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// The mannequin is a whole body. Hiding a bone hides its children, so the thighs take the
	// legs with them and the neck takes the head. The spine stays: the clavicles hang off it,
	// and hiding it would take the arms as well.
	HiddenArmBones = {
		FName(TEXT("head")), FName(TEXT("neck_01")),
		FName(TEXT("thigh_l")), FName(TEXT("thigh_r"))
	};

	// Component space on the mannequin: +Y is forward (down the camera), +X is the character's
	// left, +Z is up. Castle.ViewModel.FingerBonesExist prints the reference skeleton: the
	// shoulders sit at (-+17.7, -9.7, 149.5), the upper arm is 30.3 cm and the forearm 27.0.
	// The camera sits at component (5, 0, 154), which is where these targets are measured from.
	//
	// A boxer's guard, solved from where the fists should be rather than eyeballed: each fist
	// 35 cm in front of the camera, 12 cm below the eye line and 12.5 cm off the centre line, so
	// the pair sits 25 cm apart with the elbows pulled down and in. Each pair of directions is
	// the triangle that reaches that point. 12 cm and not the 20 the brief asked for: at 35 cm
	// out, half the screen's height is 19.7 cm, so 20 cm below the eye line put the fists exactly
	// on the bottom edge of the frame and cropped them in half.
	PoseFists = {
		MakeBonePose(TEXT("upperarm_r"), TEXT("lowerarm_r"), FVector(0.163f, 0.716f, -0.679f)),
		MakeBonePose(TEXT("lowerarm_r"), TEXT("hand_r"), FVector(0.195f, 0.853f, 0.483f)),
		// The hand flattens off the forearm so the knuckles face down the camera, not up at it.
		MakeBonePose(TEXT("hand_r"), TEXT("middle_01_r"), FVector(0.185f, 0.945f, 0.270f)),
		MakeBonePose(TEXT("upperarm_l"), TEXT("lowerarm_l"), FVector(-0.003f, 0.710f, -0.704f)),
		MakeBonePose(TEXT("lowerarm_l"), TEXT("hand_l"), FVector(-0.004f, 0.859f, 0.511f)),
		MakeBonePose(TEXT("hand_l"), TEXT("middle_01_l"), FVector(-0.004f, 0.950f, 0.312f))
	};

	PosePistol = {
		MakeBonePose(TEXT("upperarm_r"), TEXT("lowerarm_r"), FVector(0.15f, 0.80f, -0.58f)),
		MakeBonePose(TEXT("lowerarm_r"), TEXT("hand_r"), FVector(0.15f, 0.98f, 0.10f)),
		MakeBonePose(TEXT("hand_r"), TEXT("middle_01_r"), FVector(0.15f, 0.98f, 0.10f)),
		// Two-hand hold: the left hand comes across the body and wraps the right from the
		// outside, 5 cm to its left and 3 cm short of it, so it supports the grip instead of
		// hanging in space. The directions lean towards -X, the character's right.
		MakeBonePose(TEXT("upperarm_l"), TEXT("lowerarm_l"), FVector(-0.350f, 0.767f, -0.539f)),
		MakeBonePose(TEXT("lowerarm_l"), TEXT("hand_l"), FVector(-0.415f, 0.909f, -0.020f)),
		MakeBonePose(TEXT("hand_l"), TEXT("middle_01_l"), FVector(-0.415f, 0.909f, -0.020f))
	};

	for (const TCHAR* Finger : FingerNames)
	{
		const bool bThumb = FCString::Strcmp(Finger, TEXT("thumb")) == 0;
		for (int32 Depth = 1; Depth <= 3; ++Depth)
		{
			for (const bool bRight : { true, false })
			{
				FCastleFingerJoint Joint;
				Joint.BoneName = FingerBone(Finger, Depth, bRight);
				Joint.Depth = Depth;
				Joint.bThumb = bThumb;
				Joint.bRightHand = bRight;
				FingerJoints.Add(Joint);
			}
		}
	}
}

const TArray<FCastleArmBonePose>& UFirstPersonArmsComponent::GetPoseTable(ECastleArmsPose Pose) const
{
	return Pose == ECastleArmsPose::Pistol ? PosePistol : PoseFists;
}

TArray<FName> UFirstPersonArmsComponent::GetAllPoseBoneNames() const
{
	TArray<FName> Names;
	for (const TArray<FCastleArmBonePose>& Table : { PoseFists, PosePistol })
	{
		for (const FCastleArmBonePose& Bone : Table)
		{
			Names.AddUnique(Bone.BoneName);
			Names.AddUnique(Bone.ChildBone);
		}
	}
	for (const FName& Bone : HiddenArmBones)
	{
		Names.AddUnique(Bone);
	}
	for (const FCastleFingerJoint& Joint : FingerJoints)
	{
		Names.AddUnique(Joint.BoneName);
	}
	return Names;
}

TArray<FName> UFirstPersonArmsComponent::GetFingerBoneNames() const
{
	TArray<FName> Names;
	Names.Reserve(FingerJoints.Num());
	for (const FCastleFingerJoint& Joint : FingerJoints)
	{
		Names.AddUnique(Joint.BoneName);
	}
	return Names;
}

float UFirstPersonArmsComponent::GetCurlDegrees(ECastleArmsPose Pose, bool bRightHand) const
{
	if (Pose == ECastleArmsPose::Pistol)
	{
		return bRightHand ? PistolRightCurlDegrees : PistolLeftCurlDegrees;
	}
	return FistsCurlDegrees;
}

FVector UFirstPersonArmsComponent::ComputeCurlAxis(bool bRightHand)
{
	// A skeleton without fingers simply does not get a curl; the hands stay as authored.
	for (const TCHAR* Finger : FingerNames)
	{
		if (GetBoneIndex(FingerBone(Finger, 1, bRightHand)) == INDEX_NONE)
		{
			return FVector::ZeroVector;
		}
	}

	const FVector Knuckle = GetBoneLocationByName(FingerBone(TEXT("middle"), 1, bRightHand), EBoneSpaces::ComponentSpace);
	const FVector Tip = GetBoneLocationByName(FingerBone(TEXT("middle"), 3, bRightHand), EBoneSpaces::ComponentSpace);
	const FVector Index = GetBoneLocationByName(FingerBone(TEXT("index"), 1, bRightHand), EBoneSpaces::ComponentSpace);
	const FVector Pinky = GetBoneLocationByName(FingerBone(TEXT("pinky"), 1, bRightHand), EBoneSpaces::ComponentSpace);
	const FVector Thumb = GetBoneLocationByName(FingerBone(TEXT("thumb"), 3, bRightHand), EBoneSpaces::ComponentSpace);

	const FVector Along = (Tip - Knuckle).GetSafeNormal();
	const FVector Across = (Index - Pinky).GetSafeNormal();
	if (Along.IsNearlyZero() || Across.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}

	// Which side of the four-finger plane the palm is on. Taking it from the thumb rather than
	// from a hard-coded axis keeps this honest whichever way the skeleton's hands are built.
	FVector Palm = FVector::CrossProduct(Across, Along).GetSafeNormal();
	if (FVector::DotProduct(Thumb - Knuckle, Palm) < 0.f)
	{
		Palm = -Palm;
	}

	FVector Axis = FVector::CrossProduct(Along, Palm).GetSafeNormal();
	// And which way round that axis closes rather than opens the hand, measured rather than
	// assumed: the engine's rotation handedness is not something to guess at.
	const FVector Probe = FQuat(Axis, 0.1f).RotateVector(Along);
	if (FVector::DotProduct(Probe - Along, Palm) < 0.f)
	{
		Axis = -Axis;
	}
	return Axis;
}

float UFirstPersonArmsComponent::GetPunchAlpha() const
{
	if (PunchElapsed < 0.f || PunchSeconds <= 0.f)
	{
		return 0.f;
	}
	return FMath::Sin(PI * FMath::Clamp(PunchElapsed / PunchSeconds, 0.f, 1.f));
}

void UFirstPersonArmsComponent::PlayPunch()
{
	// Alternate, so a held melee button reads as a combination rather than one twitching arm.
	bPunchRightHand = !bPunchRightHand;
	PunchElapsed = 0.f;
	ApplyPose();
}

void UFirstPersonArmsComponent::InitialiseArms()
{
	if (!GetSkinnedAsset())
	{
		// A greybox pawn with no arms mesh still remembers which pose it is in, which is all a
		// test can check; there is simply nothing to write the rotations onto.
		return;
	}

	for (const FName& BoneName : HiddenArmBones)
	{
		if (GetBoneIndex(BoneName) != INDEX_NONE)
		{
			HideBoneByName(BoneName, EPhysBodyOp::PBO_None);
		}
	}

	PosedBones.Reset();
	ReferenceRotations.Reset();
	ReferenceDirections.Reset();
	ReferenceLocations.Reset();

	for (const TArray<FCastleArmBonePose>& Table : { PoseFists, PosePistol })
	{
		for (const FCastleArmBonePose& Bone : Table)
		{
			if (PosedBones.Contains(Bone.BoneName) || GetBoneIndex(Bone.BoneName) == INDEX_NONE
				|| GetBoneIndex(Bone.ChildBone) == INDEX_NONE)
			{
				continue;
			}

			// Read before anything is posed, so these really are the reference A-pose.
			const FVector Start = GetBoneLocationByName(Bone.BoneName, EBoneSpaces::ComponentSpace);
			const FVector End = GetBoneLocationByName(Bone.ChildBone, EBoneSpaces::ComponentSpace);
			if ((End - Start).IsNearlyZero())
			{
				continue;
			}

			PosedBones.Add(Bone.BoneName);
			ReferenceDirections.Add(Bone.BoneName, (End - Start).GetSafeNormal());
			ReferenceRotations.Add(Bone.BoneName,
				GetBoneRotationByName(Bone.BoneName, EBoneSpaces::ComponentSpace).Quaternion());
		}
	}

	// The finger joints are curled rather than aimed, so they only need their reference rotation.
	for (const FCastleFingerJoint& Joint : FingerJoints)
	{
		if (GetBoneIndex(Joint.BoneName) == INDEX_NONE)
		{
			continue;
		}
		ReferenceRotations.Add(Joint.BoneName,
			GetBoneRotationByName(Joint.BoneName, EBoneSpaces::ComponentSpace).Quaternion());
	}

	// Where each arm starts, so a jab can slide the whole arm forward and put it back.
	for (const TCHAR* ArmRoot : { ArmRootRight, ArmRootLeft })
	{
		const FName BoneName(ArmRoot);
		if (GetBoneIndex(BoneName) != INDEX_NONE)
		{
			ReferenceLocations.Add(BoneName, GetBoneLocationByName(BoneName, EBoneSpaces::ComponentSpace));
		}
	}

	CurlAxisRight = ComputeCurlAxis(true);
	CurlAxisLeft = ComputeCurlAxis(false);

	bArmsInitialised = true;
	ApplyPose();
}

void UFirstPersonArmsComponent::SetPose(ECastleArmsPose NewPose, bool bImmediate)
{
	if (NewPose == ActivePose && BlendAlpha >= 1.f)
	{
		return;
	}

	PreviousPose = bImmediate ? NewPose : ActivePose;
	ActivePose = NewPose;
	BlendAlpha = (bImmediate || PoseBlendSeconds <= 0.f) ? 1.f : 0.f;

	ApplyPose();
}

void UFirstPersonArmsComponent::TickComponent(
	float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const bool bBlending = BlendAlpha < 1.f;
	if (bBlending)
	{
		BlendAlpha = FMath::Clamp(
			BlendAlpha + DeltaTime / FMath::Max(PoseBlendSeconds, KINDA_SMALL_NUMBER), 0.f, 1.f);
	}

	bool bPunching = false;
	if (PunchElapsed >= 0.f)
	{
		bPunching = true;
		PunchElapsed += DeltaTime;
		if (PunchElapsed >= PunchSeconds)
		{
			// One more ApplyPose with the clock stopped puts the arm back where it started.
			PunchElapsed = -1.f;
		}
	}

	if (bBlending || bPunching)
	{
		ApplyPose();
	}
}

FQuat UFirstPersonArmsComponent::FindPoseDelta(ECastleArmsPose Pose, FName BoneName) const
{
	const FVector* Reference = ReferenceDirections.Find(BoneName);
	if (!Reference)
	{
		return FQuat::Identity;
	}

	for (const FCastleArmBonePose& Bone : GetPoseTable(Pose))
	{
		if (Bone.BoneName != BoneName || Bone.Direction.IsNearlyZero())
		{
			continue;
		}

		// The shortest arc from where the limb points in the reference pose to where this pose
		// wants it, plus any roll about the limb's own axis for the wrist.
		const FVector Target = Bone.Direction.GetSafeNormal();
		const FQuat Swing = FQuat::FindBetweenNormals(*Reference, Target);
		const FQuat Twist(Target, FMath::DegreesToRadians(Bone.TwistDegrees));
		return Twist * Swing;
	}

	// A bone one pose does not mention rests at its reference rotation rather than snapping.
	return FQuat::Identity;
}

void UFirstPersonArmsComponent::ApplyPose()
{
	if (!bArmsInitialised || !GetSkinnedAsset())
	{
		return;
	}

	for (const FName& BoneName : PosedBones)
	{
		const FQuat* Reference = ReferenceRotations.Find(BoneName);
		if (!Reference)
		{
			continue;
		}

		const FQuat From = FindPoseDelta(PreviousPose, BoneName);
		const FQuat To = FindPoseDelta(ActivePose, BoneName);
		const FQuat Delta = FQuat::Slerp(From, To, BlendAlpha).GetNormalized();

		SetBoneRotationByName(BoneName, (Delta * (*Reference)).Rotator(), EBoneSpaces::ComponentSpace);
	}

	ApplyFingerCurl();
	ApplyPunchOffset();
}

void UFirstPersonArmsComponent::ApplyFingerCurl()
{
	for (const bool bRight : { true, false })
	{
		const FVector& ReferenceAxis = bRight ? CurlAxisRight : CurlAxisLeft;
		if (ReferenceAxis.IsNearlyZero())
		{
			continue;
		}

		// The hand carries its fingers, so the flex axis rides along with whatever the pose did
		// to the wrist; everything below is then the curl on top of that.
		const FName HandBone(bRight ? HandRight : HandLeft);
		const FQuat HandDelta = FQuat::Slerp(
			FindPoseDelta(PreviousPose, HandBone), FindPoseDelta(ActivePose, HandBone), BlendAlpha).GetNormalized();
		const FVector Axis = HandDelta.RotateVector(ReferenceAxis);

		const float Degrees = FMath::Lerp(
			GetCurlDegrees(PreviousPose, bRight), GetCurlDegrees(ActivePose, bRight), BlendAlpha);

		for (const FCastleFingerJoint& Joint : FingerJoints)
		{
			const FQuat* Reference = Joint.bRightHand == bRight ? ReferenceRotations.Find(Joint.BoneName) : nullptr;
			if (!Reference)
			{
				continue;
			}

			// Each joint carries its parents' curl with it, so the angle accumulates down the
			// chain: one joint's worth at the knuckle, three at the tip.
			const float Angle = Degrees * Joint.Depth * FingerCurlSign * (Joint.bThumb ? ThumbCurlScale : 1.f);
			const FQuat Curl(Axis, FMath::DegreesToRadians(Angle));
			SetBoneRotationByName(
				Joint.BoneName, (Curl * HandDelta * (*Reference)).Rotator(), EBoneSpaces::ComponentSpace);
		}
	}
}

void UFirstPersonArmsComponent::ApplyPunchOffset()
{
	const float Alpha = GetPunchAlpha();
	for (const bool bRight : { true, false })
	{
		const FName ArmRoot(bRight ? ArmRootRight : ArmRootLeft);
		const FVector* Reference = ReferenceLocations.Find(ArmRoot);
		if (!Reference)
		{
			continue;
		}

		// +Y is down the camera in component space, so the whole arm shoots forward from the
		// shoulder. Written every frame, so the arm that is not jabbing is always at rest.
		FVector Location = *Reference;
		if (bRight == bPunchRightHand)
		{
			Location.Y += PunchDistance * Alpha;
		}
		SetBoneLocationByName(ArmRoot, Location, EBoneSpaces::ComponentSpace);
	}
}

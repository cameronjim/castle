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
	// left, +Z is up. Measured from the reference pose, the shoulders sit at z 149.5 and the
	// upper arm is 30 cm long, the forearm 27; these directions put the hands roughly 40 cm in
	// front of the shoulders and a little below them, which is where the camera is looking.
	PoseFists = {
		MakeBonePose(TEXT("upperarm_r"), TEXT("lowerarm_r"), FVector(0.02f, 0.62f, -0.78f)),
		MakeBonePose(TEXT("lowerarm_r"), TEXT("hand_r"), FVector(0.06f, 0.88f, 0.47f)),
		MakeBonePose(TEXT("hand_r"), TEXT("middle_01_r"), FVector(0.06f, 0.88f, 0.47f)),
		MakeBonePose(TEXT("upperarm_l"), TEXT("lowerarm_l"), FVector(-0.02f, 0.62f, -0.78f)),
		MakeBonePose(TEXT("lowerarm_l"), TEXT("hand_l"), FVector(-0.06f, 0.88f, 0.47f)),
		MakeBonePose(TEXT("hand_l"), TEXT("middle_01_l"), FVector(-0.06f, 0.88f, 0.47f))
	};

	PosePistol = {
		MakeBonePose(TEXT("upperarm_r"), TEXT("lowerarm_r"), FVector(0.15f, 0.80f, -0.58f)),
		MakeBonePose(TEXT("lowerarm_r"), TEXT("hand_r"), FVector(0.15f, 0.98f, 0.10f)),
		MakeBonePose(TEXT("hand_r"), TEXT("middle_01_r"), FVector(0.15f, 0.98f, 0.10f)),
		// The left hand comes across the body to meet the right under the grip, so its
		// direction leans towards -X, the character's right.
		MakeBonePose(TEXT("upperarm_l"), TEXT("lowerarm_l"), FVector(-0.10f, 0.72f, -0.68f)),
		MakeBonePose(TEXT("lowerarm_l"), TEXT("hand_l"), FVector(-0.45f, 0.88f, 0.15f)),
		MakeBonePose(TEXT("hand_l"), TEXT("middle_01_l"), FVector(-0.45f, 0.88f, 0.15f))
	};
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
	return Names;
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

	if (BlendAlpha >= 1.f)
	{
		return;
	}

	BlendAlpha = FMath::Clamp(BlendAlpha + DeltaTime / FMath::Max(PoseBlendSeconds, KINDA_SMALL_NUMBER), 0.f, 1.f);
	ApplyPose();
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
}

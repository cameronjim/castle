// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/FirstPersonArmsComponent.h"

namespace
{
	/** FRotator(Pitch, Yaw, Roll), spelled out so the pose tables below read as poses. */
	FCastleArmBonePose MakeBonePose(const TCHAR* BoneName, float Yaw, float Roll)
	{
		FCastleArmBonePose Pose;
		Pose.BoneName = FName(BoneName);
		Pose.Rotation = FRotator(0.f, Yaw, Roll);
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

	// Component space on the mannequin, after the -90 yaw that faces it down the camera:
	// +Y is forward, +X is the character's left, +Z is up. So Roll raises an arm forwards out
	// of the A-pose and Yaw swings it towards the centre line.
	PoseFists = {
		MakeBonePose(TEXT("clavicle_r"), 0.f, 5.f),
		MakeBonePose(TEXT("upperarm_r"), -15.f, 30.f),
		MakeBonePose(TEXT("lowerarm_r"), -25.f, 125.f),
		MakeBonePose(TEXT("hand_r"), -35.f, 125.f),
		MakeBonePose(TEXT("clavicle_l"), 0.f, 5.f),
		MakeBonePose(TEXT("upperarm_l"), 15.f, 30.f),
		MakeBonePose(TEXT("lowerarm_l"), 25.f, 125.f),
		MakeBonePose(TEXT("hand_l"), 35.f, 125.f)
	};

	PosePistol = {
		MakeBonePose(TEXT("clavicle_r"), 0.f, 5.f),
		MakeBonePose(TEXT("upperarm_r"), -12.f, 70.f),
		MakeBonePose(TEXT("lowerarm_r"), -14.f, 100.f),
		MakeBonePose(TEXT("hand_r"), -14.f, 100.f),
		MakeBonePose(TEXT("clavicle_l"), 0.f, 5.f),
		MakeBonePose(TEXT("upperarm_l"), 25.f, 60.f),
		MakeBonePose(TEXT("lowerarm_l"), 45.f, 110.f),
		MakeBonePose(TEXT("hand_l"), 45.f, 105.f)
	};
}

const TArray<FCastleArmBonePose>& UFirstPersonArmsComponent::GetPoseTable(ECastleArmsPose Pose) const
{
	return Pose == ECastleArmsPose::Pistol ? PosePistol : PoseFists;
}

TArray<FName> UFirstPersonArmsComponent::GetAllPoseBoneNames() const
{
	TArray<FName> Names;
	for (const FCastleArmBonePose& Bone : PoseFists)
	{
		Names.AddUnique(Bone.BoneName);
	}
	for (const FCastleArmBonePose& Bone : PosePistol)
	{
		Names.AddUnique(Bone.BoneName);
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
	for (const FName& BoneName : GetAllPoseBoneNames())
	{
		if (GetBoneIndex(BoneName) == INDEX_NONE || HiddenArmBones.Contains(BoneName))
		{
			continue;
		}
		PosedBones.Add(BoneName);
		// Read before anything is posed, so this really is the skeleton's reference A-pose.
		ReferenceRotations.Add(BoneName,
			GetBoneRotationByName(BoneName, EBoneSpaces::ComponentSpace).Quaternion());
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
	for (const FCastleArmBonePose& Bone : GetPoseTable(Pose))
	{
		if (Bone.BoneName == BoneName)
		{
			return Bone.Rotation.Quaternion();
		}
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

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/LocomotionAnim.h"

#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"

bool CastleLocomotion::PlayIfChanged(
	USkeletalMeshComponent* Mesh, UAnimSequence* Wanted, TObjectPtr<UAnimSequence>& Current)
{
	if (!Wanted || Wanted == Current)
	{
		// Re-playing the same sequence every frame would hold it on its first pose forever.
		return false;
	}

	Current = Wanted;

	if (!Mesh || !Mesh->GetSkeletalMeshAsset())
	{
		return true;
	}

	Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	Mesh->PlayAnimation(Wanted, /*bLooping=*/true);
	return true;
}

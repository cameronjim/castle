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

FVector CastleLocomotion::GetMeshFacing(const USkeletalMeshComponent* Mesh)
{
	// Right, not forward: the -90 degree yaw every character mounts the mannequin at turns the
	// component's +Y into the actor's +X.
	return Mesh ? Mesh->GetRightVector() : FVector::ForwardVector;
}

float CastleLocomotion::GetFacingAlongVelocity(const USkeletalMeshComponent* Mesh, const FVector& Velocity)
{
	const FVector Direction = Velocity.GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		return 1.f;
	}
	return FVector::DotProduct(Direction, GetMeshFacing(Mesh).GetSafeNormal2D());
}

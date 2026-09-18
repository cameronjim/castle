// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/TakedownComponent.h"

#include "Castle.h"
#include "Combat/Takedownable.h"
#include "CollisionQueryParams.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

UTakedownComponent::UTakedownComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UTakedownComponent::IsValidTarget(const AActor* Candidate) const
{
	const AActor* Owner = GetOwner();
	if (!Candidate || !Owner || Candidate == Owner)
	{
		return false;
	}

	if (!TargetTag.IsNone() && !Candidate->ActorHasTag(TargetTag))
	{
		return false;
	}

	if (!Candidate->GetClass()->ImplementsInterface(UTakedownable::StaticClass()))
	{
		return false;
	}

	// Dot of the target's forward against the direction from the target to the attacker:
	// negative means the attacker stands behind the target.
	const FVector TargetToAttacker = (Owner->GetActorLocation() - Candidate->GetActorLocation()).GetSafeNormal2D();
	const FVector TargetForward = Candidate->GetActorForwardVector().GetSafeNormal2D();
	const float Dot = FVector::DotProduct(TargetForward, TargetToAttacker);

	// cos(180 - MaxBehindAngle) is the largest dot still counted as "behind".
	const float MaxDot = FMath::Cos(FMath::DegreesToRadians(180.f - MaxBehindAngleDegrees));
	if (Dot > MaxDot)
	{
		return false;
	}

	return ITakedownable::Execute_CanBeTakenDown(const_cast<AActor*>(Candidate), const_cast<AActor*>(Owner));
}

AActor* UTakedownComponent::FindTakedownTarget() const
{
	const AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return nullptr;
	}

	const FVector Start = Owner->GetActorLocation();
	const FVector End = Start + Owner->GetActorForwardVector() * TakedownRange;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CastleTakedown), /*bTraceComplex=*/false, Owner);
	QueryParams.AddIgnoredActor(Owner);

	TArray<FHitResult> Hits;
	World->SweepMultiByChannel(
		Hits, Start, End, FQuat::Identity, TraceChannel, FCollisionShape::MakeSphere(TakedownRadius), QueryParams);

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug)
	{
		DrawDebugCapsule(World, (Start + End) * 0.5f, (End - Start).Size() * 0.5f + TakedownRadius, TakedownRadius,
			FRotationMatrix::MakeFromZ(End - Start).ToQuat(), FColor::Yellow, false, 2.f);
	}
#endif

	AActor* BestTarget = nullptr;
	float BestDistanceSq = TNumericLimits<float>::Max();

	for (const FHitResult& Hit : Hits)
	{
		AActor* Candidate = Hit.GetActor();
		if (!IsValidTarget(Candidate))
		{
			continue;
		}

		const float DistanceSq = FVector::DistSquared(Start, Candidate->GetActorLocation());
		if (DistanceSq <= FMath::Square(TakedownRange) && DistanceSq < BestDistanceSq)
		{
			BestDistanceSq = DistanceSq;
			BestTarget = Candidate;
		}
	}

	return BestTarget;
}

bool UTakedownComponent::TryTakedown()
{
	AActor* Target = FindTakedownTarget();
	if (!Target)
	{
		return false;
	}

	ITakedownable::Execute_OnTakedown(Target, GetOwner());
	OnTakedownPerformed.Broadcast(Target);

	UE_LOG(LogCastle, Verbose, TEXT("Takedown performed on %s."), *Target->GetName());
	return true;
}

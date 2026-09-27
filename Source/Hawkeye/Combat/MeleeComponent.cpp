// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/MeleeComponent.h"

#include "Hawkeye.h"
#include "Combat/HealthComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "World/ThugCharacter.h"

UMeleeComponent::UMeleeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UMeleeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AdvanceAttack(DeltaTime);
}

bool UMeleeComponent::StartAttack(const FHawkeyeMeleeAttack& Attack)
{
	if (Phase != EMeleePhase::Idle)
	{
		return false;
	}

	CurrentAttack = Attack;
	Phase = EMeleePhase::Windup;
	PhaseRemaining = Attack.WindupSeconds;

	UE_LOG(LogHawkeye, Log, TEXT("%s: %s swing winds up (%.2f s, %.0f damage)."),
		*GetNameSafe(GetOwner()), *Attack.Name.ToString(), Attack.WindupSeconds, Attack.Damage);

	OnAttackWindup.Broadcast(Attack.Name, Attack.WindupSeconds);
	ApplyLunge(Attack);

	// A zero wind-up lands on the frame it starts, not a tick later.
	if (PhaseRemaining <= 0.f)
	{
		AdvanceAttack(0.f);
	}
	return true;
}

void UMeleeComponent::CancelAttack()
{
	if (Phase == EMeleePhase::Windup)
	{
		UE_LOG(LogHawkeye, Log, TEXT("%s: %s swing interrupted."), *GetNameSafe(GetOwner()), *CurrentAttack.Name.ToString());
	}
	Phase = EMeleePhase::Idle;
	PhaseRemaining = 0.f;
}

void UMeleeComponent::AdvanceAttack(float DeltaSeconds)
{
	if (Phase == EMeleePhase::Idle)
	{
		return;
	}

	PhaseRemaining -= DeltaSeconds;
	if (Phase == EMeleePhase::Windup && PhaseRemaining <= KINDA_SMALL_NUMBER)
	{
		// Whatever the wind-up overshot comes off the recovery, so a big step lands on time.
		const float Overshoot = -FMath::Min(PhaseRemaining, 0.f);
		Phase = EMeleePhase::Recover;
		PhaseRemaining = CurrentAttack.RecoverSeconds - Overshoot;
		Strike();
	}
	if (Phase == EMeleePhase::Recover && PhaseRemaining <= KINDA_SMALL_NUMBER)
	{
		Phase = EMeleePhase::Idle;
		PhaseRemaining = 0.f;
	}
}

AActor* UMeleeComponent::FindTarget(const FHawkeyeMeleeAttack& Attack) const
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return nullptr;
	}

	const FVector Start = Owner->GetActorLocation();
	const FVector End = Start + Owner->GetActorForwardVector().GetSafeNormal2D() * Attack.Range;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(HawkeyeMelee), /*bTraceComplex=*/false, Owner);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Pawn);

	TArray<FHitResult> Hits;
	World->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity, Objects,
		FCollisionShape::MakeSphere(Attack.Radius), Params);

	for (const FHitResult& Hit : Hits)
	{
		AActor* Candidate = Hit.GetActor();
		if (!Candidate || Candidate == Owner || (!IgnoreTag.IsNone() && Candidate->ActorHasTag(IgnoreTag)))
		{
			continue;
		}
		const UHealthComponent* Health = Candidate->FindComponentByClass<UHealthComponent>();
		if (Health && Health->IsAlive())
		{
			return Candidate;
		}
	}
	return nullptr;
}

void UMeleeComponent::Strike()
{
	AActor* Owner = GetOwner();
	AActor* Target = FindTarget(CurrentAttack);
	UHealthComponent* Health = Target ? Target->FindComponentByClass<UHealthComponent>() : nullptr;
	if (!Health)
	{
		UE_LOG(LogHawkeye, Verbose, TEXT("%s: %s swing hit nothing."), *GetNameSafe(Owner), *CurrentAttack.Name.ToString());
		return;
	}

	// A knockdown replaces the stagger: the body going over is the reaction. The heavy's shield takes a
	// light strike from the front outright; a heavy gets through it (and breaks his guard instead).
	AThugCharacter* Thug = Cast<AThugCharacter>(Target);
	if (Thug && Owner && !CurrentAttack.bKnockdown
		&& Thug->TryBlock(Owner, Owner->GetActorLocation() - Thug->GetActorLocation(), CurrentAttack.Name.ToString()))
	{
		++BlockedCount;
		return;
	}
	const bool bKnockdown = CurrentAttack.bKnockdown && Thug;
	const float Dealt = Health->ApplyMeleeDamage(CurrentAttack.Damage, Owner, CurrentAttack.bStagger && !bKnockdown);
	if (Dealt <= 0.f)
	{
		UE_LOG(LogHawkeye, Log, TEXT("%s: %s swing on %s did nothing (invulnerable or dead)."),
			*GetNameSafe(Owner), *CurrentAttack.Name.ToString(), *GetNameSafe(Target));
		return;
	}

	if (bKnockdown && Health->IsAlive())
	{
		Thug->Knockdown(Owner);
	}
	if (CurrentAttack.KnockbackDistance > 0.f && Health->IsAlive())
	{
		ApplyKnockback(Target, CurrentAttack);
	}

	UE_LOG(LogHawkeye, Log, TEXT("%s: %s swing hit %s for %.1f (health %.1f)."),
		*GetNameSafe(Owner), *CurrentAttack.Name.ToString(), *GetNameSafe(Target), Dealt, Health->GetCurrentHealth());
	OnAttackLanded.Broadcast(Target, Dealt, CurrentAttack.Name);
}

void UMeleeComponent::ApplyLunge(const FHawkeyeMeleeAttack& Attack) const
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Movement || Attack.LungeDistance <= 0.f || !Movement->IsMovingOnGround() || !GetWorld())
	{
		return;
	}

	// A root motion force rather than a launch: it overrides the gait speed the sample's graph
	// writes, and ends exactly LungeSeconds later without sliding on.
	TSharedPtr<FRootMotionSource_ConstantForce> Lunge = MakeShared<FRootMotionSource_ConstantForce>();
	Lunge->InstanceName = FName(TEXT("MeleeLunge"));
	Lunge->AccumulateMode = ERootMotionAccumulateMode::Override;
	Lunge->Priority = 4;
	Lunge->Force = Character->GetActorForwardVector().GetSafeNormal2D() * (Attack.LungeDistance / Attack.LungeSeconds);
	Lunge->Duration = Attack.LungeSeconds;
	Lunge->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::SetVelocity;
	Lunge->FinishVelocityParams.SetVelocity = FVector::ZeroVector;
	Movement->ApplyRootMotionSource(Lunge);
}

FVector UMeleeComponent::ComputeKnockbackForce(const FVector& From, const FVector& To, float Distance, float Seconds)
{
	FVector Away = (To - From).GetSafeNormal2D();
	if (Away.IsNearlyZero())
	{
		Away = FVector::ForwardVector;
	}
	return Away * (FMath::Max(Distance, 0.f) / FMath::Max(Seconds, 0.01f));
}

void UMeleeComponent::ApplyKnockback(AActor* Target, const FHawkeyeMeleeAttack& Attack) const
{
	const AActor* Owner = GetOwner();
	ACharacter* Character = Cast<ACharacter>(Target);
	UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Owner || !Movement || Attack.KnockbackDistance <= 0.f)
	{
		return;
	}
	// A root motion force, like the lunge and the dodge: it beats whatever gait the target's graph asks
	// for, covers exactly the distance, and stops dead at the end.
	TSharedPtr<FRootMotionSource_ConstantForce> Shove = MakeShared<FRootMotionSource_ConstantForce>();
	Shove->InstanceName = FName(TEXT("MeleeKnockback"));
	Shove->AccumulateMode = ERootMotionAccumulateMode::Override;
	Shove->Priority = 6;
	Shove->Force = ComputeKnockbackForce(Owner->GetActorLocation(), Target->GetActorLocation(), Attack.KnockbackDistance,
		Attack.KnockbackSeconds);
	Shove->Duration = Attack.KnockbackSeconds;
	Shove->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::SetVelocity;
	Shove->FinishVelocityParams.SetVelocity = FVector::ZeroVector;
	Movement->ApplyRootMotionSource(Shove);
	UE_LOG(LogHawkeye, Log, TEXT("%s: %s knocks %s back %.0f cm."), *GetNameSafe(Owner), *Attack.Name.ToString(),
		*GetNameSafe(Target), Attack.KnockbackDistance);
}

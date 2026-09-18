// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/BossPhaseComponent.h"

#include "Castle.h"
#include "Combat/HealthComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"

UBossPhaseComponent::UBossPhaseComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UBossPhaseComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	HealthComponent = Owner ? Owner->FindComponentByClass<UHealthComponent>() : nullptr;

	if (!HealthComponent)
	{
		UE_LOG(LogCastle, Error, TEXT("%s has a UBossPhaseComponent but no UHealthComponent; phases are disabled."),
			Owner ? *Owner->GetName() : TEXT("<no owner>"));
		return;
	}

	HealthComponent->OnHealthChanged.AddDynamic(this, &UBossPhaseComponent::HandleHealthChanged);

	if (Phases.Num() > 0)
	{
		EnterPhase(0);
	}
}

void UBossPhaseComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HealthComponent)
	{
		HealthComponent->OnHealthChanged.RemoveDynamic(this, &UBossPhaseComponent::HandleHealthChanged);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TransitionTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

FName UBossPhaseComponent::GetCurrentBehaviorTag() const
{
	return Phases.IsValidIndex(CurrentPhaseIndex) ? Phases[CurrentPhaseIndex].BehaviorTag : NAME_None;
}

void UBossPhaseComponent::HandleHealthChanged(UHealthComponent* /*InHealthComponent*/, float /*NewHealth*/, float Delta, AActor* /*DamageInstigator*/)
{
	if (Delta >= 0.f || bTransitioning || !HealthComponent)
	{
		return;
	}

	const float HealthPercent = HealthComponent->GetHealthPercent();

	// Skip ahead through every threshold a single big hit crossed.
	int32 TargetPhase = CurrentPhaseIndex;
	while (Phases.IsValidIndex(TargetPhase + 1) && HealthPercent <= Phases[TargetPhase + 1].HealthThresholdPercent)
	{
		++TargetPhase;
	}

	if (TargetPhase != CurrentPhaseIndex)
	{
		EnterPhase(TargetPhase);
	}
}

void UBossPhaseComponent::EnterPhase(int32 PhaseIndex)
{
	if (!Phases.IsValidIndex(PhaseIndex) || PhaseIndex == CurrentPhaseIndex)
	{
		return;
	}

	const int32 OldPhaseIndex = CurrentPhaseIndex;
	CurrentPhaseIndex = PhaseIndex;

	const FBossPhase& Phase = Phases[PhaseIndex];

	UE_LOG(LogCastle, Log, TEXT("Boss %s entering phase %d ('%s', behaviour '%s')."),
		GetOwner() ? *GetOwner()->GetName() : TEXT("<no owner>"),
		PhaseIndex, *Phase.PhaseName.ToString(), *Phase.BehaviorTag.ToString());

	// Broadcast first so the behaviour tree can swap before the transition window elapses.
	OnPhaseChanged.Broadcast(OldPhaseIndex, PhaseIndex, Phase);

	UWorld* World = GetWorld();
	if (Phase.TransitionSeconds > 0.f && World)
	{
		bTransitioning = true;
		if (Phase.bInvulnerableDuringTransition && HealthComponent)
		{
			HealthComponent->SetInvulnerable(true);
		}

		World->GetTimerManager().SetTimer(
			TransitionTimerHandle, this, &UBossPhaseComponent::FinishTransition, Phase.TransitionSeconds, false);
	}
	else
	{
		FinishTransition();
	}
}

void UBossPhaseComponent::FinishTransition()
{
	bTransitioning = false;

	if (HealthComponent)
	{
		HealthComponent->SetInvulnerable(false);
	}

	OnPhaseTransitionFinished.Broadcast(CurrentPhaseIndex);
}

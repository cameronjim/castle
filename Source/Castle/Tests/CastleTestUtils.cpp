// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/CastleTestUtils.h"

#include "Combat/HealthComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

FCastleTestWorld::FCastleTestWorld()
{
	World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false);
	if (World && GEngine)
	{
		FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
		Context.SetCurrentWorld(World);
	}
}

FCastleTestWorld::~FCastleTestWorld()
{
	if (!World)
	{
		return;
	}

	if (GEngine)
	{
		GEngine->DestroyWorldContext(World);
	}

	World->DestroyWorld(/*bInformEngineOfWorld=*/false);
	World = nullptr;
}

AActor* FCastleTestWorld::SpawnActor(TSubclassOf<AActor> ActorClass, const FVector& Location, const FRotator& Rotation) const
{
	if (!World || !ActorClass)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<AActor>(ActorClass, Location, Rotation, SpawnParams);
}

void UCastleTestListener::HandleHealthChanged(UHealthComponent* /*HealthComponent*/, float NewHealth, float Delta, AActor* /*DamageInstigator*/)
{
	++HealthChangedCount;
	LastNewHealth = NewHealth;
	LastHealthDelta = Delta;
}

void UCastleTestListener::HandleDeath(UHealthComponent* /*HealthComponent*/, AActor* /*Killer*/)
{
	++DeathCount;
}

void UCastleTestListener::HandlePhaseChanged(int32 OldPhaseIndex, int32 NewPhaseIndex, FBossPhase /*Phase*/)
{
	++PhaseChangedCount;
	LastOldPhaseIndex = OldPhaseIndex;
	LastNewPhaseIndex = NewPhaseIndex;

	if (WatchedHealth)
	{
		bWatchedHealthInvulnerableAtPhaseChange = WatchedHealth->IsInvulnerable();
	}
}

void UCastleTestListener::HandleTransitionFinished(int32 /*PhaseIndex*/)
{
	++TransitionFinishedCount;
}

void UCastleTestListener::HandleAmmoChanged(int32 CurrentAmmo, int32 ReserveAmmo)
{
	++AmmoChangedCount;
	LastMagazine = CurrentAmmo;
	LastReserve = ReserveAmmo;
}

void UCastleTestListener::HandleEmptyClick()
{
	++EmptyClickCount;
}

void UCastleTestListener::HandleTakedownPerformed(AActor* Target)
{
	++TakedownCount;
	LastTakedownTarget = Target;
}

void UCastleTestListener::HandleObjectiveUpdated(UMissionObjective* Objective, int32 ObjectiveIndex)
{
	++ObjectiveUpdatedCount;
	LastObjective = Objective;
	LastObjectiveIndex = ObjectiveIndex;
}

void UCastleTestListener::HandleMissionComplete(UMissionDefinition* /*Mission*/)
{
	++MissionCompleteCount;
}

void UCastleTestListener::HandleFlashbackRequested(UFlashbackDefinition* /*Flashback*/)
{
	++FlashbackRequestedCount;
	bFlashbackFollowedMissionComplete = MissionCompleteCount == 1;
}

void UCastleTestListener::HandleFlashbackFinished(UFlashbackDefinition* /*Flashback*/)
{
	++FlashbackFinishedCount;
}

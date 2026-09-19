// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/CastleTestUtils.h"

#include "Combat/HealthComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "Mission/MissionTracker.h"

FCastleTestWorld::FCastleTestWorld()
{
	World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false);
	if (World && GEngine)
	{
		FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
		Context.SetCurrentWorld(World);

		// APawn::ShouldTakeDamage refuses every hit in a world with no authority game mode, so a
		// test world without one silently swallows all weapon damage. A bare AGameModeBase is
		// enough, and deliberately not the project's: tests never load Content.
		World->SetGameInstance(NewObject<UGameInstance>(GEngine));
		if (AWorldSettings* Settings = World->GetWorldSettings())
		{
			Settings->DefaultGameMode = AGameModeBase::StaticClass();
		}
		World->SetGameMode(FURL());

		// Without this the world never marks its actors initialized, and AActor::PostActorConstruction
		// then skips PostInitializeComponents and BeginPlay entirely - so anything an actor wires up
		// there (a guard binding OnDeath, for one) is silently missing in tests but present in game.
		World->InitializeActorsForPlay(FURL());
		World->SetBegunPlay(true);
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

void UCastleTestListener::HandleDeath(UHealthComponent* /*HealthComponent*/, AActor* Killer)
{
	++DeathCount;
	LastKiller = Killer;
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

void UCastleTestListener::HandleWeaponHit(AActor* HitActor, float DamageDealt)
{
	++WeaponHitCount;
	LastWeaponHitActor = HitActor;
	LastWeaponHitDamage = DamageDealt;
}

void UCastleTestListener::HandleTakedownPerformed(AActor* Target)
{
	++TakedownCount;
	LastTakedownTarget = Target;
}

void UCastleTestListener::HandleAlertStateChanged(EGuardAlertState OldState, EGuardAlertState NewState)
{
	++AlertStateChangedCount;
	LastOldAlertState = OldState;
	LastNewAlertState = NewState;
}

void UCastleTestListener::HandleMissionStarted(UMissionDefinition* Mission)
{
	++MissionStartedCount;
	LastStartedMission = Mission;

	if (WatchedTracker)
	{
		bCurrentObjectiveSetAtMissionStart = WatchedTracker->GetCurrentObjective() != nullptr;
	}
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

void ACastleAimTestCharacter::TestSetSprinting(bool bInSprinting)
{
	bIsSprinting = bInSprinting;
	if (bInSprinting)
	{
		StopAim();
	}
	UpdateMaxWalkSpeed();
}

float ACastleAimTestCharacter::MaxWalkSpeed() const
{
	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	return Movement ? Movement->MaxWalkSpeed : 0.f;
}

void UCastleTestListener::HandleSettingsChanged(FCastleSettings Settings)
{
	++SettingsChangedCount;
	LastLookSensitivity = Settings.LookSensitivity;
}

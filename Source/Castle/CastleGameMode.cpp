// Copyright Epic Games, Inc. All Rights Reserved.

#include "CastleGameMode.h"

#include "Castle.h"
#include "CastlePlayerController.h"
#include "Combat/WeaponDefinition.h"
#include "GameFramework/Pawn.h"
#include "Mission/MissionDefinition.h"
#include "Player/InventoryComponent.h"
#include "NavigationSystem.h"
#include "Mission/MissionSubsystem.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Player/CastleCharacter.h"
#include "TimerManager.h"

ACastleGameMode::ACastleGameMode()
{
	DefaultPawnClass = ACastleCharacter::StaticClass();
	PlayerControllerClass = ACastlePlayerController::StaticClass();
}

void ACastleGameMode::BeginPlay()
{
	Super::BeginPlay();

	BuildNavigationIfEmpty();

	UMissionSubsystem* MissionSubsystem = UMissionSubsystem::Get(this);
	if (!MissionSubsystem)
	{
		return;
	}

	MissionSubsystem->OnMissionComplete.AddDynamic(this, &ACastleGameMode::HandleMissionComplete);
	MissionSubsystem->OnMissionStarted.AddDynamic(this, &ACastleGameMode::HandleMissionStarted);

	if (StartingMission)
	{
		MissionSubsystem->StartMission(StartingMission);
	}
	else
	{
		UE_LOG(LogCastle, Log, TEXT("%s has no StartingMission set."), *GetName());
	}
}

void ACastleGameMode::BuildNavigationIfEmpty()
{
	if (!bBuildNavigationAtStart)
	{
		return;
	}

	UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!NavSystem)
	{
		UE_LOG(LogCastle, Warning,
			TEXT("%s: no navigation system; nothing will patrol in this level."), *GetName());
		return;
	}

	UE_LOG(LogCastle, Log, TEXT("%s: building navigation."), *GetName());
	NavSystem->Build();
}

void ACastleGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RestartTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void ACastleGameMode::RestartMission(float Delay)
{
	if (bRestartPending)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float WaitSeconds = Delay < 0.f ? RestartDelaySeconds : Delay;

	bRestartPending = true;

	// Starting over means starting over: the hotbar goes back to what the mission grants.
	if (UInventoryComponent* Inventory = FindPlayerInventory())
	{
		Inventory->Clear();
	}

	OnMissionRestarting();

	UE_LOG(LogCastle, Log, TEXT("%s: restarting the mission in %.1f s."), *GetName(), WaitSeconds);

	if (WaitSeconds <= 0.f)
	{
		ReopenCurrentLevel();
		return;
	}

	World->GetTimerManager().SetTimer(
		RestartTimerHandle, this, &ACastleGameMode::ReopenCurrentLevel, WaitSeconds, false);
}

void ACastleGameMode::ReopenCurrentLevel()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FName CurrentLevel(*UGameplayStatics::GetCurrentLevelName(World, /*bRemovePrefixString=*/true));
	UGameplayStatics::OpenLevel(World, CurrentLevel);
}

UInventoryComponent* ACastleGameMode::FindPlayerInventory() const
{
	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	return Pawn ? Pawn->FindComponentByClass<UInventoryComponent>() : nullptr;
}

void ACastleGameMode::HandleMissionStarted(UMissionDefinition* Mission)
{
	UInventoryComponent* Inventory = FindPlayerInventory();
	if (!Inventory || !Mission)
	{
		return;
	}

	// Nothing carries between missions: the mission's own data asset says what Frank starts
	// with, and everything else he has to find again.
	TArray<UWeaponDefinition*> Starting;
	for (const TSoftObjectPtr<UWeaponDefinition>& Soft : Mission->StartingWeapons)
	{
		if (UWeaponDefinition* Definition = Soft.LoadSynchronous())
		{
			Starting.Add(Definition);
		}
	}

	Inventory->ApplyStartingWeapons(Starting);
}

void ACastleGameMode::HandleMissionComplete(UMissionDefinition* Mission)
{
	UE_LOG(LogCastle, Log, TEXT("Mission complete: %s"),
		Mission ? *Mission->MissionName.ToString() : TEXT("<none>"));

	// Before the end card, so the last frame of gameplay is not a hotbar the player keeps.
	if (UInventoryComponent* Inventory = FindPlayerInventory())
	{
		Inventory->Clear();
	}

	OnMissionCompleted(Mission);
}

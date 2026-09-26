// Copyright Epic Games, Inc. All Rights Reserved.

#include "HawkeyeGameMode.h"

#include "Hawkeye.h"
#include "HawkeyePlayerController.h"
#include "GameFramework/Pawn.h"
#include "Mission/MissionDefinition.h"
#include "Player/InventoryComponent.h"
#include "NavigationSystem.h"
#include "Mission/MissionSubsystem.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Player/HawkeyeCharacter.h"
#include "TimerManager.h"

AHawkeyeGameMode::AHawkeyeGameMode()
{
	DefaultPawnClass = AHawkeyeCharacter::StaticClass();
	PlayerControllerClass = AHawkeyePlayerController::StaticClass();
}

void AHawkeyeGameMode::BeginPlay()
{
	Super::BeginPlay();

	BuildNavigationIfEmpty();

	UMissionSubsystem* MissionSubsystem = UMissionSubsystem::Get(this);
	if (!MissionSubsystem)
	{
		return;
	}

	MissionSubsystem->OnMissionComplete.AddDynamic(this, &AHawkeyeGameMode::HandleMissionComplete);
	MissionSubsystem->OnMissionStarted.AddDynamic(this, &AHawkeyeGameMode::HandleMissionStarted);

	if (StartingMission)
	{
		MissionSubsystem->StartMission(StartingMission);
	}
	else
	{
		UE_LOG(LogHawkeye, Log, TEXT("%s has no StartingMission set."), *GetName());
	}
}

void AHawkeyeGameMode::BuildNavigationIfEmpty()
{
	if (!bBuildNavigationAtStart)
	{
		return;
	}

	UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!NavSystem)
	{
		UE_LOG(LogHawkeye, Warning,
			TEXT("%s: no navigation system; nothing will patrol in this level."), *GetName());
		return;
	}

	UE_LOG(LogHawkeye, Log, TEXT("%s: building navigation."), *GetName());
	NavSystem->Build();
}

void AHawkeyeGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RestartTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void AHawkeyeGameMode::RestartMission(float Delay)
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

	// Starting over means starting over: the quiver goes back to what the mission grants.
	if (UInventoryComponent* Inventory = FindPlayerInventory())
	{
		Inventory->Clear();
	}

	OnMissionRestarting();

	UE_LOG(LogHawkeye, Log, TEXT("%s: restarting the mission in %.1f s."), *GetName(), WaitSeconds);

	if (WaitSeconds <= 0.f)
	{
		ReopenCurrentLevel();
		return;
	}

	World->GetTimerManager().SetTimer(
		RestartTimerHandle, this, &AHawkeyeGameMode::ReopenCurrentLevel, WaitSeconds, false);
}

void AHawkeyeGameMode::ReopenCurrentLevel()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FName CurrentLevel(*UGameplayStatics::GetCurrentLevelName(World, /*bRemovePrefixString=*/true));
	UGameplayStatics::OpenLevel(World, CurrentLevel);
}

UInventoryComponent* AHawkeyeGameMode::FindPlayerInventory() const
{
	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	return Pawn ? Pawn->FindComponentByClass<UInventoryComponent>() : nullptr;
}

void AHawkeyeGameMode::HandleMissionStarted(UMissionDefinition* Mission)
{
	UInventoryComponent* Inventory = FindPlayerInventory();
	if (!Inventory || !Mission)
	{
		return;
	}

	// Nothing carries between chapters: the chapter's own data asset says which bow and arrows
	// she starts with, and everything else she has to find again.
	Inventory->ApplyMissionStart(Mission);
}

void AHawkeyeGameMode::HandleMissionComplete(UMissionDefinition* Mission)
{
	UE_LOG(LogHawkeye, Log, TEXT("Mission complete: %s"),
		Mission ? *Mission->MissionName.ToString() : TEXT("<none>"));

	// Before the end card, so the last frame of gameplay is not a quiver the player keeps.
	if (UInventoryComponent* Inventory = FindPlayerInventory())
	{
		Inventory->Clear();
	}

	OnMissionCompleted(Mission);
}

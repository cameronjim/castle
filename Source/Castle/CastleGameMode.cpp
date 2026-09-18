// Copyright Epic Games, Inc. All Rights Reserved.

#include "CastleGameMode.h"

#include "Castle.h"
#include "CastlePlayerController.h"
#include "Mission/MissionDefinition.h"
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

	UMissionSubsystem* MissionSubsystem = UMissionSubsystem::Get(this);
	if (!MissionSubsystem)
	{
		return;
	}

	MissionSubsystem->OnMissionComplete.AddDynamic(this, &ACastleGameMode::HandleMissionComplete);

	if (StartingMission)
	{
		MissionSubsystem->StartMission(StartingMission);
	}
	else
	{
		UE_LOG(LogCastle, Log, TEXT("%s has no StartingMission set."), *GetName());
	}
}

void ACastleGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RestartTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void ACastleGameMode::RestartMission()
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

	bRestartPending = true;
	OnMissionRestarting();

	UE_LOG(LogCastle, Log, TEXT("%s: restarting the mission in %.1f s."), *GetName(), RestartDelaySeconds);

	if (RestartDelaySeconds <= 0.f)
	{
		ReopenCurrentLevel();
		return;
	}

	World->GetTimerManager().SetTimer(
		RestartTimerHandle, this, &ACastleGameMode::ReopenCurrentLevel, RestartDelaySeconds, false);
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

void ACastleGameMode::HandleMissionComplete(UMissionDefinition* Mission)
{
	UE_LOG(LogCastle, Log, TEXT("Mission complete: %s"),
		Mission ? *Mission->MissionName.ToString() : TEXT("<none>"));

	OnMissionCompleted(Mission);
}

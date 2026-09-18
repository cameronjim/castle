// Copyright Epic Games, Inc. All Rights Reserved.

#include "CastleGameMode.h"

#include "Castle.h"
#include "CastlePlayerController.h"
#include "Mission/MissionDefinition.h"
#include "Mission/MissionSubsystem.h"
#include "Player/CastleCharacter.h"

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

void ACastleGameMode::HandleMissionComplete(UMissionDefinition* Mission)
{
	UE_LOG(LogCastle, Log, TEXT("Mission complete: %s"),
		Mission ? *Mission->MissionName.ToString() : TEXT("<none>"));

	OnMissionCompleted(Mission);
}

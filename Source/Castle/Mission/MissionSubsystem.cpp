// Copyright Epic Games, Inc. All Rights Reserved.

#include "Mission/MissionSubsystem.h"

#include "Castle.h"
#include "Engine/World.h"
#include "Mission/MissionDefinition.h"
#include "Mission/MissionObjective.h"

UMissionSubsystem* UMissionSubsystem::Get(const UObject* WorldContextObject)
{
	if (const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr)
	{
		return World->GetSubsystem<UMissionSubsystem>();
	}

	return nullptr;
}

void UMissionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Tracker = NewObject<UMissionTracker>(this, TEXT("MissionTracker"));
	Tracker->OnMissionStarted.AddDynamic(this, &UMissionSubsystem::HandleMissionStarted);
	Tracker->OnObjectiveUpdated.AddDynamic(this, &UMissionSubsystem::HandleObjectiveUpdated);
	Tracker->OnMissionComplete.AddDynamic(this, &UMissionSubsystem::HandleMissionComplete);
	Tracker->OnFlashbackRequested.AddDynamic(this, &UMissionSubsystem::HandleFlashbackRequested);
}

void UMissionSubsystem::Deinitialize()
{
	OnMissionStarted.Clear();
	OnObjectiveUpdated.Clear();
	OnMissionComplete.Clear();
	OnFlashbackRequested.Clear();

	if (Tracker)
	{
		Tracker->Reset();
		Tracker = nullptr;
	}

	Super::Deinitialize();
}

void UMissionSubsystem::HandleMissionStarted(UMissionDefinition* Mission)
{
	OnMissionStarted.Broadcast(Mission);
}

void UMissionSubsystem::HandleObjectiveUpdated(UMissionObjective* Objective, int32 ObjectiveIndex)
{
	OnObjectiveUpdated.Broadcast(Objective, ObjectiveIndex);
}

void UMissionSubsystem::HandleMissionComplete(UMissionDefinition* Mission)
{
	OnMissionComplete.Broadcast(Mission);
}

void UMissionSubsystem::HandleFlashbackRequested(UFlashbackDefinition* Flashback)
{
	OnFlashbackRequested.Broadcast(Flashback);
}

bool UMissionSubsystem::StartMission(UMissionDefinition* MissionDefinition)
{
	return Tracker ? Tracker->StartMission(MissionDefinition) : false;
}

bool UMissionSubsystem::CompleteObjective(FName ObjectiveId)
{
	return Tracker ? Tracker->CompleteObjective(ObjectiveId) : false;
}

void UMissionSubsystem::AbortMission()
{
	if (Tracker)
	{
		Tracker->AbortMission();
	}
}

UMissionDefinition* UMissionSubsystem::GetCurrentMission() const
{
	return Tracker ? Tracker->GetCurrentMission() : nullptr;
}

TArray<UMissionObjective*> UMissionSubsystem::GetActiveObjectives() const
{
	return Tracker ? Tracker->GetActiveObjectives() : TArray<UMissionObjective*>();
}

UMissionObjective* UMissionSubsystem::GetCurrentObjective() const
{
	return Tracker ? Tracker->GetCurrentObjective() : nullptr;
}

bool UMissionSubsystem::IsMissionComplete() const
{
	return Tracker && Tracker->IsMissionComplete();
}

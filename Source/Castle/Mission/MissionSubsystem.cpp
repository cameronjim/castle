// Copyright Epic Games, Inc. All Rights Reserved.

#include "Mission/MissionSubsystem.h"

#include "Castle.h"
#include "Engine/World.h"
#include "Flashback/FlashbackDefinition.h"
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

void UMissionSubsystem::Deinitialize()
{
	OnObjectiveUpdated.Clear();
	OnMissionComplete.Clear();
	OnFlashbackRequested.Clear();

	ActiveObjectives.Reset();
	CurrentMission = nullptr;
	bMissionComplete = false;

	Super::Deinitialize();
}

void UMissionSubsystem::StartMission(UMissionDefinition* MissionDefinition)
{
	if (!MissionDefinition)
	{
		UE_LOG(LogCastle, Warning, TEXT("StartMission called with a null MissionDefinition."));
		return;
	}

	CurrentMission = MissionDefinition;
	bMissionComplete = false;
	ActiveObjectives.Reset();

	for (const TObjectPtr<UMissionObjective>& SourceObjective : MissionDefinition->Objectives)
	{
		if (!SourceObjective)
		{
			continue;
		}

		// Duplicate so runtime completion never dirties the data asset.
		UMissionObjective* RuntimeObjective = DuplicateObject<UMissionObjective>(SourceObjective, this);
		RuntimeObjective->ResetObjective();
		ActiveObjectives.Add(RuntimeObjective);
	}

	UE_LOG(LogCastle, Log, TEXT("Started mission %d '%s' with %d objective(s)."),
		MissionDefinition->MissionNumber, *MissionDefinition->MissionName.ToString(), ActiveObjectives.Num());

	// Let the HUD draw the initial list.
	for (int32 Index = 0; Index < ActiveObjectives.Num(); ++Index)
	{
		OnObjectiveUpdated.Broadcast(ActiveObjectives[Index], Index);
	}
}

UMissionObjective* UMissionSubsystem::GetObjectiveAt(int32 ObjectiveIndex) const
{
	return ActiveObjectives.IsValidIndex(ObjectiveIndex) ? ActiveObjectives[ObjectiveIndex] : nullptr;
}

int32 UMissionSubsystem::FindObjectiveIndexByTag(FName ObjectiveTag) const
{
	if (ObjectiveTag.IsNone())
	{
		return INDEX_NONE;
	}

	for (int32 Index = 0; Index < ActiveObjectives.Num(); ++Index)
	{
		const UMissionObjective* Objective = ActiveObjectives[Index];
		if (Objective && Objective->ObjectiveTag == ObjectiveTag && !Objective->bCompleted)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

bool UMissionSubsystem::CompleteObjective(int32 ObjectiveIndex)
{
	if (!ActiveObjectives.IsValidIndex(ObjectiveIndex))
	{
		return false;
	}

	UMissionObjective* Objective = ActiveObjectives[ObjectiveIndex];
	if (!Objective || !Objective->Complete())
	{
		return false;
	}

	HandleObjectiveChanged(ObjectiveIndex);
	return true;
}

bool UMissionSubsystem::CompleteObjectiveByTag(FName ObjectiveTag)
{
	const int32 Index = FindObjectiveIndexByTag(ObjectiveTag);
	return Index != INDEX_NONE && CompleteObjective(Index);
}

void UMissionSubsystem::AbortMission()
{
	CurrentMission = nullptr;
	ActiveObjectives.Reset();
	bMissionComplete = false;
}

bool UMissionSubsystem::AreRequiredObjectivesComplete() const
{
	if (ActiveObjectives.Num() == 0)
	{
		return false;
	}

	for (const TObjectPtr<UMissionObjective>& Objective : ActiveObjectives)
	{
		if (Objective && !Objective->bOptional && !Objective->bCompleted)
		{
			return false;
		}
	}

	return true;
}

void UMissionSubsystem::HandleObjectiveChanged(int32 ObjectiveIndex)
{
	OnObjectiveUpdated.Broadcast(GetObjectiveAt(ObjectiveIndex), ObjectiveIndex);

	if (bMissionComplete || !AreRequiredObjectivesComplete())
	{
		return;
	}

	bMissionComplete = true;
	OnMissionComplete.Broadcast(CurrentMission);

	if (CurrentMission && !CurrentMission->FlashbackToPlay.IsNull())
	{
		// Flashbacks are small (a handful of textures) so a synchronous load is acceptable here;
		// swap for FStreamableManager::RequestAsyncLoad if a flashback ever grows heavy.
		if (UFlashbackDefinition* Flashback = CurrentMission->FlashbackToPlay.LoadSynchronous())
		{
			OnFlashbackRequested.Broadcast(Flashback);
		}
	}
}

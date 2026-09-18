// Copyright Epic Games, Inc. All Rights Reserved.

#include "Mission/MissionTracker.h"

#include "Castle.h"
#include "Flashback/FlashbackDefinition.h"
#include "Mission/MissionDefinition.h"
#include "Mission/MissionObjective.h"

bool UMissionTracker::StartMission(UMissionDefinition* MissionDefinition)
{
	if (!MissionDefinition)
	{
		UE_LOG(LogCastle, Warning, TEXT("%s: StartMission called with a null MissionDefinition."), *GetNameSafe(this));
		return false;
	}

	int32 RequiredCount = 0;
	for (const TObjectPtr<UMissionObjective>& SourceObjective : MissionDefinition->Objectives)
	{
		if (SourceObjective && !SourceObjective->bOptional)
		{
			++RequiredCount;
		}
	}

	if (RequiredCount == 0)
	{
		UE_LOG(LogCastle, Error,
			TEXT("%s: mission '%s' has no non-optional objectives; refusing to start it."),
			*GetNameSafe(this), *MissionDefinition->GetName());
		return false;
	}

	if (CurrentMission)
	{
		UE_LOG(LogCastle, Warning,
			TEXT("%s: StartMission('%s') while '%s' is active; the old mission is ended silently."),
			*GetNameSafe(this), *MissionDefinition->GetName(), *CurrentMission->GetName());
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

	// After the objectives exist, so a listener can read GetCurrentObjective() straight away.
	OnMissionStarted.Broadcast(CurrentMission);

	return true;
}

bool UMissionTracker::CompleteObjective(FName ObjectiveId)
{
	const int32 Index = FindObjectiveIndex(ObjectiveId);
	if (Index == INDEX_NONE)
	{
		UE_LOG(LogCastle, Warning, TEXT("%s: unknown objective id '%s'."),
			*GetNameSafe(this), *ObjectiveId.ToString());
		return false;
	}

	UMissionObjective* Objective = ActiveObjectives[Index];
	if (Objective->IsCompleted())
	{
		// Duplicate completion is a silent no-op: trigger volumes fire more than once.
		return false;
	}

	if (CurrentMission && CurrentMission->bEnforceOrder && !Objective->bOptional && Objective != GetCurrentObjective())
	{
		UE_LOG(LogCastle, Warning,
			TEXT("%s: mission '%s' enforces order; '%s' cannot be completed before '%s'."),
			*GetNameSafe(this), *CurrentMission->GetName(), *ObjectiveId.ToString(),
			GetCurrentObjective() ? *GetCurrentObjective()->ObjectiveId.ToString() : TEXT("<none>"));
		return false;
	}

	if (!Objective->Complete())
	{
		return false;
	}

	OnObjectiveUpdated.Broadcast(Objective, Index);
	BroadcastCompletionIfFinished();

	return true;
}

void UMissionTracker::BroadcastCompletionIfFinished()
{
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

void UMissionTracker::AbortMission()
{
	CurrentMission = nullptr;
	ActiveObjectives.Reset();
	bMissionComplete = false;
}

UMissionObjective* UMissionTracker::GetCurrentObjective() const
{
	for (const TObjectPtr<UMissionObjective>& Objective : ActiveObjectives)
	{
		if (Objective && !Objective->bOptional && !Objective->bCompleted)
		{
			return Objective;
		}
	}

	return nullptr;
}

int32 UMissionTracker::FindObjectiveIndex(FName ObjectiveId) const
{
	if (ObjectiveId.IsNone())
	{
		return INDEX_NONE;
	}

	for (int32 Index = 0; Index < ActiveObjectives.Num(); ++Index)
	{
		const UMissionObjective* Objective = ActiveObjectives[Index];
		if (Objective && Objective->ObjectiveId == ObjectiveId)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

UMissionObjective* UMissionTracker::FindObjective(FName ObjectiveId) const
{
	return GetObjectiveAt(FindObjectiveIndex(ObjectiveId));
}

TArray<UMissionObjective*> UMissionTracker::GetActiveObjectives() const
{
	TArray<UMissionObjective*> Result;
	Result.Reserve(ActiveObjectives.Num());
	for (const TObjectPtr<UMissionObjective>& Objective : ActiveObjectives)
	{
		Result.Add(Objective);
	}
	return Result;
}

UMissionObjective* UMissionTracker::GetObjectiveAt(int32 ObjectiveIndex) const
{
	return ActiveObjectives.IsValidIndex(ObjectiveIndex) ? ActiveObjectives[ObjectiveIndex] : nullptr;
}

bool UMissionTracker::AreRequiredObjectivesComplete() const
{
	if (ActiveObjectives.Num() == 0)
	{
		return false;
	}

	return GetCurrentObjective() == nullptr;
}

void UMissionTracker::Reset()
{
	OnMissionStarted.Clear();
	OnObjectiveUpdated.Clear();
	OnMissionComplete.Clear();
	OnFlashbackRequested.Clear();

	AbortMission();
}

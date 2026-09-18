// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MissionSubsystem.generated.h"

class UMissionDefinition;
class UMissionObjective;
class UFlashbackDefinition;

/** Fired whenever an objective changes state. Index is the objective's slot in the active list. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnObjectiveUpdatedSignature, UMissionObjective*, Objective, int32, ObjectiveIndex);

/** Fired once every required objective of the active mission is complete. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMissionCompleteSignature, UMissionDefinition*, Mission);

/** Fired after mission completion when the definition supplies a flashback to play. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFlashbackRequestedSignature, UFlashbackDefinition*, Flashback);

/**
 * Per-world owner of the active mission and its objective state.
 * Get it from Blueprints with "Get World Subsystem" -> MissionSubsystem.
 */
UCLASS()
class CASTLE_API UMissionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Convenience accessor; returns nullptr outside of a valid game world. */
	UFUNCTION(BlueprintPure, Category = "Mission", meta = (WorldContext = "WorldContextObject"))
	static UMissionSubsystem* Get(const UObject* WorldContextObject);

	/** Instances the definition's objectives and begins tracking them. */
	UFUNCTION(BlueprintCallable, Category = "Mission")
	void StartMission(UMissionDefinition* MissionDefinition);

	/** Completes the objective at the given index. Returns false if already complete or out of range. */
	UFUNCTION(BlueprintCallable, Category = "Mission")
	bool CompleteObjective(int32 ObjectiveIndex);

	/** Completes the first incomplete objective whose ObjectiveTag matches. */
	UFUNCTION(BlueprintCallable, Category = "Mission")
	bool CompleteObjectiveByTag(FName ObjectiveTag);

	/** Clears the active mission without broadcasting completion. */
	UFUNCTION(BlueprintCallable, Category = "Mission")
	void AbortMission();

	UFUNCTION(BlueprintPure, Category = "Mission")
	UMissionDefinition* GetCurrentMission() const { return CurrentMission; }

	UFUNCTION(BlueprintPure, Category = "Mission")
	TArray<UMissionObjective*> GetActiveObjectives() const;

	UFUNCTION(BlueprintPure, Category = "Mission")
	UMissionObjective* GetObjectiveAt(int32 ObjectiveIndex) const;

	UFUNCTION(BlueprintPure, Category = "Mission")
	int32 FindObjectiveIndexByTag(FName ObjectiveTag) const;

	/** True once every non-optional objective is complete. */
	UFUNCTION(BlueprintPure, Category = "Mission")
	bool AreRequiredObjectivesComplete() const;

	UFUNCTION(BlueprintPure, Category = "Mission")
	bool IsMissionComplete() const { return bMissionComplete; }

	UPROPERTY(BlueprintAssignable, Category = "Mission")
	FOnObjectiveUpdatedSignature OnObjectiveUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Mission")
	FOnMissionCompleteSignature OnMissionComplete;

	UPROPERTY(BlueprintAssignable, Category = "Mission")
	FOnFlashbackRequestedSignature OnFlashbackRequested;

	//~ Begin USubsystem interface
	virtual void Deinitialize() override;
	//~ End USubsystem interface

protected:
	/** Evaluates completion and broadcasts mission/flashback events when finished. */
	void HandleObjectiveChanged(int32 ObjectiveIndex);

	UPROPERTY(Transient)
	TObjectPtr<UMissionDefinition> CurrentMission = nullptr;

	/** Runtime copies of the definition's objectives (the source asset is never mutated). */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Mission")
	TArray<TObjectPtr<UMissionObjective>> ActiveObjectives;

	bool bMissionComplete = false;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mission/MissionTracker.h"
#include "Subsystems/WorldSubsystem.h"
#include "MissionSubsystem.generated.h"

class UMissionDefinition;
class UMissionObjective;
class UFlashbackDefinition;

/**
 * Per-world owner of the active mission. All of the rules live in UMissionTracker; this is the
 * Blueprint-facing wrapper that gives it a lifetime tied to the world.
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

	/** The object that holds the mission rules. Tests drive this directly. */
	UFUNCTION(BlueprintPure, Category = "Mission")
	UMissionTracker* GetTracker() const { return Tracker; }

	/** Instances the definition's objectives and begins tracking them. */
	UFUNCTION(BlueprintCallable, Category = "Mission")
	bool StartMission(UMissionDefinition* MissionDefinition);

	/** Completes the objective carrying ObjectiveId. */
	UFUNCTION(BlueprintCallable, Category = "Mission")
	bool CompleteObjective(FName ObjectiveId);

	/** Clears the active mission without broadcasting completion. */
	UFUNCTION(BlueprintCallable, Category = "Mission")
	void AbortMission();

	UFUNCTION(BlueprintPure, Category = "Mission")
	UMissionDefinition* GetCurrentMission() const;

	UFUNCTION(BlueprintPure, Category = "Mission")
	TArray<UMissionObjective*> GetActiveObjectives() const;

	/** First incomplete non-optional objective; this is the one the HUD shows. */
	UFUNCTION(BlueprintPure, Category = "Mission")
	UMissionObjective* GetCurrentObjective() const;

	UFUNCTION(BlueprintPure, Category = "Mission")
	bool IsMissionComplete() const;

	UPROPERTY(BlueprintAssignable, Category = "Mission")
	FOnMissionStartedSignature OnMissionStarted;

	UPROPERTY(BlueprintAssignable, Category = "Mission")
	FOnObjectiveUpdatedSignature OnObjectiveUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Mission")
	FOnMissionCompleteSignature OnMissionComplete;

	UPROPERTY(BlueprintAssignable, Category = "Mission")
	FOnFlashbackRequestedSignature OnFlashbackRequested;

	//~ Begin USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem interface

protected:
	UFUNCTION()
	void HandleMissionStarted(UMissionDefinition* Mission);

	UFUNCTION()
	void HandleObjectiveUpdated(UMissionObjective* Objective, int32 ObjectiveIndex);

	UFUNCTION()
	void HandleMissionComplete(UMissionDefinition* Mission);

	UFUNCTION()
	void HandleFlashbackRequested(UFlashbackDefinition* Flashback);

	UPROPERTY(Transient)
	TObjectPtr<UMissionTracker> Tracker = nullptr;
};

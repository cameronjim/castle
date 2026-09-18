// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MissionObjective.generated.h"

class UMissionObjective;

/** Fired when this objective is completed. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnObjectiveCompletedSignature, UMissionObjective*, Objective);

/**
 * A single step of a mission ("Reach the laundry", "Take down the guard").
 * Authored inline on a UMissionDefinition and duplicated per-playthrough by the UMissionSubsystem,
 * so runtime state never dirties the source data asset.
 */
UCLASS(Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class CASTLE_API UMissionObjective : public UObject
{
	GENERATED_BODY()

public:
	/** Stable id used to complete this objective by name. Lowercase snake_case: find_weapon. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	FName ObjectiveId;

	/** Short player-facing line shown in the HUD. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	FText Title;

	/** Longer description shown in the mission log. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective", meta = (MultiLine = "true"))
	FText Description;

	/** Optional objectives do not block mission completion. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	bool bOptional = false;

	/** Runtime completion state. */
	UPROPERTY(BlueprintReadOnly, Category = "Objective")
	bool bCompleted = false;

	/** Broadcast once, the first time this objective completes. */
	UPROPERTY(BlueprintAssignable, Category = "Objective")
	FOnObjectiveCompletedSignature OnObjectiveCompleted;

	/** Marks the objective complete and broadcasts. Returns false if it was already complete. */
	UFUNCTION(BlueprintCallable, Category = "Objective")
	bool Complete();

	/** Clears completion state (used when a mission is restarted). */
	UFUNCTION(BlueprintCallable, Category = "Objective")
	void ResetObjective();

	UFUNCTION(BlueprintPure, Category = "Objective")
	bool IsCompleted() const { return bCompleted; }

	//~ Begin UObject interface
	virtual UWorld* GetWorld() const override;
	//~ End UObject interface
};

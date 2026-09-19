// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MissionDefinition.generated.h"

class UMissionObjective;
class UFlashbackDefinition;
class UWeaponDefinition;
class UWorld;

/**
 * Designer-authored description of one mission of the campaign.
 * Create via Content Browser > Miscellaneous > Data Asset > MissionDefinition.
 */
UCLASS(BlueprintType)
class CASTLE_API UMissionDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Player-facing mission name ("Cell Block D"). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission")
	FText MissionName;

	/** Ordering within the campaign; also used for save slots / debug menus. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission")
	int32 MissionNumber = 0;

	/** Ordered objectives. Instanced so each mission owns its own objective objects. */
	UPROPERTY(EditDefaultsOnly, Instanced, BlueprintReadOnly, Category = "Mission")
	TArray<TObjectPtr<UMissionObjective>> Objectives;

	/**
	 * When true, non-optional objectives must be completed in array order: completing one out of
	 * turn logs a warning and does nothing. Optional objectives are never ordered.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission")
	bool bEnforceOrder = false;

	/** When false the HUD hides the objective line for this mission (a silent opening, a boss room). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission")
	bool bShowObjectiveText = true;

	/**
	 * One line of Frank's, shown under the mission name on the end card. Optional: an empty
	 * line leaves the card as just the mission name.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission", meta = (MultiLine = "true"))
	FText EndCardLine;

	/**
	 * Weapons Frank starts this mission carrying, on top of his fists. Empty (the default) is
	 * Hands only, which is how the escape begins. Nothing carries between missions.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission")
	TArray<TSoftObjectPtr<UWeaponDefinition>> StartingWeapons;

	/** Flashback slideshow played when this mission completes. Optional. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission")
	TSoftObjectPtr<UFlashbackDefinition> FlashbackToPlay;

	/** Level opened once the mission (and its flashback) is done. Optional. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission")
	TSoftObjectPtr<UWorld> NextLevel;

	//~ Begin UPrimaryDataAsset interface
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	//~ End UPrimaryDataAsset interface
};

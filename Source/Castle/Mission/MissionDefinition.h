// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MissionDefinition.generated.h"

class UMissionObjective;
class UFlashbackDefinition;
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

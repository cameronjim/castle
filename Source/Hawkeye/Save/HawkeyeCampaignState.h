// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ISpudObject.h"
#include "HawkeyeCampaignState.generated.h"

/** What Continue does with the save it finds. Anything but Load starts a new game instead. */
UENUM(BlueprintType)
enum class EHawkeyeLoadDecision : uint8
{
	/** The save is current and its chapter exists: load it. */
	Load,
	/** There is no save in the slot. */
	NoSave,
	/** Written by another version with no migration to this one. */
	VersionMismatch,
	/** Its chapter's mission asset is gone (renamed, deleted). */
	StaleMission
};

/**
 * The campaign-wide half of the save: which chapter, which objectives are done, which safehouses
 * are found, who the player was playing. Everything that belongs to an actor (health, quiver,
 * position, a dead thug) is saved by SPUD on the actor itself; this is the part with no actor.
 *
 * UHawkeyeSaveSubsystem owns one and registers it with SPUD as the global object
 * "HawkeyeCampaign", so SPUD writes its SaveGame properties into the slot and restores them
 * before the map reloads. Rules: claude-docs/gameplay-semantics.md, "Save data".
 */
UCLASS(BlueprintType)
class HAWKEYE_API UHawkeyeCampaignState : public UObject, public ISpudObject
{
	GENERATED_BODY()

public:
	/** The version this build writes. Bump it whenever a saved field changes meaning. */
	static constexpr int32 CurrentVersion = 1;

	/** Save format version, written with every save. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	int32 Version = CurrentVersion;

	/** The chapter's mission asset as a soft object path (/Game/Missions/DA_CH01_Rooftops.DA_CH01_Rooftops). */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	FString MissionPath;

	/** Ids of every completed objective of that mission, optional ones included. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	TArray<FName> CompletedObjectives;

	/** Safehouses entered at least once; fast travel lists these. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	TArray<FName> DiscoveredSafehouses;

	/** Seconds of play in this campaign, across sessions. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	float PlayTimeSeconds = 0.f;

	/** CharacterName of the Hawkeye the player controlled when the save was written. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	FString ControlledCharacter;

	/** Back to a fresh campaign. */
	UFUNCTION(BlueprintCallable, Category = "Save")
	void ResetCampaign();

	/**
	 * Whether a save of SavedVersion can be brought up to CurrentVersion. There are no migrations
	 * yet, so only the current version passes; an older save starts a new game.
	 */
	static bool CanMigrateFrom(int32 SavedVersion);

	/** True when MissionPath names a mission asset that exists in this build. */
	static bool DoesMissionExist(const FString& InMissionPath);

	/**
	 * The rule Continue follows: no save, a version with no migration, or a chapter that no longer
	 * exists all start a new game; only a current save of an existing chapter loads.
	 */
	static EHawkeyeLoadDecision EvaluateLoad(bool bSaveExists, int32 SavedVersion, const FString& InMissionPath);

	//~ Begin ISpudObject interface
	virtual FString OverrideName_Implementation() const override;
	//~ End ISpudObject interface
};

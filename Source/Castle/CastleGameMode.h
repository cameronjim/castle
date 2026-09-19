// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CastleGameMode.generated.h"

class UInventoryComponent;
class UMissionDefinition;

/**
 * Default game mode. Create a Blueprint child per level, set StartingMission, and pick the
 * BP_CastleCharacter pawn.
 */
UCLASS(Blueprintable, BlueprintType)
class CASTLE_API ACastleGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACastleGameMode();

	/** Mission handed to the UMissionSubsystem when play begins. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission")
	TObjectPtr<UMissionDefinition> StartingMission;

	/** Seconds between the player dying and the level reloading. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission", meta = (ClampMin = "0.0"))
	float RestartDelaySeconds = 2.f;

	/**
	 * Reopens the current level, which restarts the mission from the top (there are no
	 * checkpoints until stage 3). Repeat calls are ignored.
	 *
	 * Delay is the wait in seconds before the level reloads. A negative Delay (the default)
	 * uses RestartDelaySeconds, which is what dying does so the death screen has time to read;
	 * the pause menu passes 0 because the player already chose to start over.
	 */
	UFUNCTION(BlueprintCallable, Category = "Mission")
	void RestartMission(float Delay = -1.f);

	UFUNCTION(BlueprintPure, Category = "Mission")
	bool IsRestartPending() const { return bRestartPending; }

	/**
	 * Builds the navmesh once at BeginPlay.
	 *
	 * The maps are generated headlessly and nobody ever pressed Build Paths, so the navmesh
	 * they ship with is empty: nav data spawns, every MoveTo fails with "off the navmesh" and
	 * the guards stand still. Rebuilding at start costs a fraction of a second on maps this
	 * size. TODO(stage3): build and save navigation with the level instead.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Navigation")
	bool bBuildNavigationAtStart = true;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Timer body: actually reopens the level. */
	void ReopenCurrentLevel();

	/** Rebuilds navigation when the level shipped without any. See bBuildNavigationAtStart. */
	void BuildNavigationIfEmpty();
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleMissionComplete(UMissionDefinition* Mission);

	UFUNCTION()
	void HandleMissionStarted(UMissionDefinition* Mission);

	/** The local player's inventory, or null before a pawn exists. */
	UInventoryComponent* FindPlayerInventory() const;

	/** Blueprint hook for end-of-mission scoring, stats screens, achievements. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Mission")
	void OnMissionCompleted(UMissionDefinition* Mission);

	/** Blueprint hook for a death screen shown during RestartDelaySeconds. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Mission")
	void OnMissionRestarting();

private:
	bool bRestartPending = false;
	FTimerHandle RestartTimerHandle;
};

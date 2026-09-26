// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "HawkeyeGameMode.generated.h"

class UInventoryComponent;
class UMissionDefinition;
class UMissionObjective;

/**
 * Default game mode. Create a Blueprint child per level, set StartingMission, and pick the
 * BP_HawkeyeCharacter pawn.
 */
UCLASS(Blueprintable, BlueprintType)
class HAWKEYE_API AHawkeyeGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AHawkeyeGameMode();

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
	 * Death and the pause menu's restart: fades to black over FadeSeconds (negative uses
	 * DeathFadeSeconds), then loads the last autosave through UHawkeyeSaveSubsystem. With no save
	 * to load it falls back to RestartMission's level reload. Repeat calls are ignored.
	 */
	UFUNCTION(BlueprintCallable, Category = "Mission")
	void ReloadLastCheckpoint(float FadeSeconds = -1.f);

	/** Seconds of fade to black between dying and the checkpoint loading. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Save", meta = (ClampMin = "0.0"))
	float DeathFadeSeconds = 1.5f;

	/** How often the roaming autosave clock is advanced, seconds of unpaused play. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Save", meta = (ClampMin = "0.1"))
	float AutosaveTickSeconds = 1.f;

	/**
	 * True while any living thug is alerted or the partner's fight clock is running. The roaming
	 * autosave waits for this to clear.
	 */
	UFUNCTION(BlueprintPure, Category = "Save")
	bool IsInCombat() const;

	/** The roaming autosave's gate: a living, standing player on the ground, and no fight on. */
	UFUNCTION(BlueprintPure, Category = "Save")
	bool CanAutosaveNow() const;

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

	UFUNCTION()
	void HandleObjectiveUpdated(UMissionObjective* Objective, int32 ObjectiveIndex);

	/** Timer body: one step of the roaming autosave clock. */
	void TickAutosave();

	/** Timer body: the fade is black; load the checkpoint. */
	void LoadCheckpointNow();

	/** Timer body: the first save of a new game, once the world has settled. */
	void SaveNewGame();

	/** Blueprint hook for end-of-mission scoring, stats screens, achievements. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Mission")
	void OnMissionCompleted(UMissionDefinition* Mission);

	/** Blueprint hook for a death screen shown during RestartDelaySeconds. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Mission")
	void OnMissionRestarting();

private:
	bool bRestartPending = false;
	FTimerHandle RestartTimerHandle;
	FTimerHandle AutosaveTimerHandle;
	FTimerHandle NewGameSaveTimerHandle;
};

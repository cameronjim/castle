// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CastleGameMode.generated.h"

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
	 * Reopens the current level after RestartDelaySeconds, which restarts the mission from the
	 * top (there are no checkpoints until stage 3). Repeat calls are ignored.
	 */
	UFUNCTION(BlueprintCallable, Category = "Mission")
	void RestartMission();

	UFUNCTION(BlueprintPure, Category = "Mission")
	bool IsRestartPending() const { return bRestartPending; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Timer body: actually reopens the level. */
	void ReopenCurrentLevel();
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleMissionComplete(UMissionDefinition* Mission);

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

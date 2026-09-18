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

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleMissionComplete(UMissionDefinition* Mission);

	/** Blueprint hook for end-of-mission scoring, stats screens, achievements. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Mission")
	void OnMissionCompleted(UMissionDefinition* Mission);
};

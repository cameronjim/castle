// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CastlePlayerController.generated.h"

class UFlashbackDefinition;
class UFlashbackWidget;
class UMissionDefinition;

/**
 * Owns the flashback presentation: listens to the mission subsystem, plays the slideshow and
 * travels to the mission's NextLevel once it finishes.
 */
UCLASS(Blueprintable, BlueprintType)
class CASTLE_API ACastlePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** UMG widget (reparented to UFlashbackWidget) used to present flashbacks. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Flashback")
	TSubclassOf<UFlashbackWidget> FlashbackWidgetClass;

	/** Travel to the completed mission's NextLevel once the flashback ends. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Flashback")
	bool bOpenNextLevelAfterFlashback = true;

	/** Plays a flashback immediately (debug, or bespoke story beats). */
	UFUNCTION(BlueprintCallable, Category = "Flashback")
	UFlashbackWidget* PlayFlashback(UFlashbackDefinition* Flashback);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleFlashbackRequested(UFlashbackDefinition* Flashback);

	UFUNCTION()
	void HandleFlashbackFinished(UFlashbackDefinition* Flashback);

	/** Opens the current mission's NextLevel if one is set. Returns true when travel started. */
	bool TryOpenNextLevel();

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Flashback")
	TObjectPtr<UFlashbackWidget> ActiveFlashbackWidget = nullptr;
};

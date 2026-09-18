// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CastlePlayerController.generated.h"

class UCastleHudWidget;
class UFlashbackDefinition;
class UFlashbackWidget;
class UMissionDefinition;

/**
 * Owns the HUD and the flashback presentation: listens to the mission subsystem, plays the slideshow and
 * travels to the mission's NextLevel once it finishes.
 */
UCLASS(Blueprintable, BlueprintType)
class CASTLE_API ACastlePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** UMG widget (reparented to UCastleHudWidget) created on BeginPlay. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HUD")
	TSubclassOf<UCastleHudWidget> HudWidgetClass;

	/** The live HUD, or nullptr when HudWidgetClass is unset. */
	UFUNCTION(BlueprintPure, Category = "HUD")
	UCastleHudWidget* GetCastleHud() const { return HudWidget; }

	/** Convenience for world actors: the local player's HUD, or nullptr. */
	UFUNCTION(BlueprintPure, Category = "HUD", meta = (WorldContext = "WorldContextObject"))
	static UCastleHudWidget* GetCastleHudFor(const UObject* WorldContextObject);

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

	/** Creates the HUD from HudWidgetClass and adds it to the viewport. */
	void CreateHud();

	void SetHudVisible(bool bVisible);

	UPROPERTY(Transient, BlueprintReadOnly, Category = "HUD")
	TObjectPtr<UCastleHudWidget> HudWidget = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Flashback")
	TObjectPtr<UFlashbackWidget> ActiveFlashbackWidget = nullptr;
};

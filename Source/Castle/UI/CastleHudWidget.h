// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CastleHudWidget.generated.h"

class UImage;
class UTextBlock;
class UMissionDefinition;
class UMissionObjective;
class UWeaponComponent;

/**
 * Bare gameplay HUD: current objective, ammo, an interaction prompt and a crosshair.
 *
 * Reparent a UMG widget to this class and name widgets ObjectiveText, AmmoText, PromptText and
 * Crosshair to have them driven automatically; a subclass with no designer layout works too,
 * because RebuildWidget builds them itself (same approach as UFlashbackWidget).
 *
 * All of the state comes from delegates: the mission subsystem drives the objective line, the
 * pawn's UWeaponComponent drives the ammo line, and UInteractionComponent drives the prompt.
 */
UCLASS(Blueprintable, BlueprintType)
class CASTLE_API UCastleHudWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Subscribes to the mission subsystem and to OwningPawn's weapon. Safe to call twice. */
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void BindToGame();

	/** Drops every subscription. Called from NativeDestruct. */
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void UnbindFromGame();

	/** Shows Prompt on the prompt line ("Open", "Press F to take down"). Empty text clears it. */
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void SetPrompt(FText Prompt);

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void ClearPrompt();

	UFUNCTION(BlueprintPure, Category = "HUD")
	FText GetPrompt() const { return CurrentPrompt; }

	/** Re-reads the current objective and repaints the objective line. */
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void RefreshObjective();

	/** Re-reads the pawn's weapon and repaints the ammo line (blank while unarmed). */
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void RefreshAmmo();

	/** "12 / 24", or empty when the pawn has no weapon. Pure so a test can assert on it. */
	UFUNCTION(BlueprintPure, Category = "HUD")
	static FText FormatAmmo(int32 Magazine, int32 Reserve);

protected:
	//~ Begin UUserWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End UUserWidget interface

	UFUNCTION()
	void HandleMissionStarted(UMissionDefinition* Mission);

	UFUNCTION()
	void HandleObjectiveUpdated(UMissionObjective* Objective, int32 ObjectiveIndex);

	UFUNCTION()
	void HandleMissionComplete(UMissionDefinition* Mission);

	UFUNCTION()
	void HandleAmmoChanged(int32 Magazine, int32 Reserve);

	UFUNCTION()
	void HandleTakedownPerformed(AActor* Target);

	/** The weapon the HUD is currently listening to; rebound whenever the pawn picks one up. */
	UWeaponComponent* FindPawnWeapon() const;

	/** Player-facing objective line ("Find a weapon"), or empty when there is nothing to show. */
	FText BuildObjectiveText() const;

	UPROPERTY(BlueprintReadOnly, Category = "HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ObjectiveText = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> AmmoText = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PromptText = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "HUD", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Crosshair = nullptr;

	/** Shown on the objective line once every required objective is done. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD")
	FText MissionCompleteText;

	UPROPERTY(Transient)
	FText CurrentPrompt;

	/** Weapon currently bound to OnAmmoChanged, so the binding can be swapped on pickup. */
	UPROPERTY(Transient)
	TObjectPtr<UWeaponComponent> BoundWeapon = nullptr;

	bool bBound = false;
};

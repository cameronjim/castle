// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CastleHudWidget.generated.h"

class UBorder;
class UCanvasPanel;
class UCastleHotbarWidget;
class UInventoryComponent;
class UOverlay;
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

	// --- Hotbar ---------------------------------------------------------------------------------

	/** Hotbar built into the HUD. WBP_Hotbar when the content script has wired one. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HUD|Hotbar")
	TSubclassOf<UCastleHotbarWidget> HotbarWidgetClass;

	UFUNCTION(BlueprintPure, Category = "HUD|Hotbar")
	UCastleHotbarWidget* GetHotbar() const { return Hotbar; }

	// --- Crosshair ------------------------------------------------------------------------------

	/** Tightens the centre gap to AimGapPixels. Driven from the pawn's aim state every frame. */
	UFUNCTION(BlueprintCallable, Category = "HUD|Crosshair")
	void SetCrosshairAiming(bool bNewAiming);

	/** Fades the bars to SprintOpacity: you cannot shoot accurately while sprinting anyway. */
	UFUNCTION(BlueprintCallable, Category = "HUD|Crosshair")
	void SetCrosshairSprinting(bool bNewSprinting);

	/**
	 * Collapses the four bars to a single centre dot. Hands get a dot rather than bars: there
	 * is no cone of fire to show, only where the punch lands.
	 */
	UFUNCTION(BlueprintCallable, Category = "HUD|Crosshair")
	void SetCrosshairDotMode(bool bNewDotMode);

	UFUNCTION(BlueprintPure, Category = "HUD|Crosshair")
	bool IsCrosshairDotMode() const { return bDotMode; }

	UFUNCTION(BlueprintPure, Category = "HUD|Crosshair")
	float GetCrosshairDotSize() const { return DotSizePixels; }

	/** Flashes the four bars white for HitFlashSeconds. Bound to UWeaponComponent::OnHit. */
	UFUNCTION(BlueprintCallable, Category = "HUD|Crosshair")
	void FlashHitMarker();

	/** Distance in pixels from screen centre to the near end of each bar. */
	UFUNCTION(BlueprintPure, Category = "HUD|Crosshair")
	float GetCrosshairGap() const { return bCrosshairAiming ? AimGapPixels : HipGapPixels; }

	/** How long each bar is, in pixels. Exposed so the size is a rule and not a guess. */
	UFUNCTION(BlueprintPure, Category = "HUD|Crosshair")
	float GetCrosshairBarLength() const { return BarLengthPixels; }

	UFUNCTION(BlueprintPure, Category = "HUD|Crosshair")
	float GetCrosshairBarThickness() const { return BarThicknessPixels; }

	/** The crosshair is hidden until Frank has a weapon in his hands. */
	UFUNCTION(BlueprintPure, Category = "HUD|Crosshair")
	bool IsCrosshairVisible() const;

	/** White while a hit marker is flashing, CrosshairColor otherwise. Pure so a test can read it. */
	UFUNCTION(BlueprintPure, Category = "HUD|Crosshair")
	FLinearColor GetCrosshairColor() const;

	/** Bars currently in the crosshair: four, unless the widget was never built. */
	UFUNCTION(BlueprintPure, Category = "HUD|Crosshair")
	int32 GetCrosshairBarCount() const { return CrosshairBars.Num(); }

protected:
	//~ Begin UUserWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float DeltaSeconds) override;
	//~ End UUserWidget interface

	/** Builds the four bars into a screen-filling canvas so they land on the exact centre pixel. */
	void BuildCrosshair(UOverlay* Root);

	/** Re-positions the bars for the current gap and re-applies colour, opacity and visibility. */
	void RefreshCrosshair();

	/** Reads aim and sprint off the pawn; the HUD has no input of its own to listen to. */
	void PollPawnCrosshairState();

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

	UFUNCTION()
	void HandleWeaponHit(AActor* HitActor, float DamageDealt);

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

	/** Screen-filling canvas the crosshair bars are anchored to. Built in RebuildWidget. */
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Crosshair", meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> Crosshair = nullptr;

	/** Top, bottom, left, right. No texture: four coloured borders in a plus with a centre gap. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "HUD|Crosshair")
	TArray<TObjectPtr<UBorder>> CrosshairBars;

	/** Toxic green, the one bright thing on a grey-box prison HUD. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HUD|Crosshair")
	FLinearColor CrosshairColor = FLinearColor(0.22f, 1.f, 0.08f, 1.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HUD|Crosshair")
	FLinearColor HitMarkerColor = FLinearColor::White;

	/** Small: Cameron's note after the third play was that the old 14 x 3 plus was a target. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HUD|Crosshair", meta = (ClampMin = "1.0"))
	float BarLengthPixels = 7.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HUD|Crosshair", meta = (ClampMin = "1.0"))
	float BarThicknessPixels = 2.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HUD|Crosshair", meta = (ClampMin = "0.0"))
	float HipGapPixels = 3.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HUD|Crosshair", meta = (ClampMin = "0.0"))
	float AimGapPixels = 2.f;

	/** Size of the single dot drawn instead of the bars while Frank's fists are up. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HUD|Crosshair", meta = (ClampMin = "1.0"))
	float DotSizePixels = 2.f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "HUD|Crosshair")
	bool bDotMode = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HUD|Crosshair", meta = (ClampMin = "0.0"))
	float HitFlashSeconds = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HUD|Crosshair", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SprintOpacity = 0.4f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "HUD|Crosshair")
	bool bCrosshairAiming = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "HUD|Crosshair")
	bool bCrosshairSprinting = false;

	/** Seconds of hit-marker flash left to run. */
	UPROPERTY(Transient)
	float HitFlashRemaining = 0.f;

	/** Shown on the objective line once every required objective is done. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD")
	FText MissionCompleteText;

	UPROPERTY(Transient)
	FText CurrentPrompt;

	/** Weapon currently bound to OnAmmoChanged, so the binding can be swapped on pickup. */
	UPROPERTY(Transient)
	TObjectPtr<UWeaponComponent> BoundWeapon = nullptr;

	/** The hotbar along the bottom of the screen. Built into the HUD's own overlay. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "HUD|Hotbar")
	TObjectPtr<UCastleHotbarWidget> Hotbar = nullptr;

	/** The pawn's inventory, for the ammo line and the crosshair's dot mode. */
	UInventoryComponent* FindPawnInventory() const;

	bool bBound = false;
};

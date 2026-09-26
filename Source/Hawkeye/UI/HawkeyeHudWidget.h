// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HawkeyeHudWidget.generated.h"

class UBorder;
class UBowComponent;
class UCanvasPanel;
class UHawkeyeHotbarWidget;
class UHawkeyeObjectiveWidget;
class UGrappleComponent;
class UImage;
class UInventoryComponent;
class UOverlay;
class UTextBlock;
class UMissionDefinition;
class UMissionObjective;
class UWeaponComponent;

/**
 * Bare gameplay HUD: current objective, ammo, an interaction prompt, a third-person reticle and
 * an optional movement debug line.
 *
 * Reparent a UMG widget to this class and name widgets ObjectiveText, AmmoText, PromptText,
 * DebugText and Reticle to have them driven automatically; a subclass with no designer layout
 * works too, because RebuildWidget builds them itself (same approach as UFlashbackWidget).
 *
 * All of the state comes from delegates: the mission subsystem drives the objective line, the
 * pawn's UWeaponComponent drives the ammo line, and UInteractionComponent drives the prompt.
 */
UCLASS(Blueprintable, BlueprintType)
class HAWKEYE_API UHawkeyeHudWidget : public UUserWidget
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

	/**
	 * Swaps a keyboard-flavoured hint ("[F] Take down") for its gamepad reading ("[Y] Take down")
	 * when the owning controller last saw a Gamepad_* key. Public so other widgets building their
	 * own hint strings (not routed through SetPrompt) can reuse it.
	 */
	UFUNCTION(BlueprintPure, Category = "HUD")
	FText ApplyGamepadHint(FText KeyboardHint) const;

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
	TSubclassOf<UHawkeyeHotbarWidget> HotbarWidgetClass;

	UFUNCTION(BlueprintPure, Category = "HUD|Hotbar")
	UHawkeyeHotbarWidget* GetHotbar() const { return Hotbar; }

	// --- Objective marker and compass -----------------------------------------------------------

	/** The objective marker, compass and toasts, built into the HUD's overlay. */
	UFUNCTION(BlueprintPure, Category = "HUD|Objective")
	UHawkeyeObjectiveWidget* GetObjectiveMarker() const { return ObjectiveMarker; }

	// --- Reticle --------------------------------------------------------------------------------

	/** Shows the reticle while aiming, hides it otherwise. Driven from the pawn every frame. */
	UFUNCTION(BlueprintCallable, Category = "HUD|Reticle")
	void SetReticleAiming(bool bNewAiming);

	/**
	 * Third person has no crosshair at the hip: the body and the camera say where you face. The
	 * dot only appears while aiming, where the arrow will go.
	 */
	UFUNCTION(BlueprintPure, Category = "HUD|Reticle")
	bool IsReticleVisible() const { return bReticleAiming; }

	/** Side of the square dot, in pixels. */
	UFUNCTION(BlueprintPure, Category = "HUD|Reticle")
	float GetReticleSize() const { return ReticleSizePixels; }

	/** White while a hit marker is flashing, ReticleColor otherwise. Pure so a test can read it. */
	UFUNCTION(BlueprintPure, Category = "HUD|Reticle")
	FLinearColor GetReticleColor() const;

	/** Flashes the dot white for HitFlashSeconds. Bound to UWeaponComponent::OnHit and UBowComponent::OnHit. */
	UFUNCTION(BlueprintCallable, Category = "HUD|Reticle")
	void FlashHitMarker();

	/**
	 * Radius in widget pixels of a cone of SpreadDegrees seen at Distance: the circle it covers
	 * there (Distance * tan(spread)), projected through a camera of FovDegrees horizontal FOV onto a
	 * viewport ViewportWidth wide. Pure so the ring's size is testable.
	 */
	UFUNCTION(BlueprintPure, Category = "HUD|Reticle")
	static float ComputeSpreadRingRadius(float SpreadDegrees, float FovDegrees, float ViewportWidth, float Distance = 1500.f);

	/** Shows the spread ring (InRingRadius widget px) and the draw bar. Driven from the pawn's bow every frame. */
	UFUNCTION(BlueprintCallable, Category = "HUD|Reticle")
	void SetDrawState(bool bInDrawing, float InFraction, float InRingRadius, bool bInPerfect);

	/** True while the bow is drawn: the ring and the draw bar are up. */
	UFUNCTION(BlueprintPure, Category = "HUD|Reticle")
	bool IsDrawIndicatorVisible() const { return bDrawing; }

	/** Current ring radius in widget pixels (0 while not drawing). */
	UFUNCTION(BlueprintPure, Category = "HUD|Reticle")
	float GetReticleRingRadius() const { return RingRadius; }

	UFUNCTION(BlueprintPure, Category = "HUD|Reticle")
	float GetDrawBarFraction() const { return DrawFraction; }

	// --- Grapple marker -------------------------------------------------------------------------

	/**
	 * True while the pawn's grapple has a target and a press would fire: hidden during a zip
	 * until the chain window opens, so the next anchor can be picked on the way in.
	 */
	UFUNCTION(BlueprintPure, Category = "HUD|Grapple")
	bool IsGrappleMarkerVisible() const { return bGrappleMarkerVisible; }

	/** Centre of the marker in viewport widget space, where the anchor projects to. */
	UFUNCTION(BlueprintPure, Category = "HUD|Grapple")
	FVector2D GetGrappleMarkerPosition() const { return GrappleMarkerPosition; }

	/** The key hint under the marker, shown for the first GrappleHintUses arrows. */
	UFUNCTION(BlueprintPure, Category = "HUD|Grapple")
	bool IsGrappleHintVisible() const;

	// --- Movement debug -------------------------------------------------------------------------

	/** The debug line's text, or empty while hawkeye.DebugMovement is 0 or there is no player. */
	UFUNCTION(BlueprintPure, Category = "HUD|Debug")
	FText GetMovementDebugText() const;

protected:
	//~ Begin UUserWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float DeltaSeconds) override;
	//~ End UUserWidget interface

	/** Builds the dot into a screen-filling canvas so it lands on the exact centre pixel. */
	void BuildReticle(UOverlay* Root);

	/** Re-applies size, colour and visibility to the dot. */
	void RefreshReticle();

	/** Reads aim off the pawn; the HUD has no input of its own to listen to. */
	void PollPawnReticleState();

	/** A screen-filling canvas holding the hollow diamond and its key hint. */
	void BuildGrappleMarker(UOverlay* Root);

	/** Projects the pawn's grapple target onto the screen and moves (or hides) the marker. */
	void UpdateGrappleMarker();

	/** Shows or hides the debug line and repaints it. */
	void RefreshMovementDebug();

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

	UFUNCTION()
	void HandleBowHit(AActor* HitActor, float Damage, bool bHeadshot);

	/** The owning pawn's bow, or null. */
	UBowComponent* FindPawnBow() const;

	/** Reads the draw off the pawn's bow and the camera FOV, and resizes the ring and bar. */
	void PollPawnDrawState(const FGeometry& MyGeometry);

	/** Re-applies the ring's size and colour and the bar's fill and flash. */
	void RefreshDrawIndicator();

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

	/** hawkeye.DebugMovement line, under the objective. Collapsed while the cvar is 0. */
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Debug", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DebugText = nullptr;

	/** Screen-filling canvas the dot is anchored to. Built in RebuildWidget. */
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Reticle", meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> Reticle = nullptr;

	/** The dot itself: a coloured border, no texture. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "HUD|Reticle")
	TObjectPtr<UBorder> ReticleDot = nullptr;

	/** The spread ring around the dot while drawing: an outlined circle. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "HUD|Reticle")
	TObjectPtr<UImage> ReticleRing = nullptr;

	/** Thin bar under the reticle: its track, and the fill that grows to full draw. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "HUD|Reticle")
	TObjectPtr<UBorder> DrawBarBack = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "HUD|Reticle")
	TObjectPtr<UBorder> DrawBarFill = nullptr;

	/** Distance the spread ring is measured at, cm. At 15 m the ring is the circle an arrow lands in. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HUD|Reticle", meta = (ClampMin = "100.0"))
	float ReticleSpreadDistance = 1500.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HUD|Reticle", meta = (ClampMin = "0.5"))
	float RingLineWidth = 1.5f;

	/** Radius of the white ring a hit flashes around the dot when the bow is not drawn, px. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HUD|Reticle", meta = (ClampMin = "1.0"))
	float HitRingRadius = 9.f;

	/** Smallest the ring is drawn, so it never collapses onto the dot, px. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HUD|Reticle", meta = (ClampMin = "0.0"))
	float MinRingRadius = 6.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HUD|Reticle", meta = (ClampMin = "1.0"))
	float DrawBarWidth = 64.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HUD|Reticle", meta = (ClampMin = "1.0"))
	float DrawBarHeight = 3.f;

	/** How far below the centre the bar sits, px: under the widest ring. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HUD|Reticle")
	float DrawBarOffset = 84.f;

	/** The bar during the perfect-release window. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HUD|Reticle")
	FLinearColor PerfectColor = FLinearColor(1.f, 0.85f, 0.2f, 1.f);

	UPROPERTY(Transient)
	bool bDrawing = false;

	UPROPERTY(Transient)
	bool bPerfectWindow = false;

	UPROPERTY(Transient)
	float DrawFraction = 0.f;

	UPROPERTY(Transient)
	float RingRadius = 0.f;

	/** Bow currently bound to OnHit. */
	UPROPERTY(Transient)
	TObjectPtr<UBowComponent> BoundBow = nullptr;

	/** Kate purple, light enough to read against a night sky and a grey wall alike. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HUD|Reticle")
	FLinearColor ReticleColor = FLinearColor(0.78f, 0.55f, 1.f, 1.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HUD|Reticle")
	FLinearColor HitMarkerColor = FLinearColor::White;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HUD|Reticle", meta = (ClampMin = "1.0"))
	float ReticleSizePixels = 4.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HUD|Reticle", meta = (ClampMin = "0.0"))
	float HitFlashSeconds = 0.1f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "HUD|Reticle")
	bool bReticleAiming = false;

	/** Seconds of hit-marker flash left to run. */
	UPROPERTY(Transient)
	float HitFlashRemaining = 0.f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "HUD|Grapple")
	TObjectPtr<UCanvasPanel> GrappleCanvas = nullptr;

	/** A square outline turned 45 degrees: a hollow diamond. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "HUD|Grapple")
	TObjectPtr<UImage> GrappleMarker = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "HUD|Grapple")
	TObjectPtr<UTextBlock> GrappleHint = nullptr;

	/** The green of the old first-person crosshair: nothing else on screen is that colour. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HUD|Grapple")
	FLinearColor GrappleMarkerColor = FLinearColor(0.22f, 1.f, 0.08f, 1.f);

	/** Point to point size of the diamond, in pixels. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HUD|Grapple", meta = (ClampMin = "2.0"))
	float GrappleMarkerSizePixels = 14.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HUD|Grapple", meta = (ClampMin = "0.5"))
	float GrappleMarkerLineWidth = 2.f;

	/** The hint shows until this many grapple arrows have been fired. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HUD|Grapple", meta = (ClampMin = "0"))
	int32 GrappleHintUses = 5;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HUD|Grapple")
	FText GrappleHintText;

	UPROPERTY(Transient)
	bool bGrappleMarkerVisible = false;

	UPROPERTY(Transient)
	FVector2D GrappleMarkerPosition = FVector2D::ZeroVector;

	/** Shown on the objective line once every required objective is done. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD")
	FText MissionCompleteText;

	UPROPERTY(Transient)
	FText CurrentPrompt;

	/** Weapon currently bound to OnAmmoChanged, so the binding can be swapped on pickup. */
	UPROPERTY(Transient)
	TObjectPtr<UWeaponComponent> BoundWeapon = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "HUD|Objective")
	TObjectPtr<UHawkeyeObjectiveWidget> ObjectiveMarker = nullptr;

	/** The hotbar along the bottom of the screen. Built into the HUD's own overlay. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "HUD|Hotbar")
	TObjectPtr<UHawkeyeHotbarWidget> Hotbar = nullptr;

	bool bBound = false;
};

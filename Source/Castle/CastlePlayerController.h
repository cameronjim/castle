// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CastlePlayerController.generated.h"

class SWidget;
class UCastleHudWidget;
class UCastleInventoryWidget;
class UCastlePauseWidget;
class UCastleSettingsWidget;
class UFlashbackDefinition;
class UFlashbackWidget;
class UInputAction;
class UInputMappingContext;
class UMissionDefinition;
class UMissionEndCardWidget;
class UMissionFlowController;
struct FInputActionValue;

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

	// --- End card -------------------------------------------------------------------------------

	/** UMG widget (reparented to UMissionEndCardWidget) shown when a mission completes. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "End card")
	TSubclassOf<UMissionEndCardWidget> EndCardWidgetClass;

	/**
	 * Level opened when a completed mission has no NextLevel, i.e. the campaign is over.
	 * Falls back to FallbackMenuLevel when this one does not exist.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "End card")
	FName MenuLevelName = FName(TEXT("/Game/Maps/L_MainMenu"));

	/** Used when MenuLevelName has not been built yet, so the campaign end never dead-ends. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "End card")
	FName FallbackMenuLevelName = FName(TEXT("/Game/Maps/L_Sandbox"));

	/** Shown on the final card, which waits for a key instead of counting down. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "End card")
	FText FinalCardPrompt;

	/** The end-of-mission beat ordering. Created on BeginPlay and never null. */
	UFUNCTION(BlueprintPure, Category = "End card")
	UMissionFlowController* GetMissionFlow() const { return MissionFlow; }

	// --- Pause ----------------------------------------------------------------------------------

	/** UMG widget (reparented to UCastlePauseWidget) shown while the game is paused. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Pause")
	TSubclassOf<UCastlePauseWidget> PauseWidgetClass;

	/**
	 * Enhanced Input action for the Escape key. Bound on the controller rather than the pawn so
	 * pausing still works while the pawn is locked out, dead or not yet possessed.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pause")
	TObjectPtr<UInputAction> PauseAction;

	/**
	 * Mapping context added by the controller itself. Normally IMC_Default, so the Escape key
	 * still reaches us when there is no pawn to add it.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pause")
	TObjectPtr<UInputMappingContext> PauseMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pause", meta = (ClampMin = "0"))
	int32 PauseMappingPriority = 0;

	/** Opens the pause menu, or closes it when it is already open. Ignored when CanTogglePause is false. */
	UFUNCTION(BlueprintCallable, Category = "Pause")
	void TogglePause();

	/** Opens or closes the pause menu explicitly. */
	UFUNCTION(BlueprintCallable, Category = "Pause")
	void SetPauseMenuOpen(bool bOpen);

	UFUNCTION(BlueprintPure, Category = "Pause")
	bool IsPauseMenuOpen() const { return bPauseMenuOpen; }

	/**
	 * False while a flashback is playing (the slideshow owns the pause) and while a mission
	 * restart is already under way, which is what dying starts.
	 */
	UFUNCTION(BlueprintPure, Category = "Pause")
	bool CanTogglePause() const;

	/** True between PlayFlashback and OnFlashbackFinished. */
	UFUNCTION(BlueprintPure, Category = "Flashback")
	bool IsFlashbackActive() const { return bFlashbackActive; }

	/** Pause menu "Restart mission": reloads the level now, without the death delay. */
	UFUNCTION(BlueprintCallable, Category = "Pause")
	void RestartMissionFromPause();

	// --- Inventory ------------------------------------------------------------------------------

	/** UMG widget (reparented to UCastleInventoryWidget) shown on Tab. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Inventory")
	TSubclassOf<UCastleInventoryWidget> InventoryWidgetClass;

	/** Opens the inventory screen, or closes it when it is already open. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void ToggleInventory();

	/**
	 * Opens or closes the inventory screen. Opening pauses the game the way the pause menu
	 * does; Tab or Escape closes it. Refused while the pause menu or a flashback owns the pause.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SetInventoryOpen(bool bOpen);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool IsInventoryOpen() const { return bInventoryOpen; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	UCastleInventoryWidget* GetInventoryWidget() const { return InventoryWidget; }

	// --- Settings -------------------------------------------------------------------------------

	/** UMG widget (reparented to UCastleSettingsWidget) shown when Settings is chosen. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Settings")
	TSubclassOf<UCastleSettingsWidget> SettingsWidgetClass;

	/** Swaps the pause menu for the settings screen. The game stays paused throughout. */
	UFUNCTION(BlueprintCallable, Category = "Settings")
	void OpenSettings();

	/** Swaps the settings screen back for the pause menu. */
	UFUNCTION(BlueprintCallable, Category = "Settings")
	void CloseSettings();

	UFUNCTION(BlueprintPure, Category = "Settings")
	bool IsSettingsOpen() const { return bSettingsOpen; }

	/** Pause menu "Quit to desktop". */
	UFUNCTION(BlueprintCallable, Category = "Pause")
	void QuitToDesktop();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;

	void Input_Pause(const FInputActionValue& Value);

	/** Adds PauseMappingContext to the local player's Enhanced Input subsystem. */
	void AddPauseMappingContext();

	UFUNCTION()
	void HandlePauseResumeClicked();

	UFUNCTION()
	void HandlePauseSettingsClicked();

	UFUNCTION()
	void HandleSettingsBackRequested();

	UFUNCTION()
	void HandlePauseRestartClicked();

	UFUNCTION()
	void HandlePauseQuitClicked();

	/** Creates PauseWidget (if needed) and adds it to the viewport. Returns the widget or null. */
	UCastlePauseWidget* ShowPauseWidget();

	void HidePauseWidget();

	/** Creates SettingsWidget (if needed) and adds it to the viewport. Returns the widget or null. */
	UCastleSettingsWidget* ShowSettingsWidget();

	void HideSettingsWidget();

	/** Whichever menu should hold keyboard focus right now, or an invalid pointer for none. */
	TSharedPtr<SWidget> GetFocusedMenuWidget() const;

	/** UI+Game input with a cursor while paused, game-only input while playing. */
	void ApplyPauseInputMode(bool bPaused);

	UFUNCTION()
	void HandleFlashbackRequested(UFlashbackDefinition* Flashback);

	UFUNCTION()
	void HandleFlashbackFinished(UFlashbackDefinition* Flashback);

	UFUNCTION()
	void HandleMissionComplete(UMissionDefinition* Mission);

	UFUNCTION()
	void HandleEndCardFinished(UMissionDefinition* Mission);

	/** Runs whichever beat the flow is on now. Called after every Begin and Advance. */
	void PerformCurrentFlowStep();

	/** Creates the end card (if needed) and plays it. bWaitForInput is the campaign-end card. */
	UMissionEndCardWidget* ShowEndCard(UMissionDefinition* Mission, bool bWaitForInput);

	void HideEndCard();

	/** Opens the current mission's NextLevel if one is set. Returns true when travel started. */
	bool TryOpenNextLevel();

	/** Opens MenuLevelName, or FallbackMenuLevelName when that map does not exist. */
	void OpenMenuLevel();

	/** Creates the HUD from HudWidgetClass and adds it to the viewport. */
	void CreateHud();

	void SetHudVisible(bool bVisible);

	UPROPERTY(Transient, BlueprintReadOnly, Category = "HUD")
	TObjectPtr<UCastleHudWidget> HudWidget = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Flashback")
	TObjectPtr<UFlashbackWidget> ActiveFlashbackWidget = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Pause")
	TObjectPtr<UCastlePauseWidget> PauseWidget = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Settings")
	TObjectPtr<UCastleSettingsWidget> SettingsWidget = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UCastleInventoryWidget> InventoryWidget = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Inventory")
	bool bInventoryOpen = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "End card")
	TObjectPtr<UMissionEndCardWidget> EndCardWidget = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "End card")
	TObjectPtr<UMissionFlowController> MissionFlow = nullptr;

	/** The mission whose end sequence is running. Held because the world outlives the subsystem. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "End card")
	TObjectPtr<UMissionDefinition> CompletedMission = nullptr;

	/** The definition OnFlashbackRequested handed us, played once the end card is done. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Flashback")
	TObjectPtr<UFlashbackDefinition> PendingFlashback = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Pause")
	bool bPauseMenuOpen = false;

	/** True while the settings screen has replaced the pause menu. Implies bPauseMenuOpen. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Settings")
	bool bSettingsOpen = false;

	/**
	 * Set for the whole of a flashback, including the frames where the widget exists but has
	 * not started ticking, so Escape can never steal the pause from the slideshow.
	 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Flashback")
	bool bFlashbackActive = false;
};

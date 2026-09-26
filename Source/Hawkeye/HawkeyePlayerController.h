// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "HawkeyePlayerController.generated.h"

class SWidget;
class AHawkeyeCharacter;
class AHawkeyePartnerController;
class ASafehouse;
class UBanterComponent;
class UHawkeyeHudWidget;
class UHawkeyeInventoryWidget;
class UHawkeyeMainMenuWidget;
class UHawkeyeSafehouseWidget;
class UHawkeyePauseWidget;
class UHawkeyeSettingsWidget;
class UFlashbackDefinition;
class UFlashbackWidget;
class UInputAction;
class UInputMappingContext;
class UMissionDefinition;
class UMissionEndCardWidget;
class UMissionFlowController;
struct FInputActionValue;
struct FInputKeyEventArgs;

/**
 * Owns the HUD and the flashback presentation: listens to the mission subsystem, plays the slideshow and
 * travels to the mission's NextLevel once it finishes.
 */
UCLASS(Blueprintable, BlueprintType)
class HAWKEYE_API AHawkeyePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AHawkeyePlayerController();

	/** UMG widget (reparented to UHawkeyeHudWidget) created on BeginPlay. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HUD")
	TSubclassOf<UHawkeyeHudWidget> HudWidgetClass;

	/** The live HUD, or nullptr when HudWidgetClass is unset. */
	UFUNCTION(BlueprintPure, Category = "HUD")
	UHawkeyeHudWidget* GetHawkeyeHud() const { return HudWidget; }

	/** Convenience for world actors: the local player's HUD, or nullptr. */
	UFUNCTION(BlueprintPure, Category = "HUD", meta = (WorldContext = "WorldContextObject"))
	static UHawkeyeHudWidget* GetHawkeyeHudFor(const UObject* WorldContextObject);

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
	FName FallbackMenuLevelName = FName(TEXT("/Game/Maps/L_District_EastVillage"));

	/** Shown on the final card, which waits for a key instead of counting down. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "End card")
	FText FinalCardPrompt;

	/** The end-of-mission beat ordering. Created on BeginPlay and never null. */
	UFUNCTION(BlueprintPure, Category = "End card")
	UMissionFlowController* GetMissionFlow() const { return MissionFlow; }

	// --- Pause ----------------------------------------------------------------------------------

	/** UMG widget (reparented to UHawkeyePauseWidget) shown while the game is paused. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Pause")
	TSubclassOf<UHawkeyePauseWidget> PauseWidgetClass;

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

	/** UMG widget (reparented to UHawkeyeInventoryWidget) shown on Tab. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Inventory")
	TSubclassOf<UHawkeyeInventoryWidget> InventoryWidgetClass;

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
	UHawkeyeInventoryWidget* GetInventoryWidget() const { return InventoryWidget; }

	// --- Settings -------------------------------------------------------------------------------

	/** UMG widget (reparented to UHawkeyeSettingsWidget) shown when Settings is chosen. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Settings")
	TSubclassOf<UHawkeyeSettingsWidget> SettingsWidgetClass;

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

	/** Pause menu "Quit to menu": the main menu over the paused game, as on first boot. */
	UFUNCTION(BlueprintCallable, Category = "Pause")
	void QuitToMenu();

	// --- Main menu ------------------------------------------------------------------------------

	/** Drawn over the paused district on first boot and after Quit to menu. UHawkeyeMainMenuWidget by default. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Main menu")
	TSubclassOf<UHawkeyeMainMenuWidget> MainMenuWidgetClass;

	/** Pauses the game under the main menu, Continue enabled when a save exists. */
	UFUNCTION(BlueprintCallable, Category = "Main menu")
	void ShowMainMenu();

	/** Takes the main menu down and resumes play. */
	UFUNCTION(BlueprintCallable, Category = "Main menu")
	void HideMainMenu();

	UFUNCTION(BlueprintPure, Category = "Main menu")
	bool IsMainMenuOpen() const { return bMainMenuOpen; }

	UFUNCTION(BlueprintPure, Category = "Main menu")
	UHawkeyeMainMenuWidget* GetMainMenuWidget() const { return MainMenuWidget; }

	/** Continue: loads the campaign save (or, if it cannot be used, starts a new game). */
	UFUNCTION(BlueprintCallable, Category = "Main menu")
	void MainMenuContinue();

	/**
	 * New Game: over the freshly booted district this just starts playing (and writes the first
	 * save); anywhere else the district is reopened clean.
	 */
	UFUNCTION(BlueprintCallable, Category = "Main menu")
	void MainMenuNewGame();

	// --- Safehouse ------------------------------------------------------------------------------

	/** The safehouse menu. UHawkeyeSafehouseWidget by default. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Safehouse")
	TSubclassOf<UHawkeyeSafehouseWidget> SafehouseWidgetClass;

	/** ASafehouse calls this after healing and saving: pauses under the safehouse menu. */
	UFUNCTION(BlueprintCallable, Category = "Safehouse")
	void OpenSafehouseMenu(ASafehouse* Safehouse);

	UFUNCTION(BlueprintCallable, Category = "Safehouse")
	void CloseSafehouseMenu();

	UFUNCTION(BlueprintPure, Category = "Safehouse")
	bool IsSafehouseMenuOpen() const { return bSafehouseMenuOpen; }

	UFUNCTION(BlueprintPure, Category = "Safehouse")
	UHawkeyeSafehouseWidget* GetSafehouseWidget() const { return SafehouseWidget; }

	/** Refill arrows: every filled quiver slot of the pawn being played to its cap. */
	UFUNCTION(BlueprintCallable, Category = "Safehouse")
	void SafehouseRefill();

	/** Save: writes the campaign now. */
	UFUNCTION(BlueprintCallable, Category = "Safehouse")
	void SafehouseSave();

	/** Fast travel: lists the other discovered safehouses. A stub until there is a second one. */
	UFUNCTION(BlueprintCallable, Category = "Safehouse")
	void SafehouseFastTravel();

	/** Chapter select: a stub while CH01 is the only chapter. */
	UFUNCTION(BlueprintCallable, Category = "Safehouse")
	void SafehouseChapterSelect();

	// --- Input device -----------------------------------------------------------------------

	/**
	 * True from the moment any Gamepad_* key is pressed until the next mouse or keyboard key,
	 * so the HUD can swap "[F]" style hints for a gamepad glyph. Updated from every InputKey
	 * event, not just ones a Blueprint or Enhanced Input action happens to bind.
	 */
	UFUNCTION(BlueprintPure, Category = "Input")
	bool IsUsingGamepad() const { return bUsingGamepad; }

	// --- Partner and switching (claude-docs/gameplay-semantics.md) -----------------------------

	/**
	 * Swaps control to the other Hawkeye: possesses the partner's pawn, hands the one you were
	 * playing to the partner controller, rebinds the HUD (quiver, name) and blends the camera over
	 * SwitchBlendSeconds. False, and nothing changes, when GetSwitchRefusal has a reason.
	 */
	UFUNCTION(BlueprintCallable, Category = "Partner")
	bool SwitchCharacter();

	/**
	 * Why a switch would be refused now, or empty when it would go ahead: the chapter does not allow
	 * it, there is no partner, or either Hawkeye is mid-traversal, mid-zip, mid-takedown or down.
	 */
	UFUNCTION(BlueprintPure, Category = "Partner")
	FString GetSwitchRefusal() const;

	/** The chapter's bAllowSwitching, or bAllowSwitchingOverride. */
	UFUNCTION(BlueprintPure, Category = "Partner")
	bool IsSwitchingAllowed() const;

	/** The partner controller backing up this player's pawn (any partner with a pawn if none leads it). */
	UFUNCTION(BlueprintPure, Category = "Partner")
	AHawkeyePartnerController* FindPartnerController() const;

	/**
	 * T: traces from the camera up to MarkTraceDistance and sends the partner to the point it hits.
	 * False when there is no partner or nothing under the view.
	 */
	UFUNCTION(BlueprintCallable, Category = "Partner")
	bool MarkPoint();

	/** The name the HUD shows for the character being played; updated by every switch. */
	UFUNCTION(BlueprintPure, Category = "Partner")
	FText GetHudCharacterName() const;

	UFUNCTION(BlueprintPure, Category = "Partner")
	UBanterComponent* GetBanter() const { return Banter; }

	/** X on the keyboard, LB on a pad. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Partner")
	TObjectPtr<UInputAction> SwitchCharacterAction;

	/** T on the keyboard. No pad binding yet (the D-pad is the quiver's). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Partner")
	TObjectPtr<UInputAction> PartnerMarkAction;

	/** How long the camera takes to move from one Hawkeye to the other. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Partner", meta = (ClampMin = "0.0"))
	float SwitchBlendSeconds = 0.3f;

	/** How far the mark trace looks, cm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Partner", meta = (ClampMin = "100.0"))
	float MarkTraceDistance = 5000.f;

	/** Allows switching whatever the chapter says: debug and automation tests. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Partner")
	bool bAllowSwitchingOverride = false;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;

	void Input_Pause(const FInputActionValue& Value);
	void Input_SwitchCharacter(const FInputActionValue& Value);
	void Input_PartnerMark(const FInputActionValue& Value);

	/** Kate and Clint's banter, shown on the HUD. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Partner")
	TObjectPtr<UBanterComponent> Banter;

	/** Set by SwitchCharacter; empty until the first switch, when the pawn's own name is used. */
	UPROPERTY(Transient)
	FText ActiveCharacterName;

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

	UFUNCTION()
	void HandlePauseQuitToMenuClicked();

	UFUNCTION()
	void HandleMainMenuSettingsClicked();

	UFUNCTION()
	void HandleMainMenuQuitClicked();

	/** Creates the main menu widget and binds it, once. */
	UHawkeyeMainMenuWidget* EnsureMainMenuWidget();

	/** Creates the safehouse widget and binds it, once. */
	UHawkeyeSafehouseWidget* EnsureSafehouseWidget();

	void SetSafehouseStatus(const FText& Status);

	/** Creates PauseWidget (if needed) and adds it to the viewport. Returns the widget or null. */
	UHawkeyePauseWidget* ShowPauseWidget();

	void HidePauseWidget();

	/** Creates SettingsWidget (if needed) and adds it to the viewport. Returns the widget or null. */
	UHawkeyeSettingsWidget* ShowSettingsWidget();

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
	TObjectPtr<UHawkeyeHudWidget> HudWidget = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Flashback")
	TObjectPtr<UFlashbackWidget> ActiveFlashbackWidget = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Pause")
	TObjectPtr<UHawkeyePauseWidget> PauseWidget = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Settings")
	TObjectPtr<UHawkeyeSettingsWidget> SettingsWidget = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UHawkeyeInventoryWidget> InventoryWidget = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Inventory")
	bool bInventoryOpen = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Main menu")
	TObjectPtr<UHawkeyeMainMenuWidget> MainMenuWidget = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Main menu")
	bool bMainMenuOpen = false;

	/** The menu is up over the district as it booted, untouched: New Game can just start playing. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Main menu")
	bool bMainMenuOverFreshBoot = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Safehouse")
	TObjectPtr<UHawkeyeSafehouseWidget> SafehouseWidget = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Safehouse")
	TObjectPtr<ASafehouse> ActiveSafehouse = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Safehouse")
	bool bSafehouseMenuOpen = false;

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

	/** Last device seen by InputKey: true for a Gamepad_* key, false for mouse or keyboard. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Input")
	bool bUsingGamepad = false;
};

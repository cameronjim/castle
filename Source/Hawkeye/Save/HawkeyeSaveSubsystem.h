// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Save/HawkeyeAutosaveClock.h"
#include "Save/HawkeyeCampaignState.h"
#include "HawkeyeSaveSubsystem.generated.h"

class USpudSubsystem;
class UWorld;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHawkeyeCampaignSavedSignature, const FString&, Reason, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHawkeyeCampaignLoadedSignature, bool, bSuccess);

/**
 * The campaign save: one slot, "HawkeyeCampaign", written by SPUD (Plugins/SPUD, sinbad/SPUD).
 *
 * SPUD stores every ISpudObject actor in the level (Kate and Clint: transform, health, quiver;
 * thugs: position, dead or alive) and the global UHawkeyeCampaignState (chapter, objectives,
 * safehouses). This subsystem decides when to save (objective complete, safehouse, 60 s of calm
 * roaming), whether a save may be loaded (version, stale chapter), and hands the campaign state
 * to the mission once the reloaded map has started it.
 *
 * SPUD also carries level state across any map travel by default; this game only wants explicit
 * saves, so every travel that is not a SPUD load starts SPUD afresh (see HandlePreLoadMap).
 * Rules: claude-docs/gameplay-semantics.md, "Save data".
 */
UCLASS()
class HAWKEYE_API UHawkeyeSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** The subsystem for the world this object lives in, or nullptr outside a game instance. */
	static UHawkeyeSaveSubsystem* Get(const UObject* WorldContextObject);

	/** The one campaign slot, unless SlotNameOverride is set. */
	static const TCHAR* DefaultSlotName;

	//~ Begin USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem interface

	/** Set by tests so the player's campaign is never touched. Empty in game. */
	UPROPERTY(Transient)
	FString SlotNameOverride;

	UFUNCTION(BlueprintPure, Category = "Save")
	FString GetSlotName() const;

	/** True when the slot has a save file (whether or not it will load). Drives the menu's Continue. */
	UFUNCTION(BlueprintPure, Category = "Save")
	bool HasSave() const;

	/**
	 * Writes the campaign now. Refused (false, logged) outside a game world, while SPUD is busy or
	 * a load is pending, with no mission running, or while the player is dead or down.
	 */
	UFUNCTION(BlueprintCallable, Category = "Save")
	bool SaveCampaign(const FString& Reason);

	/**
	 * Continue: loads the slot if DecideLoad says so, otherwise starts a new game. Asynchronous:
	 * the map reloads and OnCampaignLoaded fires once it is restored. True when a load started.
	 */
	UFUNCTION(BlueprintCallable, Category = "Save")
	bool LoadCampaign();

	/** Forgets the campaign state and reopens the district fresh. The old save stays until overwritten. */
	UFUNCTION(BlueprintCallable, Category = "Save")
	void StartNewGame();

	/**
	 * EvaluateLoad plus the log line: a stale chapter is an error (the save points at content that
	 * no longer exists), a version with no migration a warning.
	 */
	EHawkeyeLoadDecision DecideLoad(bool bSaveExists, int32 SavedVersion, const FString& InMissionPath) const;

	/** True from LoadCampaign until the restored world is ready. */
	UFUNCTION(BlueprintPure, Category = "Save")
	bool IsLoading() const;

	/**
	 * AHawkeyeGameMode calls this once the reloaded map has started its mission: completes the
	 * saved objectives silently. Does nothing unless a load is being restored.
	 */
	void ApplyPendingRestore(UWorld* World);

	/** True between LoadCampaign or StartNewGame and the next world starting: a travel is coming. */
	bool IsTravelPending() const { return bLoadInFlight || bNewGameAutosavePending; }

	/** True once after StartNewGame, so the new game's first autosave can be written. */
	bool ConsumeNewGameAutosave();

	// --- Safehouses -------------------------------------------------------------------------------

	/** Records a safehouse as found. True the first time. */
	UFUNCTION(BlueprintCallable, Category = "Save|Safehouse")
	bool DiscoverSafehouse(FName SafehouseId);

	UFUNCTION(BlueprintPure, Category = "Save|Safehouse")
	bool IsSafehouseDiscovered(FName SafehouseId) const;

	UFUNCTION(BlueprintPure, Category = "Save|Safehouse")
	TArray<FName> GetDiscoveredSafehouses() const;

	// --- Autosave ---------------------------------------------------------------------------------

	/**
	 * Called by the game mode about once a second of unpaused play. bCanSave is false in a fight,
	 * while down, dead or off the ground; the roaming autosave fires after IntervalSeconds of true.
	 */
	void AdvanceAutosave(float DeltaSeconds, bool bCanSave);

	UFUNCTION(BlueprintPure, Category = "Save")
	float GetAutosaveRoamingSeconds() const { return AutosaveClock.GetRoamingSeconds(); }

	/** Seconds of calm roaming between autosaves. */
	UFUNCTION(BlueprintCallable, Category = "Save")
	void SetAutosaveInterval(float Seconds) { AutosaveClock.IntervalSeconds = FMath::Max(1.f, Seconds); }

	// --- Main menu --------------------------------------------------------------------------------

	/**
	 * True exactly once per game instance, for the first world that boots: the main menu shows over
	 * it. False for automation runs (-ExecCmds with RunTests, or -NoMainMenu) and after a load.
	 */
	bool ConsumeBootMenu();

	UFUNCTION(BlueprintPure, Category = "Save")
	UHawkeyeCampaignState* GetCampaignState() const { return Campaign; }

	/** Seconds the screen takes to come back from black after a load. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save", meta = (ClampMin = "0.0"))
	float FadeInSeconds = 1.f;

	UPROPERTY(BlueprintAssignable, Category = "Save")
	FOnHawkeyeCampaignSavedSignature OnCampaignSaved;

	UPROPERTY(BlueprintAssignable, Category = "Save")
	FOnHawkeyeCampaignLoadedSignature OnCampaignLoaded;

	/** Saves written this session; tests and the log use it. */
	UFUNCTION(BlueprintPure, Category = "Save")
	int32 GetSaveCount() const { return SaveCount; }

private:
	USpudSubsystem* GetSpud() const;

	/** The game world this instance is playing, or null. */
	UWorld* GetGameWorld() const;

	/** Why the player cannot be saved right now, or empty. */
	FString GetSaveRefusal(UWorld* World) const;

	/** Copies the mission's state and who is being played into Campaign. */
	void CaptureCampaign(UWorld* World);

	void HandlePreLoadMap(const FString& MapName);

	UFUNCTION()
	void HandleSpudPostLoad(const FString& SlotName, bool bSuccess);

	UFUNCTION()
	void HandleSpudPostSave(const FString& SlotName, bool bSuccess);

	/** After a load: hand control to whoever was being played, and bring the picture back. */
	void FinishRestoredWorld(UWorld* World);

	UPROPERTY(Transient)
	TObjectPtr<UHawkeyeCampaignState> Campaign;

	UPROPERTY(Transient)
	FHawkeyeAutosaveClock AutosaveClock;

	FDelegateHandle PreLoadMapHandle;
	bool bRestorePending = false;
	bool bLoadInFlight = false;
	bool bBootMenuConsumed = false;
	bool bNewGameAutosavePending = false;
	bool bLastSaveSucceeded = false;
	int32 SaveCount = 0;
};

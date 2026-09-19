// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/BossPhaseComponent.h"
#include "Combat/WeaponDefinition.h"
#include "CastlePlayerController.h"
#include "Combat/Takedownable.h"
#include "Player/CastleCharacter.h"
#include "Settings/CastleSettings.h"
#include "World/GuardCharacter.h"
#include "GameFramework/Actor.h"
#include "UObject/Object.h"
#include "UObject/Script.h"
#include "CastleTestUtils.generated.h"

class UFlashbackDefinition;
class UHealthComponent;
class UInventoryComponent;
class UMissionDefinition;
class UMissionObjective;
class UMissionTracker;

/**
 * RAII test world. Automation tests only need one when a component requires a real actor owner;
 * everything else is exercised with NewObject and no world at all.
 */
struct CASTLE_API FCastleTestWorld
{
	FCastleTestWorld();
	~FCastleTestWorld();

	FCastleTestWorld(const FCastleTestWorld&) = delete;
	FCastleTestWorld& operator=(const FCastleTestWorld&) = delete;

	UWorld* Get() const { return World; }

	/** Spawns an actor of ActorClass at Location facing Forward's yaw. */
	AActor* SpawnActor(TSubclassOf<AActor> ActorClass, const FVector& Location, const FRotator& Rotation) const;

private:
	/**
	 * A world built by hand is not a standalone game world, so AActor::GetFunctionCallspace can
	 * decide an event belongs on a remote machine and ProcessEvent silently does nothing - which
	 * makes every ITakedownable BlueprintNativeEvent return its default. This guard forces local
	 * execution for as long as the test world lives.
	 */
	FEditorScriptExecutionGuard ScriptExecutionGuard;

	UWorld* World = nullptr;
};

/**
 * Counting sink for the project's dynamic multicast delegates. Dynamic delegates cannot bind
 * lambdas, so tests bind one of these and assert on the counters afterwards.
 */
UCLASS()
class CASTLE_API UCastleTestListener : public UObject
{
	GENERATED_BODY()

public:
	// --- Health ---------------------------------------------------------------------------------
	UPROPERTY() int32 HealthChangedCount = 0;
	UPROPERTY() float LastNewHealth = 0.f;
	UPROPERTY() float LastHealthDelta = 0.f;
	UPROPERTY() int32 DeathCount = 0;

	/** Who OnDeath said did it. A takedown that reports None is the bug this catches. */
	UPROPERTY() TObjectPtr<AActor> LastKiller = nullptr;

	UFUNCTION()
	void HandleHealthChanged(UHealthComponent* HealthComponent, float NewHealth, float Delta, AActor* DamageInstigator);

	UFUNCTION()
	void HandleDeath(UHealthComponent* HealthComponent, AActor* Killer);

	// --- Boss phases ----------------------------------------------------------------------------
	UPROPERTY() int32 PhaseChangedCount = 0;
	UPROPERTY() int32 LastOldPhaseIndex = INDEX_NONE;
	UPROPERTY() int32 LastNewPhaseIndex = INDEX_NONE;
	UPROPERTY() int32 TransitionFinishedCount = 0;

	/** Set this and the listener records that component's invulnerability at each phase change. */
	UPROPERTY() TObjectPtr<UHealthComponent> WatchedHealth = nullptr;

	UPROPERTY() bool bWatchedHealthInvulnerableAtPhaseChange = false;

	UFUNCTION()
	void HandlePhaseChanged(int32 OldPhaseIndex, int32 NewPhaseIndex, FBossPhase Phase);

	UFUNCTION()
	void HandleTransitionFinished(int32 PhaseIndex);

	// --- Weapon ---------------------------------------------------------------------------------
	UPROPERTY() int32 AmmoChangedCount = 0;
	UPROPERTY() int32 LastMagazine = 0;
	UPROPERTY() int32 LastReserve = 0;
	UPROPERTY() int32 EmptyClickCount = 0;

	UFUNCTION()
	void HandleAmmoChanged(int32 CurrentAmmo, int32 ReserveAmmo);

	UFUNCTION()
	void HandleEmptyClick();

	UPROPERTY() int32 WeaponHitCount = 0;
	UPROPERTY() TObjectPtr<AActor> LastWeaponHitActor = nullptr;
	UPROPERTY() float LastWeaponHitDamage = 0.f;

	UFUNCTION()
	void HandleWeaponHit(AActor* HitActor, float DamageDealt);

	// --- Inventory ------------------------------------------------------------------------------
	UPROPERTY() int32 InventoryChangedCount = 0;
	UPROPERTY() int32 ActiveSlotChangedCount = 0;
	UPROPERTY() EHotbarSlot LastOldHotbarSlot = EHotbarSlot::Hands;
	UPROPERTY() EHotbarSlot LastNewHotbarSlot = EHotbarSlot::Hands;

	UFUNCTION()
	void HandleInventoryChanged();

	UFUNCTION()
	void HandleActiveSlotChanged(EHotbarSlot OldSlot, EHotbarSlot NewSlot);

	// --- Stagger --------------------------------------------------------------------------------
	UPROPERTY() int32 StaggeredCount = 0;

	UFUNCTION()
	void HandleStaggered(UHealthComponent* HealthComponent, AActor* DamageInstigator);

	// --- Takedown -------------------------------------------------------------------------------
	UPROPERTY() int32 TakedownCount = 0;
	UPROPERTY() TObjectPtr<AActor> LastTakedownTarget = nullptr;

	UFUNCTION()
	void HandleTakedownPerformed(AActor* Target);

	// --- Guard ---------------------------------------------------------------------------------
	UPROPERTY() int32 AlertStateChangedCount = 0;
	UPROPERTY() EGuardAlertState LastOldAlertState = EGuardAlertState::Calm;
	UPROPERTY() EGuardAlertState LastNewAlertState = EGuardAlertState::Calm;

	UFUNCTION()
	void HandleAlertStateChanged(EGuardAlertState OldState, EGuardAlertState NewState);

	// --- Mission --------------------------------------------------------------------------------
	/** Set by the test so HandleMissionStarted can assert the objectives already exist. */
	UPROPERTY() TObjectPtr<UMissionTracker> WatchedTracker = nullptr;

	UPROPERTY() int32 MissionStartedCount = 0;
	UPROPERTY() TObjectPtr<UMissionDefinition> LastStartedMission = nullptr;

	/** True when GetCurrentObjective() already returned an objective inside OnMissionStarted. */
	UPROPERTY() bool bCurrentObjectiveSetAtMissionStart = false;

	UPROPERTY() int32 ObjectiveUpdatedCount = 0;
	UPROPERTY() int32 LastObjectiveIndex = INDEX_NONE;
	UPROPERTY() TObjectPtr<UMissionObjective> LastObjective = nullptr;
	UPROPERTY() int32 MissionCompleteCount = 0;

	/**
	 * Set this and OnMissionComplete clears it, which is what ACastleGameMode does for real.
	 * It lets the "nothing carries between missions" rule be tested without a game mode.
	 */
	UPROPERTY() TObjectPtr<UInventoryComponent> InventoryToClearOnMissionComplete = nullptr;

	UPROPERTY() int32 FlashbackRequestedCount = 0;

	/** Set when OnFlashbackRequested arrives while MissionCompleteCount is already 1. */
	UPROPERTY() bool bFlashbackFollowedMissionComplete = false;

	UFUNCTION()
	void HandleMissionStarted(UMissionDefinition* Mission);

	UFUNCTION()
	void HandleObjectiveUpdated(UMissionObjective* Objective, int32 ObjectiveIndex);

	UFUNCTION()
	void HandleMissionComplete(UMissionDefinition* Mission);

	UFUNCTION()
	void HandleFlashbackRequested(UFlashbackDefinition* Flashback);

	// --- Settings -------------------------------------------------------------------------------
	UPROPERTY() int32 SettingsChangedCount = 0;
	UPROPERTY() float LastLookSensitivity = 0.f;

	UFUNCTION()
	void HandleSettingsChanged(FCastleSettings Settings);

	// --- Flashback widget -----------------------------------------------------------------------
	UPROPERTY() int32 FlashbackFinishedCount = 0;

	UFUNCTION()
	void HandleFlashbackFinished(UFlashbackDefinition* Flashback);
};

/**
 * Frank with his protected aim state opened up. Sprinting and the FOV blend are driven by the
 * input handlers and Tick in the real game, neither of which a headless test can run.
 */
UCLASS()
class CASTLE_API ACastleAimTestCharacter : public ACastleCharacter
{
	GENERATED_BODY()

public:
	/** Stands in for Input_SprintStarted / Input_SprintCompleted. */
	void TestSetSprinting(bool bInSprinting);

	/** Stands in for one Tick of the aim FOV blend. */
	void TestTickAim(float DeltaSeconds) { UpdateAimFOV(DeltaSeconds); }

	float TestWalkSpeed() const { return WalkSpeed; }
	float TestSprintSpeed() const { return SprintSpeed; }
	float TestAimSpeedMultiplier() const { return AimSpeedMultiplier; }
	float TestAimFOV() const { return AimFOV; }
	float TestHipFOV() const { return HipFOV; }
	float TestAimBlendSeconds() const { return AimBlendSeconds; }

	float MaxWalkSpeed() const;

	/** Stands in for UCastleSettingsSubsystem::OnSettingsChanged arriving. */
	void TestApplySettings(const FCastleSettings& Settings) { HandleSettingsChanged(Settings); }

	float TestLookSensitivity() const { return LookSensitivity; }
	float TestAimLookMultiplier() const { return AimLookMultiplier; }
	bool TestHasSettingsSensitivity() const { return bHasSettingsLookSensitivity; }
};

/**
 * ACastlePlayerController with the flashback flag opened up, so a test can assert that Escape
 * is ignored while the slideshow owns the pause without building a UFlashbackWidget.
 */
UCLASS()
class CASTLE_API ACastlePauseTestController : public ACastlePlayerController
{
	GENERATED_BODY()

public:
	void TestSetFlashbackActive(bool bActive) { bFlashbackActive = bActive; }
};

/** Minimal ITakedownable actor for takedown tests. */
UCLASS()
class CASTLE_API ACastleTestTakedownTarget : public AActor, public ITakedownable
{
	GENERATED_BODY()

public:
	/** Mirrors a guard's AI state: guards return false from CanBeTakenDown while Alerted. */
	UPROPERTY()
	bool bAlerted = false;

	UPROPERTY()
	int32 TakedownReceivedCount = 0;

	virtual bool CanBeTakenDown_Implementation(AActor* Attacker) override { return !bAlerted; }
	virtual void OnTakedown_Implementation(AActor* Attacker) override { ++TakedownReceivedCount; }
};

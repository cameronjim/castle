// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Takedownable.h"
#include "GameFramework/Character.h"
#include "GuardCharacter.generated.h"

class APickupActor;
class UAnimSequence;
class UHealthComponent;
class USpotLightComponent;
class UWeaponComponent;

/** What a guard currently believes about the player. See claude-docs/gameplay-semantics.md. */
UENUM(BlueprintType)
enum class EGuardAlertState : uint8
{
	/** Patrolling. Takedown valid, does not shoot. */
	Calm,
	/** Heard or half-saw something. Investigates. Takedown still valid. */
	Suspicious,
	/** Has confirmed the player. Shoots. Takedown refused. */
	Alerted
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAlertStateChangedSignature, EGuardAlertState, OldState, EGuardAlertState, NewState);

/**
 * A prison guard: 100 HP, a pistol, an alert state, and a body that drops what it was carrying.
 *
 * AGuardAIController drives the state; this class owns the state itself so a Blueprint, a
 * takedown or a bullet can all read and change it without knowing about the controller.
 */
UCLASS(Blueprintable, BlueprintType)
class CASTLE_API AGuardCharacter : public ACharacter, public ITakedownable
{
	GENERATED_BODY()

public:
	AGuardCharacter();

	UFUNCTION(BlueprintPure, Category = "Guard")
	UHealthComponent* GetHealthComponent() const { return HealthComponent; }

	UFUNCTION(BlueprintPure, Category = "Guard")
	UWeaponComponent* GetWeaponComponent() const { return WeaponComponent; }

	UFUNCTION(BlueprintPure, Category = "Guard")
	EGuardAlertState GetAlertState() const { return AlertState; }

	/** Sets the alert state and broadcasts OnAlertStateChanged when it actually changed. */
	UFUNCTION(BlueprintCallable, Category = "Guard")
	void SetAlertState(EGuardAlertState NewState);

	UFUNCTION(BlueprintPure, Category = "Guard")
	bool IsAlerted() const { return AlertState == EGuardAlertState::Alerted; }

	/** Points this guard walks between while Calm. Place ATargetPoints and fill this in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Guard|Patrol")
	TArray<TObjectPtr<AActor>> PatrolPoints;

	/** Seconds spent standing at each patrol point before moving on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Guard|Patrol", meta = (ClampMin = "0.0"))
	float PatrolWaitSeconds = 2.f;

	/**
	 * Pickups dropped when this guard goes down, by takedown or by bullet. Set per instance in
	 * the level (the first guard carries the pistol and the keycard), not on the class.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Guard|Loot")
	TArray<TSubclassOf<APickupActor>> DropOnDeath;

	UPROPERTY(BlueprintAssignable, Category = "Guard")
	FOnAlertStateChangedSignature OnAlertStateChanged;

	/** Spawns everything in DropOnDeath around the guard's feet. Only ever runs once. */
	UFUNCTION(BlueprintCallable, Category = "Guard|Loot")
	void DropLoot();

	/**
	 * Ragdolls the mesh, disables the capsule and stops the AI. Safe to call twice.
	 *
	 * When the mesh cannot ragdoll - no physics asset, or one whose bodies do not match the
	 * skeleton, which makes InitArticulated find no root body and SetSimulatePhysics a no-op -
	 * the guard falls over procedurally instead. Killer only steers which way he goes down.
	 */
	UFUNCTION(BlueprintCallable, Category = "Guard")
	void GoLimp(AActor* Killer);

	/** True once GoLimp has run, whichever way the body went down. */
	UFUNCTION(BlueprintPure, Category = "Guard")
	bool IsLimp() const { return bLimp; }

	/** True when the mesh is actually simulating physics, i.e. the real ragdoll path took. */
	UFUNCTION(BlueprintPure, Category = "Guard")
	bool IsRagdolling() const;

	/** True while the procedural fallback is tipping the body over. */
	UFUNCTION(BlueprintPure, Category = "Guard")
	bool IsCollapsing() const { return bCollapsing; }

	/** 0 upright, 1 flat on the floor. Stays 1 once the collapse has finished. */
	UFUNCTION(BlueprintPure, Category = "Guard")
	float GetCollapseAlpha() const;

	// --- Animation ------------------------------------------------------------------------------

	/**
	 * Plays Idle or Walk depending on ground speed, and only when the choice changes.
	 *
	 * The mannequin pack's AnimBP does not compile headless, so guards drove nothing and stood
	 * in a T-pose. Two sequences on the mesh's single-node animation slot is all a patrolling
	 * guard needs, and it is something a test can assert on.
	 */
	UFUNCTION(BlueprintCallable, Category = "Guard|Animation")
	void UpdateLocomotionAnimation();

	/** WalkAnim above WalkAnimSpeedThreshold of ground speed, IdleAnim below it. */
	UFUNCTION(BlueprintPure, Category = "Guard|Animation")
	UAnimSequence* SelectLocomotionAnim() const;

	/** The sequence the mesh is currently playing, or null. */
	UFUNCTION(BlueprintPure, Category = "Guard|Animation")
	UAnimSequence* GetCurrentLocomotionAnim() const { return CurrentLocomotionAnim; }

	UFUNCTION(BlueprintPure, Category = "Guard|Components")
	USpotLightComponent* GetFlashlight() const { return Flashlight; }

	/** Idle pose. Assigned in BP_Guard from the mannequin pack. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Guard|Animation")
	TObjectPtr<UAnimSequence> IdleAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Guard|Animation")
	TObjectPtr<UAnimSequence> WalkAnim;

	/** Optional. Only used when the mesh has no physics asset to ragdoll with. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Guard|Animation")
	TObjectPtr<UAnimSequence> DeathAnim;

	/** Ground speed in uu/s above which the guard is walking rather than standing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Guard|Animation", meta = (ClampMin = "0.0"))
	float WalkAnimSpeedThreshold = 20.f;

	//~ Begin ITakedownable interface
	virtual bool CanBeTakenDown_Implementation(AActor* Attacker) override;
	virtual void OnTakedown_Implementation(AActor* Attacker) override;
	//~ End ITakedownable interface

protected:
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION()
	void HandleDeath(UHealthComponent* Health, AActor* Killer);

	/** Snaps the flashlight to the head socket when the mesh has one. Runs once at BeginPlay. */
	void AttachFlashlight();

	/** Starts the no-physics fallback: tip the mesh over and drop it, away from the killer. */
	void BeginProceduralCollapse(AActor* Killer);

	/** Writes one frame of the collapse onto the mesh. Called from Tick while bCollapsing. */
	void UpdateProceduralCollapse(float DeltaSeconds);

	/** Seconds the procedural collapse takes from upright to flat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Guard|Death", meta = (ClampMin = "0.01"))
	float CollapseSeconds = 0.6f;

	/** How far the body tips. 90 would be exactly flat; 85 leaves it looking dropped, not laid out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Guard|Death", meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float CollapsePitchDegrees = 85.f;

	/** How far the mesh sinks over the collapse, so the body ends up on the floor not above it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Guard|Death", meta = (ClampMin = "0.0"))
	float CollapseDropDistance = 20.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Guard|Components")
	TObjectPtr<UHealthComponent> HealthComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Guard|Components")
	TObjectPtr<UWeaponComponent> WeaponComponent;

	/**
	 * Head torch. It reads as black-site security, and it doubles as the stealth tell: the cone
	 * is where the guard is looking, so the player can route around it.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Guard|Components")
	TObjectPtr<USpotLightComponent> Flashlight;

	/** Socket the flashlight hangs off when the mesh has one; otherwise it sits on the capsule. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard|Components")
	FName FlashlightSocketName = FName(TEXT("head"));

	/** Whichever of IdleAnim / WalkAnim is playing, so Tick only re-plays on a real change. */
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> CurrentLocomotionAnim;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Guard")
	EGuardAlertState AlertState = EGuardAlertState::Calm;

	/** Offset applied to each dropped pickup so two drops do not land inside each other. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard|Loot")
	float DropSpacing = 40.f;

private:
	bool bLootDropped = false;
	bool bLimp = false;

	/** Procedural collapse state. Only used when the mesh refused to simulate. */
	bool bCollapsing = false;
	float CollapseElapsed = 0.f;

	/** Where the mesh was standing when the collapse started; every frame is built off this. */
	FVector CollapseStartLocation = FVector::ZeroVector;
	FQuat CollapseStartRotation = FQuat::Identity;

	/** World axis the body rotates about, so his head goes down away from whoever killed him. */
	FVector CollapseAxis = FVector::RightVector;
};

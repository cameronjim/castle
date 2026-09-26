// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Takedownable.h"
#include "GameFramework/Character.h"
#include "ThugCharacter.generated.h"

class APickupActor;
class UAnimSequence;
class UHealthComponent;
class USpotLightComponent;
class UWeaponComponent;

/** What a thug currently believes about the player. See claude-docs/gameplay-semantics.md. */
UENUM(BlueprintType)
enum class EThugAlertState : uint8
{
	/** Patrolling. Takedown valid, does not shoot. */
	Calm,
	/** Heard or half-saw something. Investigates. Takedown still valid. */
	Suspicious,
	/** Has confirmed the player. Shoots. Takedown refused. */
	Alerted
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAlertStateChangedSignature, EThugAlertState, OldState, EThugAlertState, NewState);

/**
 * A street thug: 100 HP, a hitscan pistol, an alert state, and a body that drops what it was
 * carrying. TODO(stage2): melee rush, gunner, archer and heavy variants are planned.
 *
 * AThugAIController drives the state; this class owns the state itself so a Blueprint, a
 * takedown or a bullet can all read and change it without knowing about the controller.
 */
UCLASS(Blueprintable, BlueprintType)
class CASTLE_API AThugCharacter : public ACharacter, public ITakedownable
{
	GENERATED_BODY()

public:
	AThugCharacter();

	UFUNCTION(BlueprintPure, Category = "Thug")
	UHealthComponent* GetHealthComponent() const { return HealthComponent; }

	UFUNCTION(BlueprintPure, Category = "Thug")
	UWeaponComponent* GetWeaponComponent() const { return WeaponComponent; }

	UFUNCTION(BlueprintPure, Category = "Thug")
	EThugAlertState GetAlertState() const { return AlertState; }

	/** Sets the alert state and broadcasts OnAlertStateChanged when it actually changed. */
	UFUNCTION(BlueprintCallable, Category = "Thug")
	void SetAlertState(EThugAlertState NewState);

	UFUNCTION(BlueprintPure, Category = "Thug")
	bool IsAlerted() const { return AlertState == EThugAlertState::Alerted; }

	/**
	 * A hit he survives (an arrow, a punch): a brief stagger. He stops, is shoved HitShoveSpeed
	 * away from HitBy, his AI holds off for StaggerSeconds, and he is Alerted if he was not.
	 * Bound to the health component's OnStaggered, so every staggering hit arrives here.
	 */
	UFUNCTION(BlueprintCallable, Category = "Thug")
	void HitReaction(AActor* HitBy);

	/** True for StaggerSeconds after a HitReaction. The AI does nothing while it is. */
	UFUNCTION(BlueprintPure, Category = "Thug")
	bool IsStaggered() const { return StaggerRemaining > 0.f; }

	/** How long a hit reaction holds him, seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thug|Hit", meta = (ClampMin = "0.0"))
	float StaggerSeconds = 0.5f;

	/** Speed of the shove away from whoever hit him, cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thug|Hit", meta = (ClampMin = "0.0"))
	float HitShoveSpeed = 250.f;

	/** Points this thug walks between while Calm. Place ATargetPoints and fill this in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thug|Patrol")
	TArray<TObjectPtr<AActor>> PatrolPoints;

	/** Seconds spent standing at each patrol point before moving on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thug|Patrol", meta = (ClampMin = "0.0"))
	float PatrolWaitSeconds = 2.f;

	/**
	 * Pickups dropped when this thug goes down, by takedown or by bullet. Set per instance in
	 * the level (the first thug carries the keycard), not on the class.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thug|Loot")
	TArray<TSubclassOf<APickupActor>> DropOnDeath;

	UPROPERTY(BlueprintAssignable, Category = "Thug")
	FOnAlertStateChangedSignature OnAlertStateChanged;

	/** Spawns everything in DropOnDeath around the thug's feet. Only ever runs once. */
	UFUNCTION(BlueprintCallable, Category = "Thug|Loot")
	void DropLoot();

	/**
	 * Ragdolls the mesh, disables the capsule and stops the AI. Safe to call twice.
	 *
	 * When the mesh cannot ragdoll - no physics asset, or one whose bodies do not match the
	 * skeleton, which makes InitArticulated find no root body and SetSimulatePhysics a no-op -
	 * the thug falls over procedurally instead. Killer only steers which way he goes down.
	 */
	UFUNCTION(BlueprintCallable, Category = "Thug")
	void GoLimp(AActor* Killer);

	/** True once GoLimp has run, whichever way the body went down. */
	UFUNCTION(BlueprintPure, Category = "Thug")
	bool IsLimp() const { return bLimp; }

	/** True when the mesh is actually simulating physics, i.e. the real ragdoll path took. */
	UFUNCTION(BlueprintPure, Category = "Thug")
	bool IsRagdolling() const;

	/** True while the procedural fallback is tipping the body over. */
	UFUNCTION(BlueprintPure, Category = "Thug")
	bool IsCollapsing() const { return bCollapsing; }

	/** 0 upright, 1 flat on the floor. Stays 1 once the collapse has finished. */
	UFUNCTION(BlueprintPure, Category = "Thug")
	float GetCollapseAlpha() const;

	// --- Animation ------------------------------------------------------------------------------

	/**
	 * Plays Idle or Walk depending on ground speed, and only when the choice changes.
	 *
	 * The mannequin pack's AnimBP does not compile headless, so thugs drove nothing and stood
	 * in a T-pose. Two sequences on the mesh's single-node animation slot is all a patrolling
	 * thug needs, and it is something a test can assert on.
	 */
	UFUNCTION(BlueprintCallable, Category = "Thug|Animation")
	void UpdateLocomotionAnimation();

	/** WalkAnim above WalkAnimSpeedThreshold of ground speed, IdleAnim below it. */
	UFUNCTION(BlueprintPure, Category = "Thug|Animation")
	UAnimSequence* SelectLocomotionAnim() const;

	/** The sequence the mesh is currently playing, or null. */
	UFUNCTION(BlueprintPure, Category = "Thug|Animation")
	UAnimSequence* GetCurrentLocomotionAnim() const { return CurrentLocomotionAnim; }

	UFUNCTION(BlueprintPure, Category = "Thug|Components")
	USpotLightComponent* GetFlashlight() const { return Flashlight; }

	/** Idle pose. Assigned in BP_Thug from the mannequin pack. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thug|Animation")
	TObjectPtr<UAnimSequence> IdleAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thug|Animation")
	TObjectPtr<UAnimSequence> WalkAnim;

	/** Optional. Only used when the mesh has no physics asset to ragdoll with. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thug|Animation")
	TObjectPtr<UAnimSequence> DeathAnim;

	/** Ground speed in uu/s above which the thug is walking rather than standing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thug|Animation", meta = (ClampMin = "0.0"))
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

	UFUNCTION()
	void HandleStaggered(UHealthComponent* Health, AActor* DamageInstigator);

	/** Snaps the flashlight to the head socket when the mesh has one. Runs once at BeginPlay. */
	void AttachFlashlight();

	/** Starts the no-physics fallback: tip the mesh over and drop it, away from the killer. */
	void BeginProceduralCollapse(AActor* Killer);

	/** Writes one frame of the collapse onto the mesh. Called from Tick while bCollapsing. */
	void UpdateProceduralCollapse(float DeltaSeconds);

	/** Seconds the procedural collapse takes from upright to flat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thug|Death", meta = (ClampMin = "0.01"))
	float CollapseSeconds = 0.6f;

	/** How far the body tips. 90 would be exactly flat; 85 leaves it looking dropped, not laid out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thug|Death", meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float CollapsePitchDegrees = 85.f;

	/** How far the mesh sinks over the collapse, so the body ends up on the floor not above it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thug|Death", meta = (ClampMin = "0.0"))
	float CollapseDropDistance = 20.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Thug|Components")
	TObjectPtr<UHealthComponent> HealthComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Thug|Components")
	TObjectPtr<UWeaponComponent> WeaponComponent;

	/**
	 * Head torch. Kept from the prison build; it doubles as the stealth tell: the cone
	 * is where the thug is looking, so the player can route around it.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Thug|Components")
	TObjectPtr<USpotLightComponent> Flashlight;

	/** Socket the flashlight hangs off when the mesh has one; otherwise it sits on the capsule. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Thug|Components")
	FName FlashlightSocketName = FName(TEXT("head"));

	/** Whichever of IdleAnim / WalkAnim is playing, so Tick only re-plays on a real change. */
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> CurrentLocomotionAnim;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Thug")
	EThugAlertState AlertState = EThugAlertState::Calm;

	/** Offset applied to each dropped pickup so two drops do not land inside each other. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Thug|Loot")
	float DropSpacing = 40.f;

private:
	/** Seconds of stagger left. */
	float StaggerRemaining = 0.f;

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

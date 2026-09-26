// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/MeleeComponent.h"
#include "Combat/Takedownable.h"
#include "GameFramework/Character.h"
#include "ThugCharacter.generated.h"

class APickupActor;
class UAnimSequence;
class UHealthComponent;
class UMaterialInstanceDynamic;
class UStaticMesh;
class UStaticMeshComponent;
class UWeaponComponent;

/** What a thug currently believes about the player. See claude-docs/gameplay-semantics.md. */
UENUM(BlueprintType)
enum class EThugAlertState : uint8
{
	/** Patrolling. Takedown valid, does not shoot. */
	Calm,
	/** Heard or half-saw something. Investigates. Takedown still valid. */
	Suspicious,
	/** Has confirmed the player. Attacks. Takedown refused. */
	Alerted
};

/** What a thug fights with. Only the Pistol thug shoots; Fists and Bat rush in and swing. */
UENUM(BlueprintType)
enum class EThugWeapon : uint8
{
	Fists,
	Bat,
	/** The gunner: keeps the hitscan pistol, 12 a hit. */
	Pistol
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAlertStateChangedSignature, EThugAlertState, OldState, EThugAlertState, NewState);

/**
 * A Tracksuit thug: 100 HP, a weapon (fists, a bat or the gunner's pistol), an alert state, and a
 * body that drops what it was carrying. Punches and arrows stagger him, Kate's heavy knocks him
 * down for KnockdownSeconds, and he ragdolls when he dies. TODO(stage3): archer and heavy variants.
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

	/** The swing the Fists and Bat thugs use. */
	UFUNCTION(BlueprintPure, Category = "Thug")
	UMeleeComponent* GetMeleeComponent() const { return MeleeComponent; }

	UFUNCTION(BlueprintPure, Category = "Thug")
	EThugAlertState GetAlertState() const { return AlertState; }

	/** Sets the alert state and broadcasts OnAlertStateChanged when it actually changed. */
	UFUNCTION(BlueprintCallable, Category = "Thug")
	void SetAlertState(EThugAlertState NewState);

	UFUNCTION(BlueprintPure, Category = "Thug")
	bool IsAlerted() const { return AlertState == EThugAlertState::Alerted; }

	// --- Combat ---------------------------------------------------------------------------------

	/** Fists, Bat or Pistol. Set per placed thug; the generator sets the district's four. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thug|Combat")
	EThugWeapon Weapon = EThugWeapon::Pistol;

	/** True for the Pistol thug: he keeps his distance and shoots instead of rushing. */
	UFUNCTION(BlueprintPure, Category = "Thug|Combat")
	bool IsGunner() const { return Weapon == EThugWeapon::Pistol; }

	/** The swing for his weapon: BatAttack for a Bat thug, FistsAttack otherwise. */
	UFUNCTION(BlueprintPure, Category = "Thug|Combat")
	FCastleMeleeAttack GetMeleeAttack() const { return Weapon == EThugWeapon::Bat ? BatAttack : FistsAttack; }

	/** 15 damage after a 0.6 s telegraphed wind-up, then 0.6 s to recover. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thug|Combat")
	FCastleMeleeAttack FistsAttack;

	/** 25 damage after the same wind-up, a little more reach. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thug|Combat")
	FCastleMeleeAttack BatAttack;

	/** The bat he carries; shown in his right hand only when Weapon is Bat. Assigned in BP_Thug. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thug|Combat")
	TObjectPtr<UStaticMesh> BatMesh;

	/** Shows the bat for a Bat thug and hides it for anyone else. Runs at BeginPlay. */
	UFUNCTION(BlueprintCallable, Category = "Thug|Combat")
	void RefreshHeldWeapon();

	UFUNCTION(BlueprintPure, Category = "Thug|Components")
	UStaticMeshComponent* GetHeldWeaponComponent() const { return HeldWeaponComponent; }

	// --- Hit reactions --------------------------------------------------------------------------

	/**
	 * A hit he survives (an arrow, a punch): a brief stagger. He stops, is shoved HitShoveSpeed
	 * away from HitBy, drops any swing he was winding up, his AI holds off for StaggerSeconds, and
	 * he is Alerted if he was not. Bound to the health component's OnStaggered.
	 */
	UFUNCTION(BlueprintCallable, Category = "Thug")
	void HitReaction(AActor* HitBy);

	/** True for StaggerSeconds after a HitReaction. The AI does nothing while it is. */
	UFUNCTION(BlueprintPure, Category = "Thug")
	bool IsStaggered() const { return StaggerRemaining > 0.f; }

	/**
	 * Kate's heavy: he goes over (a ragdoll when the mesh can, otherwise only the state), stays down
	 * KnockdownSeconds, then stands back up where his body landed. His swing is dropped, his AI
	 * holds off, and he is Alerted. There is no get-up animation; standing up is a snap.
	 */
	UFUNCTION(BlueprintCallable, Category = "Thug")
	void Knockdown(AActor* By);

	UFUNCTION(BlueprintPure, Category = "Thug")
	bool IsKnockedDown() const { return bKnockedDown; }

	/** Staggered or on the floor: the AI does nothing. */
	UFUNCTION(BlueprintPure, Category = "Thug")
	bool IsIncapacitated() const { return IsStaggered() || bKnockedDown; }

	/** Counts a knockdown down and stands him up at the end. Called from Tick; public for tests. */
	UFUNCTION(BlueprintCallable, Category = "Thug")
	void UpdateKnockdown(float DeltaSeconds);

	/** How long a hit reaction holds him, seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thug|Hit", meta = (ClampMin = "0.0"))
	float StaggerSeconds = 0.5f;

	/** Speed of the shove away from whoever hit him, cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thug|Hit", meta = (ClampMin = "0.0"))
	float HitShoveSpeed = 250.f;

	/** Seconds a heavy keeps him on the floor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thug|Hit", meta = (ClampMin = "0.0"))
	float KnockdownSeconds = 3.f;

	/** Speed the body is thrown away from the hit at, cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thug|Hit", meta = (ClampMin = "0.0"))
	float KnockdownLaunchSpeed = 450.f;

	/** Pulses the body's emissive for HitFlashSeconds. Every hit that costs him health calls it. */
	UFUNCTION(BlueprintCallable, Category = "Thug|Hit")
	void FlashHit();

	/** 1 the instant he is hit, fading to 0 over HitFlashSeconds. */
	UFUNCTION(BlueprintPure, Category = "Thug|Hit")
	float GetHitFlashAlpha() const { return HitFlashSeconds > 0.f ? HitFlashRemaining / HitFlashSeconds : 0.f; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thug|Hit", meta = (ClampMin = "0.0"))
	float HitFlashSeconds = 0.1f;

	// --- Patrol and loot ------------------------------------------------------------------------

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
	 * Plays Idle, Walk or Run depending on ground speed, and only when the choice changes.
	 *
	 * The mannequin pack's AnimBP does not compile headless, so thugs drove nothing and stood
	 * in a T-pose. Sequences on the mesh's single-node animation slot are all a patrolling
	 * thug needs, and it is something a test can assert on.
	 */
	UFUNCTION(BlueprintCallable, Category = "Thug|Animation")
	void UpdateLocomotionAnimation();

	/** RunAnim above RunAnimSpeedThreshold, WalkAnim above WalkAnimSpeedThreshold, IdleAnim below. */
	UFUNCTION(BlueprintPure, Category = "Thug|Animation")
	UAnimSequence* SelectLocomotionAnim() const;

	/** The sequence the mesh is currently playing, or null. */
	UFUNCTION(BlueprintPure, Category = "Thug|Animation")
	UAnimSequence* GetCurrentLocomotionAnim() const { return CurrentLocomotionAnim; }

	/** Idle pose. Assigned in BP_Thug from the mannequin pack. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thug|Animation")
	TObjectPtr<UAnimSequence> IdleAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thug|Animation")
	TObjectPtr<UAnimSequence> WalkAnim;

	/** Played above RunAnimSpeedThreshold (the melee rush). Falls back to WalkAnim when unset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thug|Animation")
	TObjectPtr<UAnimSequence> RunAnim;

	/** Optional. Only used when the mesh has no physics asset to ragdoll with. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thug|Animation")
	TObjectPtr<UAnimSequence> DeathAnim;

	/** Ground speed in uu/s above which the thug is walking rather than standing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thug|Animation", meta = (ClampMin = "0.0"))
	float WalkAnimSpeedThreshold = 20.f;

	/** Ground speed in uu/s above which the rush plays RunAnim. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thug|Animation", meta = (ClampMin = "0.0"))
	float RunAnimSpeedThreshold = 350.f;

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

	UFUNCTION()
	void HandleHealthChanged(UHealthComponent* Health, float NewHealth, float Delta, AActor* DamageInstigator);

	/** One dynamic material instance per slot, so his flashes are his alone. Runs at BeginPlay. */
	void CreateBodyMaterials();

	/** Writes the hit flash and the swing telegraph into the body's materials. Called from Tick. */
	void UpdateMaterialPulse(float DeltaSeconds);

	/** Throws the mesh into a ragdoll for a knockdown. False when it cannot simulate. */
	bool BeginKnockdownRagdoll(AActor* By);

	/** Ends a knockdown: the capsule moves to where the body lies and the mesh snaps back upright. */
	void StandUp();

	/** Tells the brain (or, without one, the state) that By just hurt him. */
	void AlertTo(AActor* By);

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Thug|Components")
	TObjectPtr<UMeleeComponent> MeleeComponent;

	/** The bat, in the right hand. Empty and hidden unless Weapon is Bat. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Thug|Components")
	TObjectPtr<UStaticMeshComponent> HeldWeaponComponent;

	/** Socket the bat hangs off. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Thug|Components")
	FName HeldWeaponSocketName = FName(TEXT("hand_r"));

	/** Scalar parameter on the body material pulsed by FlashHit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Thug|Hit")
	FName HitFlashParameter = FName(TEXT("HitFlash"));

	/** Scalar parameter held at 1 through a swing's wind-up: the mask glows red. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Thug|Hit")
	FName TelegraphParameter = FName(TEXT("Telegraph"));

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> BodyMaterials;

	/** Whichever of IdleAnim / WalkAnim / RunAnim is playing, so Tick only re-plays on a real change. */
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

	bool bKnockedDown = false;
	bool bKnockdownRagdoll = false;
	float KnockdownRemaining = 0.f;
	float HitFlashRemaining = 0.f;

	/** What the materials were last given, so the parameters are only written on a change. */
	float LastFlashWritten = -1.f;
	float LastTelegraphWritten = -1.f;

	/** The mesh's mount on the capsule, restored when he stands back up. */
	FTransform MeshRelativeTransform = FTransform::Identity;

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

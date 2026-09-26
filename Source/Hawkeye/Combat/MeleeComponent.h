// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MeleeComponent.generated.h"

/** One swing: how hard, how slow, how far. Kate's light and heavy and each thug weapon are one of these. */
USTRUCT(BlueprintType)
struct HAWKEYE_API FHawkeyeMeleeAttack
{
	GENERATED_BODY()

	/** For the log and for listeners: "light", "heavy", "fists", "bat". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee")
	FName Name = FName(TEXT("light"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee", meta = (ClampMin = "0.0"))
	float Damage = 15.f;

	/** Seconds between starting the swing and the hit landing. The telegraph runs this long. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee", meta = (ClampMin = "0.0"))
	float WindupSeconds = 0.1f;

	/** Seconds after the hit before another swing can start. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee", meta = (ClampMin = "0.0"))
	float RecoverSeconds = 0.2f;

	/** How far in front of the owner's capsule centre the sweep reaches, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee", meta = (ClampMin = "0.0"))
	float Range = 120.f;

	/** Radius of the swept sphere, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee", meta = (ClampMin = "1.0"))
	float Radius = 35.f;

	/** Passes the hit through ApplyMeleeDamage with stagger on (above the target's StaggerThreshold). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee")
	bool bStagger = true;

	/** Puts a surviving thug on the floor (AThugCharacter::Knockdown) instead of staggering him. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee")
	bool bKnockdown = false;

	/** Procedural step forward at the start of the swing, cm. There is no attack animation yet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee", meta = (ClampMin = "0.0"))
	float LungeDistance = 0.f;

	/** Seconds the lunge takes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee", meta = (ClampMin = "0.01"))
	float LungeSeconds = 0.1f;

	/** Seconds from the start of the swing until another can start. */
	float GetTotalSeconds() const { return WindupSeconds + RecoverSeconds; }
};

UENUM(BlueprintType)
enum class EMeleePhase : uint8
{
	Idle,
	/** Telegraphing: the hit has not landed yet. */
	Windup,
	/** The hit has been swept; waiting out RecoverSeconds. */
	Recover
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMeleeWindupSignature, FName, AttackName, float, WindupSeconds);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnMeleeLandedSignature, AActor*, HitActor, float, DamageDealt, FName, AttackName);

/**
 * A swing: wind-up, one sphere sweep in front of the owner at the end of it, recovery. Shared by
 * Kate (light, heavy) and the thugs (fists, bat); the attack itself is data, an FHawkeyeMeleeAttack.
 *
 * The sweep looks for pawns only (object type Pawn), so the hidden traversal ledges on the Weapon
 * channel along every roof edge never eat a punch. Actors carrying IgnoreTag are never hit, which
 * is how a thug's bat misses the thug beside him.
 *
 * Timing runs from TickComponent; AdvanceAttack is public so a test can step it without a world tick.
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = (Hawkeye), meta = (BlueprintSpawnableComponent))
class HAWKEYE_API UMeleeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMeleeComponent();

	/** Starts Attack. Refused (false) while another swing is winding up or recovering. */
	UFUNCTION(BlueprintCallable, Category = "Melee")
	bool StartAttack(const FHawkeyeMeleeAttack& Attack);

	/** Drops the current swing without a hit: a stagger or a knockdown interrupts the wind-up. */
	UFUNCTION(BlueprintCallable, Category = "Melee")
	void CancelAttack();

	/** Moves the current swing on by DeltaSeconds; the sweep runs the moment the wind-up ends. */
	UFUNCTION(BlueprintCallable, Category = "Melee")
	void AdvanceAttack(float DeltaSeconds);

	UFUNCTION(BlueprintPure, Category = "Melee")
	bool IsAttacking() const { return Phase != EMeleePhase::Idle; }

	UFUNCTION(BlueprintPure, Category = "Melee")
	bool IsWindingUp() const { return Phase == EMeleePhase::Windup; }

	UFUNCTION(BlueprintPure, Category = "Melee")
	EMeleePhase GetPhase() const { return Phase; }

	/** The swing in progress (or the last one). */
	UFUNCTION(BlueprintPure, Category = "Melee")
	const FHawkeyeMeleeAttack& GetCurrentAttack() const { return CurrentAttack; }

	/** Seconds left in the current phase. */
	UFUNCTION(BlueprintPure, Category = "Melee")
	float GetPhaseRemaining() const { return PhaseRemaining; }

	/**
	 * The first living pawn with a health component, other than the owner and anything tagged
	 * IgnoreTag, in a sphere of Attack.Radius swept Attack.Range forward from the owner. Null if none.
	 */
	UFUNCTION(BlueprintCallable, Category = "Melee")
	AActor* FindTarget(const FHawkeyeMeleeAttack& Attack) const;

	/** Actors with this tag are never hit. Thugs set "Thug". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee")
	FName IgnoreTag;

	/** Fired as a swing starts: the telegraph. */
	UPROPERTY(BlueprintAssignable, Category = "Melee")
	FOnMeleeWindupSignature OnAttackWindup;

	/** Fired when the sweep at the end of the wind-up found something and hurt it. */
	UPROPERTY(BlueprintAssignable, Category = "Melee")
	FOnMeleeLandedSignature OnAttackLanded;

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** The end of the wind-up: sweep, damage, stagger or knockdown. */
	void Strike();

	/** The procedural step into the swing, as a root motion force on a character owner. */
	void ApplyLunge(const FHawkeyeMeleeAttack& Attack) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Melee")
	EMeleePhase Phase = EMeleePhase::Idle;

	UPROPERTY(Transient)
	FHawkeyeMeleeAttack CurrentAttack;

	UPROPERTY(Transient)
	float PhaseRemaining = 0.f;
};

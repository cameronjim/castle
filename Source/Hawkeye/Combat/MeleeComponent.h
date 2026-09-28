// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/CombatAnimSet.h"
#include "MeleeComponent.generated.h"

class UAnimInstance;
class UAnimMontage;
class UAnimSequenceBase;
class UMotionWarpingComponent;
class USkeletalMeshComponent;
class USoundBase;

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

	/** Shoves whoever it lands on this far away from the swinger, cm (the heavy's bash: 250). 0 for none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee", meta = (ClampMin = "0.0"))
	float KnockbackDistance = 0.f;

	/** Seconds the knockback takes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee", meta = (ClampMin = "0.01"))
	float KnockbackSeconds = 0.35f;

	/**
	 * Which clip of the owner's UCombatAnimSet plays this swing. None, an empty slot, or a clip that
	 * will not play keeps the procedural swing (the strike pose and the lunge).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee|Animation")
	ECombatAnimRole AnimRole = ECombatAnimRole::None;

	/** Procedural step forward at the start of the swing, cm. Skipped when a clip with root motion moves the owner. */
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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMeleeMissedSignature, FName, AttackName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMeleeComboWindowSignature, bool, bOpen);

/**
 * A swing: wind-up, one sphere sweep in front of the owner at the end of it, recovery. Shared by
 * Kate (light, heavy) and the thugs (fists, bat); the attack itself is data, an FHawkeyeMeleeAttack.
 *
 * The sweep looks for pawns only (object type Pawn), so the hidden traversal ledges on the Weapon
 * channel along every roof edge never eat a punch. Actors carrying IgnoreTag are never hit, which
 * is how a thug's bat misses the thug beside him.
 *
 * Timing runs from TickComponent; AdvanceAttack is public so a test can step it without a world tick.
 *
 * With a clip (claude-docs/gameplay-semantics.md, "Combat animation clips"): when the attack's
 * AnimRole resolves in the anim set and the montage plays, the swing is timed by the montage's
 * ANS_HitWindow and ANS_ComboWindow instead of the attack's timers (a window it lacks keeps its
 * timer), a root-motion clip is warped toward the target given by SetNextAttackTarget, and the
 * lunge only runs when the clip does not move the owner.
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = (Hawkeye), meta = (BlueprintSpawnableComponent))
class HAWKEYE_API UMeleeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMeleeComponent();

	/**
	 * Starts Attack. Refused (false) while another swing is winding up or recovering, except inside a
	 * clip's combo window, where the new swing cuts the old one.
	 */
	UFUNCTION(BlueprintCallable, Category = "Melee")
	bool StartAttack(const FHawkeyeMeleeAttack& Attack);

	/** True when StartAttack would take a swing now: idle, or inside the current clip's combo window. */
	UFUNCTION(BlueprintPure, Category = "Melee")
	bool CanStartAttack() const;

	/** The clips the owner's swings use. Null, or an empty role, keeps the procedural swing. */
	UFUNCTION(BlueprintCallable, Category = "Melee|Animation")
	void SetAnimSet(UCombatAnimSet* InAnimSet) { AnimSet = InAnimSet; }

	UFUNCTION(BlueprintPure, Category = "Melee|Animation")
	UCombatAnimSet* GetAnimSet() const { return AnimSet; }

	/** The next StartAttack is aimed at Target: a root-motion clip warps to him. Null clears it. */
	UFUNCTION(BlueprintCallable, Category = "Melee|Animation")
	void SetNextAttackTarget(AActor* Target) { PendingTarget = Target; }

	/** The montage the current (or last) swing plays, or null on the procedural path. */
	UFUNCTION(BlueprintPure, Category = "Melee|Animation")
	UAnimMontage* GetCurrentMontage() const { return CurrentMontage; }

	/** True when the current swing's hit comes from ANS_HitWindow rather than WindupSeconds. */
	UFUNCTION(BlueprintPure, Category = "Melee|Animation")
	bool IsHitFromNotify() const { return bHitFromNotify; }

	/** True when the current swing's chain and recovery come from ANS_ComboWindow rather than the timers. */
	UFUNCTION(BlueprintPure, Category = "Melee|Animation")
	bool IsComboFromNotify() const { return bComboFromNotify; }

	UFUNCTION(BlueprintPure, Category = "Melee|Animation")
	bool IsHitWindowOpen() const { return bHitWindowOpen; }

	UFUNCTION(BlueprintPure, Category = "Melee|Animation")
	bool IsComboWindowOpen() const { return bComboWindowOpen; }

	/** ANS_HitWindow opened on Animation: the swing lands (or starts sweeping). Ignored for another montage. */
	void NotifyHitWindowBegin(const UAnimSequenceBase* Animation);

	/** ANS_HitWindow closed: a swing that found nobody missed. */
	void NotifyHitWindowEnd(const UAnimSequenceBase* Animation);

	/** ANS_ComboWindow opened: the next swing may start. */
	void NotifyComboWindowBegin(const UAnimSequenceBase* Animation);

	/** ANS_ComboWindow closed. */
	void NotifyComboWindowEnd(const UAnimSequenceBase* Animation);

	/** The current swing's montage ended (or was cut): the swing is over. */
	void NotifyMontageEnded(bool bInterrupted);

	/**
	 * Tests: the next StartAttack is timed by the notify calls as if its montage carried both windows,
	 * without playing anything (a test world has no anim instance).
	 */
	void ForceNotifyTimingForTest() { bForceNotifyTimingForTest = true; }

	/**
	 * The next StartAttack lunges and sweeps along Direction (flattened) instead of the owner's forward,
	 * for the whole swing: the soft lock turns the body over 0.1 s, but the blow goes at the target at once.
	 * Zero clears it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Melee")
	void SetNextAttackDirection(FVector Direction) { PendingDirection = Direction.GetSafeNormal2D(); }

	/** The way the current swing goes: the direction it was given, else the owner's forward. */
	UFUNCTION(BlueprintPure, Category = "Melee")
	FVector GetSwingDirection() const;

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

	/**
	 * The knockback's force (cm/s, flat) on a target at To hit from From: Distance over Seconds, pointing away
	 * from From. Pure.
	 */
	static FVector ComputeKnockbackForce(const FVector& From, const FVector& To, float Distance, float Seconds);

	/** Pushes Target (a character) Attack.KnockbackDistance away from the owner as a root motion force. */
	void ApplyKnockback(AActor* Target, const FHawkeyeMeleeAttack& Attack) const;

	/** Swings the target's shield took. */
	UFUNCTION(BlueprintPure, Category = "Melee")
	int32 GetBlockedCount() const { return BlockedCount; }

	/** Actors with this tag are never hit. Thugs set "Thug". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee")
	FName IgnoreTag;

	/**
	 * The telegraph: played as a wind-up of TelegraphMinWindup or longer starts (the thugs' swings and
	 * the heavy's bash), so the tell is heard as well as seen (MS_Thug_Telegraph).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee|Audio")
	TSoftObjectPtr<USoundBase> WindupSound;

	/** Wind-ups shorter than this are not telegraphed (Kate's 0.1 s jab). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee|Audio", meta = (ClampMin = "0.0"))
	float TelegraphMinWindup = 0.3f;

	/** The swing through the air, hit or miss (MS_Thug_BatSwing on the thugs). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee|Audio")
	TSoftObjectPtr<USoundBase> SwingSound;

	/** A strike that lands (MS_Melee_Punch). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee|Audio")
	TSoftObjectPtr<USoundBase> HitSound;

	/** A knockdown strike that lands (Kate's heavy, MS_Melee_Heavy). Falls back to HitSound. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee|Audio")
	TSoftObjectPtr<USoundBase> HeavyHitSound;

	/** Fired as a swing starts: the telegraph. */
	UPROPERTY(BlueprintAssignable, Category = "Melee")
	FOnMeleeWindupSignature OnAttackWindup;

	/** Fired when the sweep at the end of the wind-up found something and hurt it. */
	UPROPERTY(BlueprintAssignable, Category = "Melee")
	FOnMeleeLandedSignature OnAttackLanded;

	/** Fired when the sweep found nobody, or a shield took it. */
	UPROPERTY(BlueprintAssignable, Category = "Melee")
	FOnMeleeMissedSignature OnAttackMissed;

	/** A clip's combo window opened (true) or closed (false; also when its swing ends or is cancelled). */
	UPROPERTY(BlueprintAssignable, Category = "Melee|Animation")
	FOnMeleeComboWindowSignature OnComboWindowChanged;

	/** The motion warping target a strike clip is warped to. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee|Animation")
	FName WarpTargetName = FName(TEXT("CombatTarget"));

	/** The warp target stands this far short of the target, toward the owner, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee|Animation", meta = (ClampMin = "0.0"))
	float WarpStandOffDistance = 90.f;

	/**
	 * Play each clip at the rate that opens its ANS_HitWindow at the attack's WindupSeconds, so the
	 * telegraph keeps its length (thugs). Off: clips play at rate 1 (Kate).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee|Animation")
	bool bFitClipToWindup = false;

	/** A clip-timed swing ends on its own this long after its montage should have, s (notifies that never came). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee|Animation", meta = (ClampMin = "0.0"))
	float ClipTimeoutPadding = 0.5f;

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** The end of the wind-up: sweep, damage, stagger or knockdown, or a miss. */
	void Strike();

	/**
	 * One sweep and its consequences. False when nobody was in reach (nothing broadcast); true when
	 * the swing is settled: it landed, a shield took it, or the target could not be hurt.
	 */
	bool ResolveSweep();

	/** Plays the attack's clip when it has one that will play, and takes the swing's timing from it. */
	bool TryPlaySwingClip(const FHawkeyeMeleeAttack& Attack);

	/** Points the owner's motion warping at the swing's target (or clears it) for a root-motion clip. */
	void UpdateWarpTarget() const;

	/** The owner's motion warping component, made on first use when it has none. */
	UMotionWarpingComponent* FindOrAddMotionWarping() const;

	/** Steps a swing whose current phase a clip times; true when it did, so the timers are skipped. */
	bool AdvanceClipSwing(float DeltaSeconds);

	/** Ends a clip-timed swing: a hit left open is a miss, an open combo window closes. */
	void EndClipSwing(bool bInterrupted);

	/** Closes the combo window if it is open, telling listeners. */
	void CloseComboWindow();

	/** Forgets the current clip and its windows, quietly (a new swing is taking over). */
	void ResetClipState();

	/** True when Animation is the current swing's montage, or there is none to compare with (tests). */
	bool IsCurrentClip(const UAnimSequenceBase* Animation) const;

	/** The procedural step into the swing, as a root motion force on a character owner. */
	void ApplyLunge(const FHawkeyeMeleeAttack& Attack) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Melee")
	EMeleePhase Phase = EMeleePhase::Idle;

	UPROPERTY(Transient)
	FHawkeyeMeleeAttack CurrentAttack;

	UPROPERTY(Transient)
	float PhaseRemaining = 0.f;

	int32 BlockedCount = 0;

	/** SetNextAttackDirection's value, waiting for the next swing, and the one the current swing took. */
	FVector PendingDirection = FVector::ZeroVector;
	FVector SwingDirection = FVector::ZeroVector;

	UPROPERTY(Transient)
	TObjectPtr<UCombatAnimSet> AnimSet;

	/** The current swing's clip and the instance it plays on. */
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> CurrentMontage;

	TWeakObjectPtr<UAnimInstance> MontageInstance;

	/** SetNextAttackTarget's value, and the one the current swing took. */
	TWeakObjectPtr<AActor> PendingTarget;
	TWeakObjectPtr<AActor> SwingTarget;

	bool bHitFromNotify = false;
	bool bComboFromNotify = false;
	bool bHitWindowOpen = false;
	bool bComboWindowOpen = false;

	/** The hit window found someone, or ended without: the swing's hit is settled. */
	bool bSwingResolved = false;

	/** The clip moves the owner (root motion on the main instance), so no lunge. */
	bool bClipMovesOwner = false;

	bool bForceNotifyTimingForTest = false;

	/** Seconds until a clip-timed swing gives up on its notifies. */
	float ClipTimeRemaining = 0.f;

	/** Counts swings, so a cut montage's end event is not taken for the swing that cut it. */
	int32 SwingSerial = 0;
};

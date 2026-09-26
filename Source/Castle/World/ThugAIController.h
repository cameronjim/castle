// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "World/ThugCharacter.h"
#include "ThugAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Hearing;
class UAISenseConfig_Sight;

/** Where a stimulus came from, so tests can drive the state machine without a perception system. */
UENUM(BlueprintType)
enum class EStimulusKind : uint8
{
	Sight,
	Hearing
};

/**
 * Thug brain. A plain C++ state machine on a 0.25 s think timer plus perception callbacks -
 * no behavior tree, because three states that a test can drive directly is worth more right now
 * than a tree nobody can assert on. Stage 3 replaces this with BT_Thug.
 *
 * Calm       patrol PatrolPoints, waiting PatrolWaitSeconds at each
 * Suspicious walk to the last stimulus, wait InvestigateSeconds, back to Calm
 * Alerted    by weapon. The gunner (EThugWeapon::Pistol) faces the player and fires every
 *            FireInterval inside EngageRange, closing in otherwise. Fists and Bat rush in at
 *            RushSpeed; inside MeleeEngageRange (250) they wind up a swing and keep closing to
 *            MeleeCloseDistance (120), swing again after MeleeCooldownSeconds, and back off
 *            MeleeBackOffDistance after every MeleeSwingsBeforeBackOff swings.
 *            Drops to Suspicious after LoseTargetSeconds with no perception.
 */
UCLASS(Blueprintable, BlueprintType)
class CASTLE_API AThugAIController : public AAIController
{
	GENERATED_BODY()

public:
	AThugAIController();

	// --- Perception tuning ----------------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Thug|Sight", meta = (ClampMin = "0.0"))
	float SightRadius = 1500.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Thug|Sight", meta = (ClampMin = "0.0"))
	float LoseSightRadius = 1800.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Thug|Sight", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float SightHalfAngleDegrees = 35.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Thug|Hearing", meta = (ClampMin = "0.0"))
	float HearingRange = 1200.f;

	// --- Behaviour tuning -----------------------------------------------------------------------

	/** Seconds the player must stay visible before Suspicious becomes Alerted. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Thug|Behaviour", meta = (ClampMin = "0.0"))
	float SightConfirmSeconds = 0.6f;

	/** Seconds spent at the stimulus location before giving up and going back to Calm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Thug|Behaviour", meta = (ClampMin = "0.0"))
	float InvestigateSeconds = 4.f;

	/** Seconds without perceiving the player before Alerted drops back to Suspicious. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Thug|Behaviour", meta = (ClampMin = "0.0"))
	float LoseTargetSeconds = 5.f;

	/** Seconds between shots while Alerted. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Thug|Behaviour", meta = (ClampMin = "0.01"))
	float FireInterval = 0.9f;

	/** Half-angle of the random cone the thug's shots are scattered into. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Thug|Behaviour", meta = (ClampMin = "0.0"))
	float AimSpreadDegrees = 6.f;

	/** Inside this the thug shoots instead of closing the distance. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Thug|Behaviour", meta = (ClampMin = "0.0"))
	float EngageRange = 1200.f;

	// --- Melee rush (Fists and Bat) --------------------------------------------------------------

	/** Walking speed while Alerted with a melee weapon, cm/s. Calm and Suspicious walk at the pawn's own speed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Thug|Melee", meta = (ClampMin = "0.0"))
	float RushSpeed = 450.f;

	/** Inside this he commits: the swing's wind-up starts while he is still closing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Thug|Melee", meta = (ClampMin = "0.0"))
	float MeleeEngageRange = 250.f;

	/** How close he gets before he stops and lets the swing land, centre to centre. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Thug|Melee", meta = (ClampMin = "0.0"))
	float MeleeCloseDistance = 120.f;

	/** Seconds after a swing ends before the next can start. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Thug|Melee", meta = (ClampMin = "0.0"))
	float MeleeCooldownSeconds = 1.2f;

	/** Swings in a row before he steps back. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Thug|Melee", meta = (ClampMin = "1"))
	int32 MeleeSwingsBeforeBackOff = 2;

	/** How far he steps back after MeleeSwingsBeforeBackOff swings, cm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Thug|Melee", meta = (ClampMin = "0.0"))
	float MeleeBackOffDistance = 200.f;

	/** The longest a back-off lasts before he comes again, arrived or not. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Thug|Melee", meta = (ClampMin = "0.0"))
	float MeleeBackOffMaxSeconds = 1.5f;

	/** True while he is stepping back after a pair of swings. */
	UFUNCTION(BlueprintPure, Category = "Thug|Melee")
	bool IsBackingOff() const { return bBackingOff; }

	/** Swings since the last back-off. */
	UFUNCTION(BlueprintPure, Category = "Thug|Melee")
	int32 GetSwingsSinceBackOff() const { return SwingsSinceBackOff; }

	/** How often the state machine runs. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Thug|Behaviour", meta = (ClampMin = "0.01"))
	float ThinkIntervalSeconds = 0.25f;

	/** Loudness at or above which a heard noise counts as a gunshot and alerts immediately. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Thug|Hearing", meta = (ClampMin = "0.0"))
	float GunshotLoudnessThreshold = 2.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Thug")
	TObjectPtr<UAIPerceptionComponent> ThugPerception;

	/**
	 * The one entry point into the state machine. The perception callback funnels into this, and
	 * tests call it directly so the whole thing is exercisable without a perception system.
	 *
	 * Loudness is only meaningful for Hearing; a value at or above GunshotLoudnessThreshold is
	 * treated as a gunshot and alerts the thug on the spot.
	 */
	UFUNCTION(BlueprintCallable, Category = "Thug")
	void ReportStimulus(EStimulusKind Kind, FVector Location, bool bSuccessful, float Loudness = 1.f);

	/** Runs one step of the state machine. Called on a timer; tests call it directly. */
	UFUNCTION(BlueprintCallable, Category = "Thug")
	void Think(float DeltaSeconds);

	UFUNCTION(BlueprintPure, Category = "Thug")
	AThugCharacter* GetThug() const;

	UFUNCTION(BlueprintPure, Category = "Thug")
	EThugAlertState GetAlertState() const;

	/** Last place a stimulus came from; where a Suspicious thug walks to. */
	UFUNCTION(BlueprintPure, Category = "Thug")
	FVector GetLastStimulusLocation() const { return LastStimulusLocation; }

	/** The pawn the thug is attacking while Alerted, or nullptr. */
	UFUNCTION(BlueprintPure, Category = "Thug")
	AActor* GetTarget() const { return TargetActor; }

	/** Points him at Target, as a sighting would. A hit from the player does this too. */
	UFUNCTION(BlueprintCallable, Category = "Thug")
	void SetTarget(AActor* Target) { TargetActor = Target; }

	/**
	 * Off: the think timer stops, he stands where he is, and perception is ignored. For scripted
	 * moments and the screenshot pass. On: the state machine picks up where it left off.
	 */
	UFUNCTION(BlueprintCallable, Category = "Thug")
	void SetThinkingEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Thug")
	bool IsThinkingEnabled() const { return bThinkingEnabled; }

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	/** Timer body. */
	void TickThink();

	void TickCalm(float DeltaSeconds);
	void TickSuspicious(float DeltaSeconds);
	void TickAlerted(float DeltaSeconds);

	/** Moves to the current patrol point, advancing the index once it is reached. */
	void AdvancePatrol();

	/**
	 * MoveToActor / MoveToLocation with the failure reported. A thug that cannot move is
	 * almost always a missing navmesh rather than a broken state machine, and that used to be
	 * invisible in the log; this says so once per controller instead of every think tick.
	 */
	void RequestMoveToActor(AActor* Goal, float AcceptanceRadius);
	void RequestMoveToLocation(const FVector& Goal, float AcceptanceRadius);
	void ReportMoveResult(EPathFollowingRequestResult::Type Result, const FString& GoalDescription);

	/**
	 * Turns a stopped thug to face ToTarget. Only ever called when he is not moving: while he
	 * is, the movement component orients him along his own velocity so the walk cycle points the
	 * way he is going.
	 */
	static void FaceTarget(APawn* Thug, const FVector& ToTarget);

	/** One shot at TargetActor through the thug's weapon, scattered by AimSpreadDegrees. */
	void FireAtTarget();

	/** The gunner's Alerted step: close to EngageRange, then stand and shoot. */
	void TickGunner(float DeltaSeconds, const FVector& ToTarget);

	/** The Fists and Bat Alerted step: rush, wind up inside MeleeEngageRange, swing, back off. */
	void TickMeleeRush(float DeltaSeconds, const FVector& ToTarget);

	/** Counts a finished swing, starts the cooldown, and steps back after enough of them. */
	void FinishSwing(const FVector& ToTarget);

	/** RushSpeed while Alerted with a melee weapon, the pawn's own walking speed otherwise. */
	void ApplyMoveSpeed();

	void SetState(EThugAlertState NewState);

	UPROPERTY(Transient)
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UPROPERTY(Transient)
	TObjectPtr<UAISenseConfig_Hearing> HearingConfig;

	UPROPERTY(Transient)
	TObjectPtr<AActor> TargetActor = nullptr;

	UPROPERTY(Transient)
	FVector LastStimulusLocation = FVector::ZeroVector;

	/** Index into the thug's PatrolPoints. */
	UPROPERTY(Transient)
	int32 PatrolIndex = 0;

private:
	/** True while the player is inside the sight cone right now. */
	bool bSeesTarget = false;

	float SeenSeconds = 0.f;
	float UnseenSeconds = 0.f;
	float InvestigateElapsed = 0.f;
	float PatrolWaitElapsed = 0.f;
	float TimeSinceLastShot = 0.f;
	bool bPatrolWaiting = false;

	/** Latches after the first failed move so a broken navmesh logs once, not four times a second. */
	bool bLoggedMoveFailure = false;

	bool bThinkingEnabled = true;

	/** Melee rush state. */
	bool bWasSwinging = false;
	bool bBackingOff = false;
	int32 SwingsSinceBackOff = 0;
	float MeleeCooldownRemaining = 0.f;
	float BackOffElapsed = 0.f;

	/** The pawn's own MaxWalkSpeed, read at possession; the rush speed is laid over it. */
	float BaseWalkSpeed = 300.f;

	FTimerHandle ThinkTimerHandle;
};

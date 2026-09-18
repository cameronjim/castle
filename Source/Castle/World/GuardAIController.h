// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "World/GuardCharacter.h"
#include "GuardAIController.generated.h"

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
 * Guard brain. A plain C++ state machine on a 0.25 s think timer plus perception callbacks -
 * no behavior tree, because three states that a test can drive directly is worth more right now
 * than a tree nobody can assert on. Stage 3 replaces this with BT_Guard.
 *
 * Calm       patrol PatrolPoints, waiting PatrolWaitSeconds at each
 * Suspicious walk to the last stimulus, wait InvestigateSeconds, back to Calm
 * Alerted    face the player and fire every FireInterval inside EngageRange, close in otherwise;
 *            drops to Suspicious after LoseTargetSeconds with no perception
 */
UCLASS(Blueprintable, BlueprintType)
class CASTLE_API AGuardAIController : public AAIController
{
	GENERATED_BODY()

public:
	AGuardAIController();

	// --- Perception tuning ----------------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Guard|Sight", meta = (ClampMin = "0.0"))
	float SightRadius = 1500.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Guard|Sight", meta = (ClampMin = "0.0"))
	float LoseSightRadius = 1800.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Guard|Sight", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float SightHalfAngleDegrees = 35.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Guard|Hearing", meta = (ClampMin = "0.0"))
	float HearingRange = 1200.f;

	// --- Behaviour tuning -----------------------------------------------------------------------

	/** Seconds the player must stay visible before Suspicious becomes Alerted. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Guard|Behaviour", meta = (ClampMin = "0.0"))
	float SightConfirmSeconds = 0.6f;

	/** Seconds spent at the stimulus location before giving up and going back to Calm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Guard|Behaviour", meta = (ClampMin = "0.0"))
	float InvestigateSeconds = 4.f;

	/** Seconds without perceiving the player before Alerted drops back to Suspicious. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Guard|Behaviour", meta = (ClampMin = "0.0"))
	float LoseTargetSeconds = 5.f;

	/** Seconds between shots while Alerted. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Guard|Behaviour", meta = (ClampMin = "0.01"))
	float FireInterval = 0.9f;

	/** Half-angle of the random cone the guard's shots are scattered into. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Guard|Behaviour", meta = (ClampMin = "0.0"))
	float AimSpreadDegrees = 6.f;

	/** Inside this the guard shoots instead of closing the distance. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Guard|Behaviour", meta = (ClampMin = "0.0"))
	float EngageRange = 1200.f;

	/** How often the state machine runs. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Guard|Behaviour", meta = (ClampMin = "0.01"))
	float ThinkIntervalSeconds = 0.25f;

	/** Loudness at or above which a heard noise counts as a gunshot and alerts immediately. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Guard|Hearing", meta = (ClampMin = "0.0"))
	float GunshotLoudnessThreshold = 2.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Guard")
	TObjectPtr<UAIPerceptionComponent> GuardPerception;

	/**
	 * The one entry point into the state machine. The perception callback funnels into this, and
	 * tests call it directly so the whole thing is exercisable without a perception system.
	 *
	 * Loudness is only meaningful for Hearing; a value at or above GunshotLoudnessThreshold is
	 * treated as a gunshot and alerts the guard on the spot.
	 */
	UFUNCTION(BlueprintCallable, Category = "Guard")
	void ReportStimulus(EStimulusKind Kind, FVector Location, bool bSuccessful, float Loudness = 1.f);

	/** Runs one step of the state machine. Called on a timer; tests call it directly. */
	UFUNCTION(BlueprintCallable, Category = "Guard")
	void Think(float DeltaSeconds);

	UFUNCTION(BlueprintPure, Category = "Guard")
	AGuardCharacter* GetGuard() const;

	UFUNCTION(BlueprintPure, Category = "Guard")
	EGuardAlertState GetAlertState() const;

	/** Last place a stimulus came from; where a Suspicious guard walks to. */
	UFUNCTION(BlueprintPure, Category = "Guard")
	FVector GetLastStimulusLocation() const { return LastStimulusLocation; }

	/** The pawn the guard is shooting at while Alerted, or nullptr. */
	UFUNCTION(BlueprintPure, Category = "Guard")
	AActor* GetTarget() const { return TargetActor; }

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
	 * MoveToActor / MoveToLocation with the failure reported. A guard that cannot move is
	 * almost always a missing navmesh rather than a broken state machine, and that used to be
	 * invisible in the log; this says so once per controller instead of every think tick.
	 */
	void RequestMoveToActor(AActor* Goal, float AcceptanceRadius);
	void RequestMoveToLocation(const FVector& Goal, float AcceptanceRadius);
	void ReportMoveResult(EPathFollowingRequestResult::Type Result, const FString& GoalDescription);

	/** One shot at TargetActor through the guard's weapon, scattered by AimSpreadDegrees. */
	void FireAtTarget();

	void SetState(EGuardAlertState NewState);

	UPROPERTY(Transient)
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UPROPERTY(Transient)
	TObjectPtr<UAISenseConfig_Hearing> HearingConfig;

	UPROPERTY(Transient)
	TObjectPtr<AActor> TargetActor = nullptr;

	UPROPERTY(Transient)
	FVector LastStimulusLocation = FVector::ZeroVector;

	/** Index into the guard's PatrolPoints. */
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

	FTimerHandle ThinkTimerHandle;
};

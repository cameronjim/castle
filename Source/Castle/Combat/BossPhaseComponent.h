// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BossPhaseComponent.generated.h"

class UHealthComponent;

/** One stage of a boss fight. Author these in descending HealthThresholdPercent order. */
USTRUCT(BlueprintType)
struct CASTLE_API FBossPhase
{
	GENERATED_BODY()

	/** Designer-facing name ("Warden_Gunplay", "Warden_Melee"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase")
	FName PhaseName;

	/** Phase begins once health drops to or below this fraction of MaxHealth (1.0 = full health). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HealthThresholdPercent = 1.f;

	/** Ignore damage while transitioning into this phase (scripted animation, teleport, etc.). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase")
	bool bInvulnerableDuringTransition = true;

	/** Length of the transition before normal combat resumes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase", meta = (ClampMin = "0.0"))
	float TransitionSeconds = 2.f;

	/**
	 * Behaviour selector read by the AI. Set the same name as a blackboard value and branch on it in
	 * the behaviour tree (e.g. "Gun" -> ranged sub-tree, "Melee" -> chainsaw sub-tree).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase")
	FName BehaviorTag;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnBossPhaseChangedSignature, int32, OldPhaseIndex, int32, NewPhaseIndex, FBossPhase, Phase);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBossPhaseTransitionFinishedSignature, int32, PhaseIndex);

/**
 * Drives multi-phase boss fights off the owner's UHealthComponent.
 * Requires a UHealthComponent on the same actor.
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = (Castle), meta = (BlueprintSpawnableComponent))
class CASTLE_API UBossPhaseComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBossPhaseComponent();

	/** Ordered phases, highest threshold first. Index 0 is entered on BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss")
	TArray<FBossPhase> Phases;

	/** Broadcast the moment a new phase is entered; hook this up to swap the AI behaviour. */
	UPROPERTY(BlueprintAssignable, Category = "Boss")
	FOnBossPhaseChangedSignature OnPhaseChanged;

	/** Broadcast when the transition window ends and invulnerability is lifted. */
	UPROPERTY(BlueprintAssignable, Category = "Boss")
	FOnBossPhaseTransitionFinishedSignature OnPhaseTransitionFinished;

	UFUNCTION(BlueprintPure, Category = "Boss")
	int32 GetCurrentPhaseIndex() const { return CurrentPhaseIndex; }

	UFUNCTION(BlueprintPure, Category = "Boss")
	FName GetCurrentBehaviorTag() const;

	UFUNCTION(BlueprintPure, Category = "Boss")
	bool IsTransitioning() const { return bTransitioning; }

	/** Forces a phase regardless of health (debug / scripted beats). */
	UFUNCTION(BlueprintCallable, Category = "Boss")
	void EnterPhase(int32 PhaseIndex);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleHealthChanged(UHealthComponent* InHealthComponent, float NewHealth, float Delta, AActor* DamageInstigator);

	void FinishTransition();

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Boss")
	TObjectPtr<UHealthComponent> HealthComponent = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Boss")
	int32 CurrentPhaseIndex = INDEX_NONE;

private:
	bool bTransitioning = false;
	FTimerHandle TransitionTimerHandle;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TakedownComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTakedownPerformedSignature, AActor*, Target);

/**
 * Player-side stealth takedowns: finds a nearby actor tagged "Guard" that the player is standing
 * behind, and fires ITakedownable::OnTakedown on it.
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = (Castle), meta = (BlueprintSpawnableComponent))
class CASTLE_API UTakedownComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTakedownComponent();

	/** Maximum distance from the owner to a valid target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Takedown", meta = (ClampMin = "0.0"))
	float TakedownRange = 200.f;

	/** Radius of the sweep used to gather candidates. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Takedown", meta = (ClampMin = "0.0"))
	float TakedownRadius = 120.f;

	/**
	 * Half-angle, in degrees, of the cone behind the target that the attacker must stand in.
	 * 60 means the attacker must be within 60 degrees of directly behind the guard.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Takedown", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float MaxBehindAngleDegrees = 60.f;

	/** Actor tag a candidate must carry. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Takedown")
	FName TargetTag = FName(TEXT("Guard"));

	/** Object channel swept for candidates. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Takedown")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Pawn;

	/** Draws the sweep for a couple of seconds in non-shipping builds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Takedown|Debug")
	bool bDrawDebug = false;

	UPROPERTY(BlueprintAssignable, Category = "Takedown")
	FOnTakedownPerformedSignature OnTakedownPerformed;

	/** Attempts a takedown. Returns true when one was executed. */
	UFUNCTION(BlueprintCallable, Category = "Takedown")
	bool TryTakedown();

	/** Best current candidate, or nullptr. Useful for a "press E to take down" prompt. */
	UFUNCTION(BlueprintCallable, Category = "Takedown")
	AActor* FindTakedownTarget() const;

protected:
	/** True when Candidate is tagged, implements ITakedownable, allows it, and the owner is behind it. */
	bool IsValidTarget(const AActor* Candidate) const;
};

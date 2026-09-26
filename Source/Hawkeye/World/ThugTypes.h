// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ThugTypes.generated.h"

/**
 * What a thug's brain is doing. ST_Thug has one state per mode, tried in this priority order:
 * Stunned, Reposition, Cover, Attack, Investigate, then Patrol as the fallback. Without a tree
 * AThugAIController::ChooseMode picks the same way.
 */
UENUM(BlueprintType)
enum class EThugMode : uint8
{
	/** Held by putty, staggered, on the floor, dead or switched off: nothing. */
	Stunned,
	/** The gunner backing off to RetreatToDistance, or the archer zipping to another roof. */
	Reposition,
	/** The gunner moving to, or hiding at, a cover point out of the player's line. */
	Cover,
	/** Alerted: the swing, the burst or the draw, by weapon. */
	Attack,
	/** Suspicious: walk to the stimulus (an archer only turns to it), wait, give up. */
	Investigate,
	/** Calm: patrol points, or stand. */
	Patrol
};

/** Where a gunner is in his fight. */
UENUM(BlueprintType)
enum class EGunnerPhase : uint8
{
	/** Standing where he is and shooting: no cover yet, or none to be had. */
	Open,
	/** Going to or crouched at a cover point. */
	Covering,
	/** Stepped out of cover to a spot with a line on the player, for one burst. */
	Peeking
};

/** What an archer should do about the distance to his target. */
UENUM(BlueprintType)
enum class EArcherRangeAction : uint8
{
	/** In the band: stand and shoot. */
	Hold,
	/** Inside the band but not close: a few steps back on his roof. */
	StepBack,
	/** Beyond the band: zip to a roof nearer the target, or shoot from here if there is none. */
	Approach,
	/** She is on top of him: zip to another roof. */
	Relocate
};

/**
 * The gunner's burst (claude-docs/gameplay-semantics.md, "gunner"): a TelegraphSeconds wind-up (the
 * pistol raised, the glint on), then ShotsPerBurst shots ShotIntervalSeconds apart. Pure: Advance
 * returns how many shots came due, so the timing is testable without a world or a weapon.
 */
USTRUCT(BlueprintType)
struct HAWKEYE_API FHawkeyeBurstClock
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Burst", meta = (ClampMin = "0.0"))
	float TelegraphSeconds = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Burst", meta = (ClampMin = "1"))
	int32 ShotsPerBurst = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Burst", meta = (ClampMin = "0.01"))
	float ShotIntervalSeconds = 0.25f;

	/** Begins the telegraph. Ignored while a burst is running. */
	void Start()
	{
		if (bActive)
		{
			return;
		}
		bActive = true;
		Elapsed = 0.f;
		ShotsFired = 0;
	}

	/** Drops the burst where it is (a hit, a duck into cover). */
	void Cancel()
	{
		bActive = false;
		Elapsed = 0.f;
		ShotsFired = 0;
	}

	/** Runs the clock on; the number of shots that came due in this step. The burst ends after its last. */
	int32 Advance(float DeltaSeconds)
	{
		if (!bActive)
		{
			return 0;
		}
		Elapsed += FMath::Max(DeltaSeconds, 0.f);
		int32 Due = 0;
		if (Elapsed >= TelegraphSeconds)
		{
			Due = FMath::Min(ShotsPerBurst, 1 + FMath::FloorToInt((Elapsed - TelegraphSeconds) / ShotIntervalSeconds));
		}
		const int32 New = FMath::Max(Due - ShotsFired, 0);
		ShotsFired = Due;
		if (ShotsFired >= ShotsPerBurst)
		{
			bActive = false;
			++BurstsCompleted;
		}
		return New;
	}

	bool IsActive() const { return bActive; }
	bool IsTelegraphing() const { return bActive && Elapsed < TelegraphSeconds; }
	bool IsFiring() const { return bActive && Elapsed >= TelegraphSeconds; }
	int32 GetShotsFired() const { return ShotsFired; }
	int32 GetBurstsCompleted() const { return BurstsCompleted; }
	float GetElapsed() const { return Elapsed; }

private:
	bool bActive = false;
	float Elapsed = 0.f;
	int32 ShotsFired = 0;
	int32 BurstsCompleted = 0;
};

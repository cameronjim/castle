// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MeleeCombo.generated.h"

/** Tuning for Kate's chain and her combo counter. */
USTRUCT(BlueprintType)
struct HAWKEYE_API FHawkeyeComboSettings
{
	GENERATED_BODY()

	/** Lights in one chain: light, light, then the finishing light. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee|Combo", meta = (ClampMin = "1"))
	int32 ChainLength = 3;

	/** After a hit lands, the next light must come within this to carry the chain on, seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee|Combo", meta = (ClampMin = "0.0"))
	float ChainWindowSeconds = 0.35f;

	/** The counter goes back to 0 this long after the last hit, seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee|Combo", meta = (ClampMin = "0.0"))
	float CounterResetSeconds = 2.f;

	/** From this count on, hits do BonusMultiplier and the counter glows. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee|Combo", meta = (ClampMin = "1"))
	int32 BonusAtCount = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee|Combo", meta = (ClampMin = "1.0"))
	float BonusMultiplier = 1.2f;
};

/**
 * The chain and the counter, pure (the character feeds it hits, misses and time).
 *
 * Chain: the step of the next light. A light that lands opens ChainWindowSeconds for the next step;
 * the third wraps back to the first. A miss, a heavy or the window running out puts the next light
 * back at step 0.
 *
 * Counter: every melee hit that lands counts one; CounterResetSeconds without a hit clears it. A hit
 * struck while the counter already reads BonusAtCount or more does BonusMultiplier.
 */
USTRUCT(BlueprintType)
struct HAWKEYE_API FHawkeyeComboTracker
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee|Combo")
	FHawkeyeComboSettings Settings;

	/** Runs the chain window and the counter's reset on. */
	void Advance(float DeltaSeconds)
	{
		const float Step = FMath::Max(DeltaSeconds, 0.f);
		if (ChainWindowRemaining > 0.f)
		{
			ChainWindowRemaining = FMath::Max(0.f, ChainWindowRemaining - Step);
			if (ChainWindowRemaining <= 0.f)
			{
				NextStep = 0;
			}
		}
		if (Count > 0)
		{
			CountRemaining -= Step;
			if (CountRemaining <= 0.f)
			{
				Count = 0;
				CountRemaining = 0.f;
			}
		}
	}

	/** A light of chain step Step landed: the counter goes up and the window for the next step opens. */
	void NotifyLightLanded(int32 Step)
	{
		NotifyHit();
		NextStep = (FMath::Max(Step, 0) + 1) % FMath::Max(Settings.ChainLength, 1);
		ChainWindowRemaining = NextStep == 0 ? 0.f : Settings.ChainWindowSeconds;
	}

	/** Any other melee hit landed (a heavy, a finisher): counts, and ends the chain. */
	void NotifyHit()
	{
		++Count;
		CountRemaining = Settings.CounterResetSeconds;
		EndChain();
	}

	/** A swing that hit nothing (or a shield): the next light starts the chain again. */
	void NotifyMiss() { EndChain(); }

	void EndChain()
	{
		NextStep = 0;
		ChainWindowRemaining = 0.f;
	}

	void Reset()
	{
		EndChain();
		Count = 0;
		CountRemaining = 0.f;
	}

	/** The step the next light is: 0, 1 or 2. */
	int32 GetNextStep() const { return NextStep; }

	/** True while a landed light's window for the next step is open. */
	bool IsChainOpen() const { return ChainWindowRemaining > 0.f; }

	float GetChainWindowRemaining() const { return ChainWindowRemaining; }

	int32 GetCount() const { return Count; }

	/** The counter reads BonusAtCount or more: it glows, and the next hit does the bonus. */
	bool IsBonusActive() const { return Count >= Settings.BonusAtCount; }

	/** What a hit struck now is multiplied by. */
	float GetDamageMultiplier() const { return IsBonusActive() ? Settings.BonusMultiplier : 1.f; }

private:
	int32 NextStep = 0;
	float ChainWindowRemaining = 0.f;
	int32 Count = 0;
	float CountRemaining = 0.f;
};

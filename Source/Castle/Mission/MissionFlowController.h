// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MissionFlowController.generated.h"

/** One beat of the end-of-mission sequence. ACastlePlayerController performs each in turn. */
UENUM(BlueprintType)
enum class EMissionFlowStep : uint8
{
	/** Nothing is running; the mission is still being played. */
	Idle,
	/** Black card with the mission name and its EndCardLine, held for EndCardSeconds. */
	EndCard,
	/** The mission's flashback slideshow. */
	Flashback,
	/** End card again, held until a key is pressed. Only when there is no next level. */
	FinalCard,
	/** Travel to the completed mission's NextLevel. */
	OpenNextLevel,
	/** Travel to the main menu (or the sandbox), because the campaign is over. */
	OpenMenuLevel,
	/** The sequence has handed off; nothing left to do. */
	Done
};

/**
 * The order of the end-of-mission beats, with no UMG, no world and no travel in it.
 *
 * ACastlePlayerController owns one, calls Begin when the mission completes, performs whatever
 * GetStep returns, and calls Advance when that beat reports it has finished. Keeping the
 * ordering here is what makes it testable: a test drives Begin/Advance and asserts the
 * sequence, without a widget or a level to load.
 */
UCLASS(BlueprintType)
class CASTLE_API UMissionFlowController : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Starts the sequence at the end card. bHasFlashback and bHasNextLevel are read once, here,
	 * so the route is decided before anything on screen can change it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Mission|Flow")
	EMissionFlowStep Begin(bool bInHasFlashback, bool bInHasNextLevel);

	/** Moves to the beat after the current one and returns it. Idle and Done never move. */
	UFUNCTION(BlueprintCallable, Category = "Mission|Flow")
	EMissionFlowStep Advance();

	/** Puts the sequence back to Idle, for a fresh mission in the same world. */
	UFUNCTION(BlueprintCallable, Category = "Mission|Flow")
	void Reset();

	UFUNCTION(BlueprintPure, Category = "Mission|Flow")
	EMissionFlowStep GetStep() const { return Step; }

	/** True between Begin and the sequence reaching Done. */
	UFUNCTION(BlueprintPure, Category = "Mission|Flow")
	bool IsRunning() const { return Step != EMissionFlowStep::Idle && Step != EMissionFlowStep::Done; }

	UFUNCTION(BlueprintPure, Category = "Mission|Flow")
	bool HasFlashback() const { return bHasFlashback; }

	UFUNCTION(BlueprintPure, Category = "Mission|Flow")
	bool HasNextLevel() const { return bHasNextLevel; }

private:
	/** The beat that follows Current, given the flashback and next-level flags. */
	EMissionFlowStep NextAfter(EMissionFlowStep Current) const;

	UPROPERTY(Transient)
	EMissionFlowStep Step = EMissionFlowStep::Idle;

	UPROPERTY(Transient)
	bool bHasFlashback = false;

	UPROPERTY(Transient)
	bool bHasNextLevel = false;
};

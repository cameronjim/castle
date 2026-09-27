// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HawkeyeSceneReturn.generated.h"

class UWorld;

/** An actor in the district the player could be put at on coming back from a playable scene. */
USTRUCT(BlueprintType)
struct HAWKEYE_API FHawkeyeReturnCandidate
{
	GENERATED_BODY()

	/** The actor's label (editor builds) or name. */
	UPROPERTY(BlueprintReadOnly, Category = "Save|Scene")
	FName Label;

	UPROPERTY(BlueprintReadOnly, Category = "Save|Scene")
	TArray<FName> Tags;

	UPROPERTY(BlueprintReadOnly, Category = "Save|Scene")
	FTransform Transform;
};

/** A flashback's playable scene in progress: where to come back to (claude-docs/gameplay-semantics.md, "Flashback"). */
USTRUCT(BlueprintType)
struct HAWKEYE_API FHawkeyeSceneReturn
{
	GENERATED_BODY()

	/** Between entering the scene and its mission completing. Saves are refused meanwhile. */
	UPROPERTY(BlueprintReadOnly, Category = "Save|Scene")
	bool bInScene = false;

	/** Between leaving the scene and the district being back with the player placed. */
	UPROPERTY(BlueprintReadOnly, Category = "Save|Scene")
	bool bReturnPending = false;

	/** The district was saved on the way in, so the way back is a load. */
	UPROPERTY(BlueprintReadOnly, Category = "Save|Scene")
	bool bSavedOnEntry = false;

	UPROPERTY(BlueprintReadOnly, Category = "Save|Scene")
	FName ReturnPointLabel;

	/** The district map's package, opened fresh if the save cannot be loaded. */
	UPROPERTY(BlueprintReadOnly, Category = "Save|Scene")
	FString ReturnMap;

	/**
	 * Which candidate the player goes back to: the one labelled ReturnPointLabel, else the first
	 * tagged with it. INDEX_NONE when the label is None or nothing matches (the saved position stands).
	 */
	static int32 SelectReturnPoint(const TArray<FHawkeyeReturnCandidate>& Candidates, FName ReturnPointLabel);

	/** Every actor in World as a candidate. */
	static TArray<FHawkeyeReturnCandidate> GatherCandidates(UWorld* World);
};

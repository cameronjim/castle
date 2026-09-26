// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CityLedgeData.generated.h"

/** One hidden traversable ledge block along a roof edge: BP_TraversableBlock at Transform. */
USTRUCT(BlueprintType)
struct CASTLE_API FCityLedgeRecord
{
	GENERATED_BODY()

	/** The block's bottom corner, turned so local X runs along the edge, scaled into a slab. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "City")
	FTransform Transform;

	/** Which edge of the footprint (after ring cleaning) this is. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "City")
	int32 EdgeIndex = 0;

	/** The building's OpenStreetMap id, e.g. way/123456. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "City")
	FString OsmId;
};

/** One grapple anchor on a roof: BP_GrappleAnchor at Transform, its landing point at LandingOffset. */
USTRUCT(BlueprintType)
struct CASTLE_API FCityAnchorRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "City")
	FTransform Transform;

	/** The LandingPoint component's relative location. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "City")
	FVector LandingOffset = FVector(60.f, 0.f, 0.f);

	/** The generator's stable anchor number (City_Anchor_<Index>). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "City")
	int32 Index = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "City")
	FString OsmId;
};

/**
 * The generated district's many small actors, as data instead of saved actors: every roof-edge
 * traversable ledge and every grapple anchor. Tools/Editor/generate_city.py writes it
 * (DA_EastVillage_CityProps) and ACityLedgeSpawner spawns the actors at load. Saved as actors
 * they made the district map 72 MB.
 */
UCLASS(BlueprintType)
class CASTLE_API UCityLedgeData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "City")
	TArray<FCityLedgeRecord> Ledges;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "City")
	TArray<FCityAnchorRecord> Anchors;

	/** Hash of the generator inputs that produced the arrays; a rerun with the same hash saves nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "City")
	FString SourceHash;
};

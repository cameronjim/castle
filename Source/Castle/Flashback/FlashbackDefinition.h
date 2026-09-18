// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FlashbackDefinition.generated.h"

class UTexture2D;
class USoundBase;

/** One still frame of a flashback slideshow. */
USTRUCT(BlueprintType)
struct CASTLE_API FFlashbackSlide
{
	GENERATED_BODY()

	/** Full-screen still. Soft so an unplayed flashback costs no memory. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slide")
	TSoftObjectPtr<UTexture2D> Image;

	/** Caption shown under the image. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slide", meta = (MultiLine = "true"))
	FText Caption;

	/** Seconds this slide stays up before advancing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slide", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float HoldSeconds = 4.f;

	/** Seconds spent fading in from the previous slide. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slide", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float CrossfadeSeconds = 1.f;

	/** Optional narration played when this slide appears. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slide")
	TSoftObjectPtr<USoundBase> VoiceLine;
};

/**
 * A slideshow told between missions (the origin story beats).
 * Create via Content Browser > Miscellaneous > Data Asset > FlashbackDefinition.
 */
UCLASS(BlueprintType)
class CASTLE_API UFlashbackDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flashback")
	FText Title;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flashback")
	TArray<FFlashbackSlide> Slides;

	/** Looping bed played for the whole flashback. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flashback")
	TSoftObjectPtr<USoundBase> AmbientLoop;

	/** When true any key dismisses the flashback early. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flashback")
	bool bSkippable = true;

	//~ Begin UPrimaryDataAsset interface
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	//~ End UPrimaryDataAsset interface
};

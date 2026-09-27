// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HawkeyeAmbience.generated.h"

class UAudioComponent;
class USoundBase;

/**
 * The district's sound bed (claude-docs/gameplay-semantics.md, "Audio"): rooftop wind and the street's
 * hum, two non-spatial loops crossfaded by how high the listener's pawn stands above this actor (placed
 * at street level by generate_city.py as City_Ambience). Wind is at its floor under 3 m and full from
 * 10 m up; the street falls to a third up there. The fade is HawkeyeAudioMath::ComputeAmbienceMix.
 */
UCLASS(Blueprintable, BlueprintType)
class HAWKEYE_API AHawkeyeAmbience : public AActor
{
	GENERATED_BODY()

public:
	AHawkeyeAmbience();

	/** Gusting wind (MS_Amb_Wind). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambience")
	TSoftObjectPtr<USoundBase> WindSound;

	/** Traffic drone and the odd far-off horn (MS_Amb_Street). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambience")
	TSoftObjectPtr<USoundBase> StreetSound;

	/** How often the crossfade is recomputed, s. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambience", meta = (ClampMin = "0.02"))
	float UpdateSeconds = 0.25f;

	/** Seconds each volume takes to reach a new level, so a zip up a building swells rather than steps. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambience", meta = (ClampMin = "0.0"))
	float FadeSeconds = 1.0f;

	/** The listener's height above this actor, cm, as last measured. */
	UFUNCTION(BlueprintPure, Category = "Ambience")
	float GetListenerHeight() const { return ListenerHeight; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Measures the local pawn's height and fades both loops toward the mix for it. */
	void UpdateMix();

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Wind = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Street = nullptr;

	float ListenerHeight = 0.f;
	float SinceUpdate = 0.f;
};

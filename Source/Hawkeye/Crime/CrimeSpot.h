// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Crime/CrimeTypes.h"
#include "CrimeSpot.generated.h"

class UCrimeDefinition;

/**
 * A place a street crime can start: a street corner, an alley mouth or a rooftop. generate_city.py
 * places twelve (City_CrimeSpot_<n>: eight on the street, four on roofs, none within 40 m of the
 * safehouse or a challenge pedestal) and fills in which crimes may happen at each. The actor's
 * location is the feet of the crime's centre; it faces the storefront a robbery is at. Nothing to
 * see: it only marks the spot.
 */
UCLASS(Blueprintable, BlueprintType)
class HAWKEYE_API ACrimeSpot : public AActor
{
	GENERATED_BODY()

public:
	ACrimeSpot();

	/** The crimes that may start here. Street spots take the mugging, robbery and ambush; rooftops the rooftop crime. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crime")
	TArray<TObjectPtr<UCrimeDefinition>> Crimes;

	/** On a roof rather than the street. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crime")
	bool bRooftop = false;

	/** Robbery: where the runner heads with the loot (feet, on the sidewalk). Unset (zero) when the spot has none. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crime")
	FVector EscapeLocation = FVector::ZeroVector;

	UFUNCTION(BlueprintPure, Category = "Crime")
	bool HasEscapeLocation() const { return !EscapeLocation.IsNearlyZero(); }

	/** The first of Crimes whose type is Type, or null. */
	UCrimeDefinition* FindCrime(ECrimeType Type) const;
};

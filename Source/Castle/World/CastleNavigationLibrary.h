// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CastleNavigationLibrary.generated.h"

/**
 * Navigation helpers for the headless scripts. verify_city.py builds the district's navmesh in
 * the editor world and checks every placed thug stands on it; neither is reachable from Python
 * as the engine ships (an editor world holds its nav build behind an async-load lock that only
 * a ticking editor ever releases).
 */
UCLASS()
class CASTLE_API UCastleNavigationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Builds navigation in WorldContext's world now, blocking until it is done, the way the game
	 * mode does at BeginPlay. Releases the editor's async-load build lock first. False when the
	 * world has no navigation system or nothing was built.
	 */
	UFUNCTION(BlueprintCallable, Category = "Castle|Navigation", meta = (WorldContext = "WorldContext"))
	static bool BuildNavigationNow(UObject* WorldContext);

	/**
	 * True when Point is within Extent of the navmesh; OutProjected is where it lands on it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Castle|Navigation", meta = (WorldContext = "WorldContext"))
	static bool ProjectToNavigation(UObject* WorldContext, FVector Point, FVector Extent, FVector& OutProjected);
};

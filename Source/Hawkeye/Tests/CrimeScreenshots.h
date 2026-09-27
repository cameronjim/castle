// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

class FAutomationTestBase;

/**
 * Queues the street crime shots onto the Kate screenshot pass (Saved/Screenshots/Kate/): crime_marker,
 * crime_mugging and crime_results. See CrimeScreenshotTest.cpp.
 */
void HawkeyeAddCrimeShots(FAutomationTestBase* Test);

#endif

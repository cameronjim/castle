// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

class FAutomationTestBase;

/**
 * Queues the side-challenge shots onto the Kate screenshot pass (Saved/Screenshots/Kate/):
 * challenge_pedestal, archery_targets, challenge_results and traversal_checkpoint. See
 * ChallengeScreenshotTest.cpp.
 */
void HawkeyeAddChallengeShots(FAutomationTestBase* Test);

#endif

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

class FAutomationTestBase;

/**
 * Queues the world map shot onto the Kate screenshot pass (Saved/Screenshots/Kate/): map_open.png, the
 * map open at the PlayerStart with its icons and legend and the cursor on the nearest safehouse.
 * See MapScreenshotTest.cpp.
 */
void HawkeyeAddMapShots(FAutomationTestBase* Test);

#endif

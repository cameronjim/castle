// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

class FAutomationTestBase;

/**
 * Queues the melee shots onto a screenshot pass (Saved/Screenshots/Kate/): combo_x3, parry_flash,
 * finisher_mid, hit_lean and strike_pose. See MeleeScreenshotTest.cpp. Fights a thug it spawns itself
 * (and removes), so the district's thugs are left as they were; Kate is put back as she was.
 */
void HawkeyeAddMeleeShots(FAutomationTestBase* Test);

#endif

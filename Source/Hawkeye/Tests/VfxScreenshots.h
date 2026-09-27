// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

class FAutomationTestBase;

/**
 * Queues the effect shots onto the Kate screenshot pass (Saved/Screenshots/Kate/): vfx_snowfall,
 * vfx_footstep, vfx_explosion, vfx_emp, vfx_hit_spark and vfx_smoke, 30 m down the street from the
 * PlayerStart so the lamps an EMP puts out and the scorch it leaves stay out of the other shots.
 * See VfxScreenshotTest.cpp. vfx_zipline.png is taken on the pass's own grapple zip.
 */
void HawkeyeAddVfxShots(FAutomationTestBase* Test);

#endif

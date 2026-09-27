// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Real frame times after the first playable frame: the worst one, when it came, and how many ran
 * over a threshold, with the frames the engine spent writing a screenshot counted apart (a capture
 * is the test's cost, not the game's). No world, no engine: AHawkeyeGameMode feeds it wall-clock
 * deltas and the tests feed it numbers.
 */
struct HAWKEYE_API FHawkeyeFrameWatch
{
	/** Frames at or over this count as hitches, s. */
	double ThresholdSeconds = 0.1;

	int32 Frames = 0;
	double WorstSeconds = 0.0;
	double WorstAtSeconds = 0.0;
	int32 WorstFrame = INDEX_NONE;
	bool bWorstWasScreenshot = false;
	int32 HitchFrames = 0;
	int32 ScreenshotHitchFrames = 0;

	/** Worst frame that was not a screenshot capture. */
	double WorstGameSeconds = 0.0;
	double WorstGameAtSeconds = 0.0;

	/** One frame of FrameSeconds ending SinceStartSeconds after the watch began. True when it is a hitch. */
	bool AddFrame(double FrameSeconds, double SinceStartSeconds, bool bScreenshotFrame);

	/** Hitches that were not screenshot captures. */
	int32 GetGameHitchFrames() const { return HitchFrames - ScreenshotHitchFrames; }
};

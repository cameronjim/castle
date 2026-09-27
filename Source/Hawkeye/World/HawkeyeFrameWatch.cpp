// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/HawkeyeFrameWatch.h"

bool FHawkeyeFrameWatch::AddFrame(double FrameSeconds, double SinceStartSeconds, bool bScreenshotFrame)
{
	const double Seconds = FMath::Max(FrameSeconds, 0.0);
	if (Seconds > WorstSeconds || Frames == 0)
	{
		WorstSeconds = Seconds;
		WorstAtSeconds = SinceStartSeconds;
		WorstFrame = Frames;
		bWorstWasScreenshot = bScreenshotFrame;
	}
	if (!bScreenshotFrame && Seconds > WorstGameSeconds)
	{
		WorstGameSeconds = Seconds;
		WorstGameAtSeconds = SinceStartSeconds;
	}
	++Frames;
	const bool bHitch = Seconds >= ThresholdSeconds;
	if (bHitch)
	{
		++HitchFrames;
		ScreenshotHitchFrames += bScreenshotFrame ? 1 : 0;
	}
	return bHitch;
}

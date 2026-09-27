// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HawkeyeSettings.generated.h"

/**
 * Every player-facing option, in one struct, so the save file is one blob and the Settings
 * screen has one thing to read. Adding an option means a new field here and a Version bump;
 * an old save then fails the version check and the player gets defaults rather than garbage.
 */
USTRUCT(BlueprintType)
struct HAWKEYE_API FHawkeyeSettings
{
	GENERATED_BODY()

	/** Multiplier on raw mouse look input. Clamped to [0.02, 1.0] by the subsystem. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float LookSensitivity = 0.2f;

	/**
	 * Multiplier on the gamepad look stick's degrees-per-second rate. Clamped to [0.2, 3.0] by
	 * the subsystem. Unlike LookSensitivity this scales an already delta-time-scaled rate rather
	 * than a raw per-frame pixel delta, so 1.0 reads as "the tuned default", not "off".
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float StickSensitivity = 1.0f;

	/** Flips mouse look pitch: on, pushing the mouse forward looks down instead of up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	bool bInvertMouseY = false;

	/** Flips gamepad look pitch: on, pushing the stick up looks down instead of up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	bool bInvertStickY = false;

	/** Everything the game plays, 0..1. The sliders are squared into gains (HawkeyeAudioMath). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Audio")
	float MasterVolume = 1.0f;

	/** Bow, arrows, fights, footsteps and the menus, 0..1, under the master. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Audio")
	float SfxVolume = 1.0f;

	/** The city: wind, street hum, lamp buzz, 0..1, under the master. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Audio")
	float AmbientVolume = 0.8f;

	/** Bumped whenever the meaning of a field changes. A mismatch on load yields defaults. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	int32 Version = 4;

	/** The version this build writes and accepts. A save from an older version yields defaults. */
	static constexpr int32 CurrentVersion = 4;
};

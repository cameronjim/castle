// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CastleSettings.generated.h"

/**
 * Every player-facing option, in one struct, so the save file is one blob and the Settings
 * screen has one thing to read. Adding an option means a new field here and a Version bump;
 * an old save then fails the version check and the player gets defaults rather than garbage.
 */
USTRUCT(BlueprintType)
struct CASTLE_API FCastleSettings
{
	GENERATED_BODY()

	/** Multiplier on raw mouse look input. Clamped to [0.02, 1.0] by the subsystem. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float LookSensitivity = 0.2f;

	/** Bumped whenever the meaning of a field changes. A mismatch on load yields defaults. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	int32 Version = 1;

	/** The version this build writes and accepts. */
	static constexpr int32 CurrentVersion = 1;
};

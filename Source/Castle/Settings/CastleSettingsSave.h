// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Settings/CastleSettings.h"
#include "CastleSettingsSave.generated.h"

/**
 * The on-disk form of FCastleSettings, in its own slot so that wiping a campaign save never
 * costs the player their sensitivity. Saved/SaveGames/CastleSettings.sav.
 */
UCLASS()
class CASTLE_API UCastleSettingsSave : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FCastleSettings Settings;
};

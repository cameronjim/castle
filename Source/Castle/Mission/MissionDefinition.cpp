// Copyright Epic Games, Inc. All Rights Reserved.

#include "Mission/MissionDefinition.h"
#include "Mission/MissionObjective.h"

FPrimaryAssetId UMissionDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("Mission"), GetFName());
}

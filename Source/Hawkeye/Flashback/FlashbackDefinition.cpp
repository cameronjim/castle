// Copyright Epic Games, Inc. All Rights Reserved.

#include "Flashback/FlashbackDefinition.h"

FPrimaryAssetId UFlashbackDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("Flashback"), GetFName());
}

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "DialogueLine.generated.h"

/** When a banter line can play. Row names in DT_Dialogue carry the snake_case form: kate_after_fight_03. */
UENUM(BlueprintType)
enum class EHawkeyeBanterSituation : uint8
{
	/** Roaming the district, every 45 to 90 s. */
	IdleRoam,
	/** The fight clock ran out. */
	AfterFight,
	/** The current objective is close. */
	ObjectiveNear,
	/** The lead dropped under a third of her health. */
	LowHealth
};

/** One spoken line: a row of DT_Dialogue. No audio yet; the HUD shows it as a subtitle. */
USTRUCT(BlueprintType)
struct HAWKEYE_API FHawkeyeDialogueLine : public FTableRowBase
{
	GENERATED_BODY()

	/** Kate or Clint. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FName Speaker;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	EHawkeyeBanterSituation Situation = EHawkeyeBanterSituation::IdleRoam;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue", meta = (MultiLine = "true"))
	FText Text;
};

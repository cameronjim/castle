// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ArrowDefinition.generated.h"

class AArrowProjectile;
class UTexture2D;

/** What an arrow does when it lands, beyond its damage. Only Grapple is built; the rest are data for later. */
UENUM(BlueprintType)
enum class EArrowHitEffect : uint8
{
	None,
	/** Replaced by the grapple flow: flies at the marked anchor and starts the zip. */
	Grapple,
	Putty,
	Bola,
	Smoke,
	EMP,
	Explosive
};

/** Number of quiver slots. Number keys 1..6 select them. */
static constexpr int32 CastleQuiverSlotCount = 6;

/**
 * One arrow type (claude-docs/gameplay-semantics.md, "bow and arrows"): the projectile it fires,
 * what it does on hit, its damage, how many the quiver holds and whether it can be pulled back out
 * of whatever it stuck in. A new trick arrow is a new asset (DA_Arrow_<Type>) plus, only when it
 * behaves differently in flight, a BP_Arrow_<Type> projectile.
 */
UCLASS(BlueprintType)
class CASTLE_API UArrowDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Player-facing name ("Standard arrow"). Shown in the inventory screen. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arrow")
	FText DisplayName;

	/** Hotbar label, about eight characters. Falls back to DisplayName. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arrow")
	FText ShortName;

	/** Quiver slot, 1..6, which is also the number key. Standard arrows are always slot 1. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arrow", meta = (ClampMin = "1", ClampMax = "6"))
	int32 Slot = 1;

	/** What the bow spawns. BP_Arrow_Standard for most; BP_Arrow_Grapple flies straight at an anchor. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arrow")
	TSubclassOf<AArrowProjectile> ProjectileClass;

	/** Damage at full draw before the headshot multiplier and the perfect-release bonus. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arrow", meta = (ClampMin = "0.0"))
	float Damage = 40.f;

	/** Most the quiver carries. Pickups and recovery never push the count past it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arrow", meta = (ClampMin = "1"))
	int32 Cap = 30;

	/** Stuck arrows of this type go back in the quiver when the player walks within reach. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arrow")
	bool bRecoverable = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arrow")
	EArrowHitEffect OnHitEffect = EArrowHitEffect::None;

	/** Hotbar icon. TODO(stage3): unused until icons are drawn; the hotbar shows ShortName. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arrow")
	TSoftObjectPtr<UTexture2D> Icon;

	/** DisplayName, falling back to the asset name. */
	UFUNCTION(BlueprintPure, Category = "Arrow")
	FText GetDisplayNameOrAssetName() const;

	/** ShortName, then DisplayName, then the asset name. */
	UFUNCTION(BlueprintPure, Category = "Arrow")
	FText GetShortNameOrDisplayName() const;

	//~ Begin UPrimaryDataAsset interface
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	//~ End UPrimaryDataAsset interface
};

/** An arrow type and a count: what a mission grants at the start. */
USTRUCT(BlueprintType)
struct CASTLE_API FCastleArrowGrant
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arrow")
	TSoftObjectPtr<UArrowDefinition> Arrow;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arrow", meta = (ClampMin = "0"))
	int32 Count = 0;
};

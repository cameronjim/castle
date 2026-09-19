// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WeaponDefinition.generated.h"

class UStaticMesh;

/**
 * The three hotbar slots. The numeric values are the number keys the player presses, minus
 * one, so a slot and its key never drift apart.
 */
UENUM(BlueprintType)
enum class EHotbarSlot : uint8
{
	/** Always present: Frank's fists. Left click punches. */
	Hands = 0,
	Pistol = 1,
	Rifle = 2
};

/** Number of hotbar slots. Kept next to the enum so a fourth slot is one edit. */
static constexpr int32 CastleHotbarSlotCount = 3;

/**
 * Everything that makes one weapon different from another: damage, ammo, feel and the mesh
 * the player sees. A new weapon is a new data asset, never a new class.
 *
 * Create via Content Browser > Miscellaneous > Data Asset > WeaponDefinition, or let
 * Tools/Editor/create_weapon_data.py make the three the game ships with.
 */
UCLASS(BlueprintType)
class CASTLE_API UWeaponDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Player-facing name ("Pistol"). Shown in the inventory screen. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	FText DisplayName;

	/** Short label for the hotbar box, where there is room for about eight characters. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	FText ShortName;

	/** Which hotbar slot this weapon occupies. Two weapons can share a slot; the last one wins. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	EHotbarSlot Slot = EHotbarSlot::Pistol;

	/** Damage per hit before the headshot multiplier. For a melee weapon this is the punch. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0.0"))
	float Damage = 34.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Ammo", meta = (ClampMin = "0"))
	int32 MagazineSize = 12;

	/** Spare rounds carried when this weapon is first picked up. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Ammo", meta = (ClampMin = "0"))
	int32 DefaultReserve = 24;

	/** Rounds per minute. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "1.0"))
	float FireRate = 600.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0.0"))
	float ReloadSeconds = 2.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Spread", meta = (ClampMin = "0.0"))
	float HipSpreadDegrees = 2.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Spread", meta = (ClampMin = "0.0"))
	float AimSpreadDegrees = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "1.0"))
	float HeadshotMultiplier = 3.f;

	/** Bones that count as a head. Matches the default UE5 skeleton naming. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	TSet<FName> HeadBoneNames;

	// --- Melee ----------------------------------------------------------------------------------

	/** True for Hands: Fire() punches instead of tracing a bullet, and there is no ammo. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Melee")
	bool bIsMelee = false;

	/** Reach of the punch sweep, in centimetres. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Melee", meta = (ClampMin = "0.0"))
	float MeleeRange = 120.f;

	/** Seconds between punches. Slower than the fire rate on purpose: fists are not a gun. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Melee", meta = (ClampMin = "0.0"))
	float MeleeCooldown = 0.6f;

	/** Melee hits above UHealthComponent's stagger threshold fire OnStaggered on the victim. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Melee")
	bool bStaggerOnHit = true;

	// --- View model -----------------------------------------------------------------------------

	/** Mesh held in the view model's right hand. Empty for Hands (and for the rifle, so far). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|ViewModel")
	TSoftObjectPtr<UStaticMesh> ViewModelMesh;

	/** Arms pose this weapon is held in: "Fists", "Pistol" or "Rifle". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|ViewModel")
	FName ArmsPoseName = FName(TEXT("Pistol"));

	/** Where the grip sits in the palm, relative to the hand bone. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|ViewModel")
	FVector HandOffset = FVector(4.f, 0.f, 0.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|ViewModel")
	FRotator HandRotation = FRotator(0.f, -90.f, 0.f);

	UWeaponDefinition();

	/** DisplayName, falling back to the asset name so an unnamed definition still reads. */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	FText GetDisplayNameOrAssetName() const;

	/** ShortName, falling back to DisplayName and then to the asset name. */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	FText GetShortNameOrDisplayName() const;

	//~ Begin UPrimaryDataAsset interface
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	//~ End UPrimaryDataAsset interface
};

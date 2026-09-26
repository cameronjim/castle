// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/ArrowDefinition.h"
#include "InventoryComponent.generated.h"

class UBowDefinition;
class UMissionDefinition;
class UWeaponComponent;
class UWeaponDefinition;

/** One quiver slot: which arrow type it holds and how many are left. */
USTRUCT(BlueprintType)
struct CASTLE_API FCastleQuiverSlot
{
	GENERATED_BODY()

	/** The arrow type in this slot, or null while the slot is empty. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UArrowDefinition> Arrow = nullptr;

	/** Arrows left, 0..Arrow->Cap. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	int32 Count = 0;

	bool IsEmpty() const { return Arrow == nullptr; }
};

/** Fired whenever the bow, an arrow count or the keycards change. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryChangedSignature);

/** Fired when the active quiver slot changes. Slots are 1..6. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnActiveArrowSlotChangedSignature, int32, OldSlot, int32, NewSlot);

/**
 * What Kate carries: a bow (or none), a six-slot quiver, and a keycard ring.
 *
 * Rules (claude-docs/gameplay-semantics.md, "bow and arrows"): standard arrows are always in
 * slot 1, present even at zero; counts never exceed an arrow's Cap; number keys pick a slot and
 * the wheel steps through the filled ones, skipping empty slots. Without a bow, left click is
 * Hands, the melee fallback on the owner's UWeaponComponent. Clear() goes back to what the mission
 * granted (StartingBow and StartingArrows); nothing carries between missions.
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = (Castle), meta = (BlueprintSpawnableComponent))
class CASTLE_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryComponent();

	/** DA_Weapon_Hands: the punch used while no bow is owned. A transient stand-in when unset (tests). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	TSoftObjectPtr<UWeaponDefinition> HandsDefinition;

	/** DA_Arrow_Standard: what fills slot 1 when nothing else has. A transient stand-in when unset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	TSoftObjectPtr<UArrowDefinition> StandardArrowDefinition;

	/** The bow Clear() hands back. Set from the mission by ApplyStartingQuiver. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UBowDefinition> StartingBow = nullptr;

	/** The arrows Clear() hands back, one entry per type. Set from the mission by ApplyStartingQuiver. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	TArray<FCastleQuiverSlot> StartingArrows;

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnInventoryChangedSignature OnInventoryChanged;

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnActiveArrowSlotChangedSignature OnActiveArrowSlotChanged;

	// --- Bow --------------------------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Inventory|Bow")
	UBowDefinition* GetBow() const { return Bow; }

	UFUNCTION(BlueprintPure, Category = "Inventory|Bow")
	bool HasBow() const { return Bow != nullptr; }

	/** Kate now carries Definition (null takes the bow away). Returns true when it changed. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Bow")
	bool GiveBow(UBowDefinition* Definition);

	// --- Quiver -----------------------------------------------------------------------------------

	/**
	 * Adds Count arrows of Definition to its slot, filling the slot if it was empty (and replacing
	 * a different type that was there). Clamped to the arrow's Cap. Returns how many went in.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Quiver")
	int32 AddArrows(UArrowDefinition* Definition, int32 Count);

	/** Takes one arrow from Slot. False (and nothing changes) when the slot is empty or at zero. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Quiver")
	bool ConsumeArrow(int32 Slot);

	/** Makes Slot (1..6) active. Does nothing for an empty or out-of-range slot or the active one. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Quiver")
	bool SelectArrowSlot(int32 Slot);

	/** Next filled slot, wrapping. Mouse wheel up. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Quiver")
	bool SelectNextArrowSlot();

	/** Previous filled slot, wrapping. Mouse wheel down. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Quiver")
	bool SelectPreviousArrowSlot();

	UFUNCTION(BlueprintPure, Category = "Inventory|Quiver")
	int32 GetActiveArrowSlot() const { return ActiveArrowSlot; }

	/** The arrow type in the active slot. Standard arrows unless another slot was picked. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Quiver")
	UArrowDefinition* GetActiveArrow() const;

	/** Slot's contents (1..6). Out of range returns an empty slot. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Quiver")
	FCastleQuiverSlot GetArrowSlot(int32 Slot) const;

	UFUNCTION(BlueprintPure, Category = "Inventory|Quiver")
	bool IsArrowSlotEmpty(int32 Slot) const { return GetArrowSlot(Slot).IsEmpty(); }

	UFUNCTION(BlueprintPure, Category = "Inventory|Quiver")
	int32 GetArrowCount(int32 Slot) const { return GetArrowSlot(Slot).Count; }

	/** The first slot holding an arrow with Effect, or INDEX_NONE. The grapple uses this. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Quiver")
	int32 FindArrowSlotByEffect(EArrowHitEffect Effect) const;

	/** Sets Slot's count directly, clamped to its Cap. Debug and tests. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Quiver")
	void SetArrowCount(int32 Slot, int32 Count);

	// --- Mission ----------------------------------------------------------------------------------

	/** Drops everything and rebuilds StartingBow and StartingArrows, slot 1 active. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void Clear();

	/** Replaces the starting bow and arrows and rebuilds from them. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void ApplyStartingQuiver(UBowDefinition* InBow, const TArray<FCastleQuiverSlot>& Arrows);

	/** Loads Mission's StartingBow and StartingArrows and applies them. ACastleGameMode calls this on mission start. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void ApplyMissionStart(const UMissionDefinition* Mission);

	// --- Keycards ---------------------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Inventory|Keycards")
	bool HasKeycard(FName KeycardId) const;

	/** Adds a keycard to the ring. Returns false when she already had it. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Keycards")
	bool GiveKeycard(FName KeycardId);

	UFUNCTION(BlueprintPure, Category = "Inventory|Keycards")
	TSet<FName> GetKeycards() const { return Keycards; }

	// --- Hands ------------------------------------------------------------------------------------

	/** Hands, loaded or made on demand. Never null outside the class default object. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	UWeaponDefinition* GetHandsDefinition();

	/** Standard arrows, loaded or made on demand. Never null outside the class default object. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	UArrowDefinition* GetStandardArrowDefinition();

protected:
	//~ Begin UActorComponent interface
	virtual void BeginPlay() override;
	//~ End UActorComponent interface

	/** Puts standard arrows in slot 1 (at zero) if nothing is there. Safe to call repeatedly. */
	void EnsureStandardSlot();

	/** Points the owner's weapon component at Hands, the melee fallback. */
	void ApplyHandsToWeapon();

	/** The next filled slot Step away from the active one, or INDEX_NONE. Slots are 1-based. */
	int32 FindAdjacentArrowSlot(int32 Step) const;

	/** Slot 1..6 to an array index, or INDEX_NONE. */
	static int32 SlotToIndex(int32 Slot);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory|Bow")
	TObjectPtr<UBowDefinition> Bow = nullptr;

	/** Always CastleQuiverSlotCount long; index 0 is slot 1. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory|Quiver")
	TArray<FCastleQuiverSlot> Arrows;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory|Quiver")
	int32 ActiveArrowSlot = 1;

	/** Keycard ids collected so far. Doors check this by id. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory|Keycards")
	TSet<FName> Keycards;

	UPROPERTY(Transient)
	TObjectPtr<UWeaponDefinition> FallbackHands = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UArrowDefinition> FallbackStandardArrow = nullptr;
};

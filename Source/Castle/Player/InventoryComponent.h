// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/WeaponDefinition.h"
#include "InventoryComponent.generated.h"

class UWeaponComponent;

/** One hotbar slot: what is in it and how much ammo that weapon is carrying. */
USTRUCT(BlueprintType)
struct CASTLE_API FCastleInventorySlot
{
	GENERATED_BODY()

	/** The weapon occupying this slot, or null while the slot is empty. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UWeaponDefinition> Weapon = nullptr;

	/** Rounds in the magazine. Always 0 for a melee weapon. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	int32 Magazine = 0;

	/** Spare rounds carried for this weapon. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	int32 Reserve = 0;

	bool IsEmpty() const { return Weapon == nullptr; }

	/** True for a slot holding a weapon that actually fires bullets. */
	bool IsRanged() const { return Weapon != nullptr && !Weapon->bIsMelee; }
};

/** Fired whenever a slot's weapon or ammo changes, or a keycard is added. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryChangedSignature);

/** Fired when the active slot changes, after the weapon component has been re-pointed. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnActiveSlotChangedSignature, EHotbarSlot, OldSlot, EHotbarSlot, NewSlot);

/**
 * What Frank is carrying: three hotbar slots and a ring of keycards.
 *
 * Rules (claude-docs/gameplay-semantics.md, "Inventory and hotbar"):
 * Hands are always in slot 0; a slot is empty until its weapon is picked up; selecting an
 * empty slot does nothing; a switch takes SwapSeconds during which Fire is refused; picking
 * a weapon up auto-selects it only when Hands was active; the inventory clears to
 * StartingSlots at mission complete and on restart.
 *
 * UWeaponComponent reads the active slot's definition for its stats and writes its ammo back
 * into the slot, so ammo survives a swap without the weapon component knowing about slots.
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = (Castle), meta = (BlueprintSpawnableComponent))
class CASTLE_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryComponent();

	/** Seconds a weapon switch takes. Fire is refused for the whole of it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory", meta = (ClampMin = "0.0"))
	float SwapSeconds = 0.4f;

	/**
	 * Definition used for slot 0 when DA_Weapon_Hands cannot be loaded (every automation test).
	 * Left unset in content; the component makes a melee stand-in so Hands is never missing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	TSoftObjectPtr<UWeaponDefinition> HandsDefinition;

	/**
	 * What the inventory holds after Clear(). Hands are added on top of this and never removed,
	 * so an empty array means "Hands only" - which is what a mission that grants nothing wants.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	TArray<TObjectPtr<UWeaponDefinition>> StartingSlots;

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnInventoryChangedSignature OnInventoryChanged;

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnActiveSlotChangedSignature OnActiveSlotChanged;

	/**
	 * Makes Slot active. Does nothing when the slot is empty or already active.
	 * Returns true when the switch started. bImmediate skips the swap lockout (mission start).
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool SelectSlot(EHotbarSlot Slot, bool bImmediate = false);

	/** Next non-empty slot, wrapping. Mouse wheel up. Returns true when the slot changed. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool SelectNextSlot();

	/** Previous non-empty slot, wrapping. Mouse wheel down. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool SelectPreviousSlot();

	/**
	 * Puts Definition in its own slot with a full magazine and its default reserve, replacing
	 * whatever was there. Auto-selects it only when Hands were active. Returns true when the
	 * inventory changed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool AddWeapon(UWeaponDefinition* Definition);

	/** Adds reserve rounds for Definition's slot. Returns false when that slot is empty. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool AddAmmo(UWeaponDefinition* Definition, int32 Rounds);

	/** Adds reserve rounds to one slot by index. Returns false when the slot is empty. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool AddAmmoToSlot(EHotbarSlot Slot, int32 Rounds);

	/** Drops everything and rebuilds Hands plus StartingSlots, with Hands active. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void Clear();

	/** Replaces StartingSlots and rebuilds the inventory from them. Called on mission start. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void ApplyStartingWeapons(const TArray<UWeaponDefinition*>& Weapons);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	EHotbarSlot GetActiveSlot() const { return ActiveSlot; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	UWeaponDefinition* GetActiveWeapon() const;

	/** The slot's contents. An out-of-range index returns an empty slot. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	FCastleInventorySlot GetSlot(EHotbarSlot Slot) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool IsSlotEmpty(EHotbarSlot Slot) const;

	/** True while a weapon switch is still running; UWeaponComponent refuses to fire. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool IsSwapping() const { return bSwapping; }

	/** Ends the swap lockout now. The swap timer calls this; so does a test with no world. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void FinishSwapNow();

	/** Writes the active weapon's ammo back into its slot. Called by UWeaponComponent. */
	void SetSlotAmmo(EHotbarSlot Slot, int32 Magazine, int32 Reserve);

	// --- Keycards ---------------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Inventory|Keycards")
	bool HasKeycard(FName KeycardId) const;

	/** Adds a keycard to the ring. Returns false when Frank already had it. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Keycards")
	bool GiveKeycard(FName KeycardId);

	UFUNCTION(BlueprintPure, Category = "Inventory|Keycards")
	TSet<FName> GetKeycards() const { return Keycards; }

	/** Hands, created or loaded on demand. Never null outside the class default object. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	UWeaponDefinition* GetHandsDefinition();

protected:
	//~ Begin UActorComponent interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent interface

	/** The owner's weapon component, or null for an owner that has none. */
	UWeaponComponent* FindWeaponComponent() const;

	/** Points the weapon component at the active slot and hands it that slot's ammo. */
	void ApplyActiveSlotToWeapon();

	/** Makes sure slot 0 holds a melee definition. Safe to call repeatedly. */
	void EnsureHands();

	/** Starts (or, with bImmediate, skips) the swap lockout. */
	void StartSwap(bool bImmediate);

	/** Index of the next non-empty slot Step places away from the active one, or INDEX_NONE. */
	int32 FindAdjacentSlot(int32 Step) const;

	/** The three slots, in hotbar order. Always CastleHotbarSlotCount long. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	TArray<FCastleInventorySlot> Slots;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	EHotbarSlot ActiveSlot = EHotbarSlot::Hands;

	/** Keycard ids collected so far. Doors check this by id. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory|Keycards")
	TSet<FName> Keycards;

	/** Transient stand-in for DA_Weapon_Hands, made when no definition asset is available. */
	UPROPERTY(Transient)
	TObjectPtr<UWeaponDefinition> FallbackHands = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Inventory")
	bool bSwapping = false;

private:
	FTimerHandle SwapTimerHandle;
};

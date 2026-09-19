// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/InventoryComponent.h"

#include "Castle.h"
#include "Combat/WeaponComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"

UInventoryComponent::UInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	Slots.SetNum(CastleHotbarSlotCount);
}

void UInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	EnsureHands();

	// Whatever the mission granted is already in the slots by now; this only makes sure the
	// weapon component is pointed at the active one and nothing is stuck mid-swap.
	ApplyActiveSlotToWeapon();
	OnInventoryChanged.Broadcast();
}

void UInventoryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SwapTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

UWeaponDefinition* UInventoryComponent::GetHandsDefinition()
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return nullptr;
	}

	if (!HandsDefinition.IsNull())
	{
		if (UWeaponDefinition* Loaded = HandsDefinition.LoadSynchronous())
		{
			return Loaded;
		}
	}

	if (!FallbackHands)
	{
		// Automation worlds have no content; Hands still have to exist, because every rule in
		// the semantics doc assumes slot 0 is occupied.
		FallbackHands = NewObject<UWeaponDefinition>(this, TEXT("FallbackHands"));
		FallbackHands->DisplayName = NSLOCTEXT("Castle", "WeaponHands", "Fists");
		FallbackHands->ShortName = NSLOCTEXT("Castle", "WeaponHandsShort", "Fists");
		FallbackHands->Slot = EHotbarSlot::Hands;
		FallbackHands->bIsMelee = true;
		FallbackHands->Damage = 15.f;
		FallbackHands->MagazineSize = 0;
		FallbackHands->DefaultReserve = 0;
		FallbackHands->ArmsPoseName = FName(TEXT("Fists"));
	}

	return FallbackHands;
}

void UInventoryComponent::EnsureHands()
{
	if (Slots.Num() != CastleHotbarSlotCount)
	{
		Slots.SetNum(CastleHotbarSlotCount);
	}

	FCastleInventorySlot& Hands = Slots[static_cast<int32>(EHotbarSlot::Hands)];
	if (Hands.Weapon)
	{
		return;
	}

	Hands.Weapon = GetHandsDefinition();
	Hands.Magazine = 0;
	Hands.Reserve = 0;
}

UWeaponComponent* UInventoryComponent::FindWeaponComponent() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->FindComponentByClass<UWeaponComponent>() : nullptr;
}

FCastleInventorySlot UInventoryComponent::GetSlot(EHotbarSlot Slot) const
{
	const int32 Index = static_cast<int32>(Slot);
	return Slots.IsValidIndex(Index) ? Slots[Index] : FCastleInventorySlot();
}

bool UInventoryComponent::IsSlotEmpty(EHotbarSlot Slot) const
{
	return GetSlot(Slot).IsEmpty();
}

UWeaponDefinition* UInventoryComponent::GetActiveWeapon() const
{
	return GetSlot(ActiveSlot).Weapon;
}

void UInventoryComponent::ApplyActiveSlotToWeapon()
{
	UWeaponComponent* Weapon = FindWeaponComponent();
	if (!Weapon)
	{
		return;
	}

	const FCastleInventorySlot Slot = GetSlot(ActiveSlot);
	Weapon->SetInventory(this);
	Weapon->SetActiveWeapon(Slot.Weapon, Slot.Magazine, Slot.Reserve);
}

bool UInventoryComponent::SelectSlot(EHotbarSlot Slot, bool bImmediate)
{
	EnsureHands();

	const int32 Index = static_cast<int32>(Slot);
	if (!Slots.IsValidIndex(Index))
	{
		return false;
	}

	if (Slots[Index].IsEmpty())
	{
		// An empty slot is not a weapon you can hold; pressing its key does nothing at all.
		UE_LOG(LogCastle, Verbose, TEXT("%s: hotbar slot %d is empty."), *GetNameSafe(GetOwner()), Index);
		return false;
	}

	if (Slot == ActiveSlot)
	{
		return false;
	}

	const EHotbarSlot OldSlot = ActiveSlot;
	ActiveSlot = Slot;

	ApplyActiveSlotToWeapon();
	StartSwap(bImmediate);

	OnActiveSlotChanged.Broadcast(OldSlot, ActiveSlot);
	OnInventoryChanged.Broadcast();
	return true;
}

int32 UInventoryComponent::FindAdjacentSlot(int32 Step) const
{
	const int32 Count = Slots.Num();
	if (Count <= 0)
	{
		return INDEX_NONE;
	}

	int32 Index = static_cast<int32>(ActiveSlot);
	for (int32 Tried = 0; Tried < Count - 1; ++Tried)
	{
		Index = (Index + Step + Count) % Count;
		if (!Slots[Index].IsEmpty())
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

bool UInventoryComponent::SelectNextSlot()
{
	const int32 Index = FindAdjacentSlot(1);
	return Index != INDEX_NONE && SelectSlot(static_cast<EHotbarSlot>(Index));
}

bool UInventoryComponent::SelectPreviousSlot()
{
	const int32 Index = FindAdjacentSlot(-1);
	return Index != INDEX_NONE && SelectSlot(static_cast<EHotbarSlot>(Index));
}

void UInventoryComponent::StartSwap(bool bImmediate)
{
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(SwapTimerHandle);
	}

	if (bImmediate || SwapSeconds <= 0.f)
	{
		bSwapping = false;
		return;
	}

	bSwapping = true;

	// With no world (an automation test) the lockout stays up until FinishSwapNow is called,
	// which is exactly what the swap-lockout test wants to drive by hand.
	if (World)
	{
		World->GetTimerManager().SetTimer(
			SwapTimerHandle, this, &UInventoryComponent::FinishSwapNow, SwapSeconds, false);
	}
}

void UInventoryComponent::FinishSwapNow()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SwapTimerHandle);
	}

	if (!bSwapping)
	{
		return;
	}

	bSwapping = false;
	OnInventoryChanged.Broadcast();
}

bool UInventoryComponent::AddWeapon(UWeaponDefinition* Definition)
{
	if (!Definition)
	{
		return false;
	}

	EnsureHands();

	const int32 Index = static_cast<int32>(Definition->Slot);
	if (!Slots.IsValidIndex(Index))
	{
		UE_LOG(LogCastle, Warning, TEXT("%s: %s wants hotbar slot %d, which does not exist."),
			*GetNameSafe(GetOwner()), *Definition->GetName(), Index);
		return false;
	}

	FCastleInventorySlot& Slot = Slots[Index];
	Slot.Weapon = Definition;
	Slot.Magazine = Definition->bIsMelee ? 0 : Definition->MagazineSize;
	Slot.Reserve = Definition->bIsMelee ? 0 : Definition->DefaultReserve;

	UE_LOG(LogCastle, Log, TEXT("%s picked up %s into slot %d."),
		*GetNameSafe(GetOwner()), *Definition->GetName(), Index);

	// Picking a gun up while already holding one must not yank it out of your hands mid-fight.
	const bool bAutoSelect = ActiveSlot == EHotbarSlot::Hands && Definition->Slot != EHotbarSlot::Hands;
	if (bAutoSelect)
	{
		SelectSlot(Definition->Slot);
	}
	else if (Definition->Slot == ActiveSlot)
	{
		ApplyActiveSlotToWeapon();
	}

	OnInventoryChanged.Broadcast();
	return true;
}

bool UInventoryComponent::AddWeaponWithAmmo(UWeaponDefinition* Definition, int32 Magazine, int32 Reserve)
{
	if (!AddWeapon(Definition))
	{
		return false;
	}

	// What this particular pickup carries, which can differ from the definition's defaults - a
	// half-empty gun off a dead guard.
	SetSlotAmmo(Definition->Slot,
		FMath::Clamp(Magazine, 0, FMath::Max(Definition->MagazineSize, 0)), FMath::Max(Reserve, 0));

	if (Definition->Slot == ActiveSlot)
	{
		ApplyActiveSlotToWeapon();
	}
	return true;
}

bool UInventoryComponent::AddAmmoToSlot(EHotbarSlot Slot, int32 Rounds)
{
	const int32 Index = static_cast<int32>(Slot);
	if (Rounds <= 0 || !Slots.IsValidIndex(Index) || !Slots[Index].IsRanged())
	{
		return false;
	}

	Slots[Index].Reserve += Rounds;

	if (Slot == ActiveSlot)
	{
		ApplyActiveSlotToWeapon();
	}

	OnInventoryChanged.Broadcast();
	return true;
}

bool UInventoryComponent::AddAmmo(UWeaponDefinition* Definition, int32 Rounds)
{
	// No definition on an ammo pickup means "whatever is in my hands", which is what a guard's
	// dropped magazine should do.
	const EHotbarSlot Slot = Definition ? Definition->Slot : ActiveSlot;
	return AddAmmoToSlot(Slot, Rounds);
}

void UInventoryComponent::SetSlotAmmo(EHotbarSlot Slot, int32 Magazine, int32 Reserve)
{
	const int32 Index = static_cast<int32>(Slot);
	if (!Slots.IsValidIndex(Index))
	{
		return;
	}

	FCastleInventorySlot& Entry = Slots[Index];
	if (Entry.Magazine == Magazine && Entry.Reserve == Reserve)
	{
		return;
	}

	Entry.Magazine = Magazine;
	Entry.Reserve = Reserve;
	OnInventoryChanged.Broadcast();
}

void UInventoryComponent::Clear()
{
	const EHotbarSlot OldSlot = ActiveSlot;

	Slots.Reset();
	Slots.SetNum(CastleHotbarSlotCount);
	Keycards.Reset();
	ActiveSlot = EHotbarSlot::Hands;

	EnsureHands();

	for (UWeaponDefinition* Definition : StartingSlots)
	{
		if (!Definition || Definition->Slot == EHotbarSlot::Hands)
		{
			continue;
		}
		const int32 Index = static_cast<int32>(Definition->Slot);
		if (!Slots.IsValidIndex(Index))
		{
			continue;
		}
		Slots[Index].Weapon = Definition;
		Slots[Index].Magazine = Definition->bIsMelee ? 0 : Definition->MagazineSize;
		Slots[Index].Reserve = Definition->bIsMelee ? 0 : Definition->DefaultReserve;
	}

	// A clear is not a weapon switch: nothing should be locked out afterwards.
	bSwapping = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SwapTimerHandle);
	}

	ApplyActiveSlotToWeapon();

	if (OldSlot != ActiveSlot)
	{
		OnActiveSlotChanged.Broadcast(OldSlot, ActiveSlot);
	}
	OnInventoryChanged.Broadcast();
}

void UInventoryComponent::ApplyStartingWeapons(const TArray<UWeaponDefinition*>& Weapons)
{
	StartingSlots.Reset();
	for (UWeaponDefinition* Definition : Weapons)
	{
		if (Definition)
		{
			StartingSlots.Add(Definition);
		}
	}

	Clear();
}

bool UInventoryComponent::HasKeycard(FName KeycardId) const
{
	return !KeycardId.IsNone() && Keycards.Contains(KeycardId);
}

bool UInventoryComponent::GiveKeycard(FName KeycardId)
{
	if (KeycardId.IsNone() || Keycards.Contains(KeycardId))
	{
		return false;
	}

	Keycards.Add(KeycardId);
	UE_LOG(LogCastle, Log, TEXT("%s picked up keycard '%s'."),
		*GetNameSafe(GetOwner()), *KeycardId.ToString());

	OnInventoryChanged.Broadcast();
	return true;
}

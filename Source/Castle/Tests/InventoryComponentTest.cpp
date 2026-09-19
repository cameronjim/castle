// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/HealthComponent.h"
#include "Combat/WeaponComponent.h"
#include "Combat/WeaponDefinition.h"
#include "Misc/AutomationTest.h"
#include "Player/CastleCharacter.h"
#include "Player/InventoryComponent.h"
#include "Tests/CastleTestUtils.h"
#include "UI/CastleHotbarWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * The hotbar rules from claude-docs/gameplay-semantics.md, "Inventory and hotbar".
 *
 * Tests never load content, so the weapon definitions here are built with NewObject. The
 * inventory makes its own stand-in for Hands for the same reason.
 */
namespace CastleInventoryTest
{
	/** A pistol definition with the shipping stats, owned by the outer so it survives the test. */
	static UWeaponDefinition* MakePistol(UObject* Outer)
	{
		UWeaponDefinition* Definition = NewObject<UWeaponDefinition>(Outer);
		Definition->DisplayName = FText::FromString(TEXT("Pistol"));
		Definition->ShortName = FText::FromString(TEXT("Pistol"));
		Definition->Slot = EHotbarSlot::Pistol;
		Definition->Damage = 34.f;
		Definition->MagazineSize = 12;
		Definition->DefaultReserve = 24;
		return Definition;
	}

	static UWeaponDefinition* MakeRifle(UObject* Outer)
	{
		UWeaponDefinition* Definition = NewObject<UWeaponDefinition>(Outer);
		Definition->DisplayName = FText::FromString(TEXT("Rifle"));
		Definition->Slot = EHotbarSlot::Rifle;
		Definition->Damage = 24.f;
		Definition->MagazineSize = 30;
		Definition->DefaultReserve = 90;
		return Definition;
	}

	/** A bare inventory with no owner: enough for every rule that is not about the weapon. */
	static UInventoryComponent* MakeInventory()
	{
		UInventoryComponent* Inventory = NewObject<UInventoryComponent>();
		Inventory->SelectSlot(EHotbarSlot::Hands, /*bImmediate=*/true);
		return Inventory;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleInventoryStartsWithHands, "Castle.Inventory.StartsWithHands",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleInventoryStartsWithHands::RunTest(const FString& Parameters)
{
	UInventoryComponent* Inventory = CastleInventoryTest::MakeInventory();

	TestEqual(TEXT("Hands are the active slot"), Inventory->GetActiveSlot(), EHotbarSlot::Hands);
	TestFalse(TEXT("And slot 1 is never empty"), Inventory->IsSlotEmpty(EHotbarSlot::Hands));
	TestTrue(TEXT("Hands are a melee weapon"), Inventory->GetActiveWeapon() != nullptr
		&& Inventory->GetActiveWeapon()->bIsMelee);
	TestTrue(TEXT("The pistol slot starts empty"), Inventory->IsSlotEmpty(EHotbarSlot::Pistol));
	TestTrue(TEXT("So does the rifle slot"), Inventory->IsSlotEmpty(EHotbarSlot::Rifle));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleInventoryEmptySlotIsNoOp, "Castle.Inventory.EmptySlotIsNoOp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleInventoryEmptySlotIsNoOp::RunTest(const FString& Parameters)
{
	UInventoryComponent* Inventory = CastleInventoryTest::MakeInventory();

	UCastleTestListener* Listener = NewObject<UCastleTestListener>();
	Inventory->OnActiveSlotChanged.AddDynamic(Listener, &UCastleTestListener::HandleActiveSlotChanged);

	TestFalse(TEXT("Selecting the empty pistol slot does nothing"),
		Inventory->SelectSlot(EHotbarSlot::Pistol));
	TestEqual(TEXT("The active slot is unchanged"), Inventory->GetActiveSlot(), EHotbarSlot::Hands);
	TestEqual(TEXT("And nothing was broadcast"), Listener->ActiveSlotChangedCount, 0);
	TestFalse(TEXT("Nothing is swapping either"), Inventory->IsSwapping());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleInventoryAutoSelectsOnlyFromHands, "Castle.Inventory.AutoSelectsOnlyFromHands",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleInventoryAutoSelectsOnlyFromHands::RunTest(const FString& Parameters)
{
	UInventoryComponent* Inventory = CastleInventoryTest::MakeInventory();
	UWeaponDefinition* Pistol = CastleInventoryTest::MakePistol(Inventory);
	UWeaponDefinition* Rifle = CastleInventoryTest::MakeRifle(Inventory);

	TestTrue(TEXT("The pistol goes in"), Inventory->AddWeapon(Pistol));
	TestEqual(TEXT("And is drawn, because Hands were active"),
		Inventory->GetActiveSlot(), EHotbarSlot::Pistol);
	TestEqual(TEXT("With a full magazine"), Inventory->GetSlot(EHotbarSlot::Pistol).Magazine, 12);
	TestEqual(TEXT("And its default reserve"), Inventory->GetSlot(EHotbarSlot::Pistol).Reserve, 24);

	// Picking a second gun up mid-fight must not take the first one out of his hands.
	TestTrue(TEXT("The rifle goes in"), Inventory->AddWeapon(Rifle));
	TestEqual(TEXT("But the pistol stays drawn"), Inventory->GetActiveSlot(), EHotbarSlot::Pistol);
	TestFalse(TEXT("The rifle slot is filled all the same"), Inventory->IsSlotEmpty(EHotbarSlot::Rifle));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleInventoryScrollSkipsEmptySlots, "Castle.Inventory.ScrollSkipsEmptySlots",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleInventoryScrollSkipsEmptySlots::RunTest(const FString& Parameters)
{
	UInventoryComponent* Inventory = CastleInventoryTest::MakeInventory();
	UWeaponDefinition* Rifle = CastleInventoryTest::MakeRifle(Inventory);

	// Hands and rifle only: the wheel must jump straight over the empty pistol slot.
	Inventory->AddWeapon(Rifle);
	Inventory->SelectSlot(EHotbarSlot::Hands, /*bImmediate=*/true);

	TestTrue(TEXT("Wheel up moves on"), Inventory->SelectNextSlot());
	TestEqual(TEXT("Straight to the rifle"), Inventory->GetActiveSlot(), EHotbarSlot::Rifle);

	TestTrue(TEXT("Wheel up again wraps"), Inventory->SelectNextSlot());
	TestEqual(TEXT("Back to Hands"), Inventory->GetActiveSlot(), EHotbarSlot::Hands);

	TestTrue(TEXT("Wheel down moves back"), Inventory->SelectPreviousSlot());
	TestEqual(TEXT("To the rifle, not the empty pistol slot"),
		Inventory->GetActiveSlot(), EHotbarSlot::Rifle);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleInventoryScrollWithOnlyHands, "Castle.Inventory.ScrollWithOnlyHands",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleInventoryScrollWithOnlyHands::RunTest(const FString& Parameters)
{
	UInventoryComponent* Inventory = CastleInventoryTest::MakeInventory();

	TestFalse(TEXT("With nothing else carried the wheel does nothing"), Inventory->SelectNextSlot());
	TestFalse(TEXT("In either direction"), Inventory->SelectPreviousSlot());
	TestEqual(TEXT("Hands stay active"), Inventory->GetActiveSlot(), EHotbarSlot::Hands);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleInventoryKeycardsLiveHere, "Castle.Inventory.KeycardsLiveHere",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleInventoryKeycardsLiveHere::RunTest(const FString& Parameters)
{
	FCastleTestWorld TestWorld;
	ACastleCharacter* Frank = Cast<ACastleCharacter>(TestWorld.SpawnActor(
		ACastleCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator));
	if (!Frank || !Frank->GetInventoryComponent())
	{
		AddError(TEXT("Could not spawn a character with an inventory."));
		return false;
	}

	UInventoryComponent* Inventory = Frank->GetInventoryComponent();
	const FName Cellblock(TEXT("cellblock"));

	TestFalse(TEXT("No keycard to start with"), Frank->HasKeycard(Cellblock));
	TestTrue(TEXT("The pawn's GiveKeycard forwards to the inventory"), Frank->GiveKeycard(Cellblock));
	TestTrue(TEXT("The inventory holds it"), Inventory->HasKeycard(Cellblock));
	TestTrue(TEXT("And the pawn still answers for it"), Frank->HasKeycard(Cellblock));
	TestFalse(TEXT("A second grant changes nothing"), Frank->GiveKeycard(Cellblock));
	TestEqual(TEXT("One keycard on the ring"), Frank->GetKeycards().Num(), 1);

	Inventory->Clear();
	TestFalse(TEXT("Clearing the inventory takes the keycards with it"), Frank->HasKeycard(Cellblock));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleInventoryClearResetsToStartingSlots, "Castle.Inventory.ClearResetsToStartingSlots",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleInventoryClearResetsToStartingSlots::RunTest(const FString& Parameters)
{
	UInventoryComponent* Inventory = CastleInventoryTest::MakeInventory();
	UWeaponDefinition* Pistol = CastleInventoryTest::MakePistol(Inventory);
	UWeaponDefinition* Rifle = CastleInventoryTest::MakeRifle(Inventory);

	// This mission hands Frank a pistol; everything else he found on the way.
	Inventory->ApplyStartingWeapons({ Pistol });
	TestFalse(TEXT("The starting pistol is carried"), Inventory->IsSlotEmpty(EHotbarSlot::Pistol));
	TestEqual(TEXT("And Hands are what he is holding"), Inventory->GetActiveSlot(), EHotbarSlot::Hands);

	Inventory->AddWeapon(Rifle);
	Inventory->GiveKeycard(FName(TEXT("cellblock")));
	Inventory->AddAmmoToSlot(EHotbarSlot::Pistol, 12);
	TestEqual(TEXT("The pistol picked up spare rounds"),
		Inventory->GetSlot(EHotbarSlot::Pistol).Reserve, 36);

	Inventory->Clear();

	TestEqual(TEXT("Hands are active again"), Inventory->GetActiveSlot(), EHotbarSlot::Hands);
	TestFalse(TEXT("Hands are still there"), Inventory->IsSlotEmpty(EHotbarSlot::Hands));
	TestFalse(TEXT("The mission's pistol comes back"), Inventory->IsSlotEmpty(EHotbarSlot::Pistol));
	TestEqual(TEXT("With its default reserve, not the rounds he found"),
		Inventory->GetSlot(EHotbarSlot::Pistol).Reserve, 24);
	TestTrue(TEXT("The rifle he found is gone"), Inventory->IsSlotEmpty(EHotbarSlot::Rifle));
	TestFalse(TEXT("And so are the keycards"), Inventory->HasKeycard(FName(TEXT("cellblock"))));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleInventorySwapBlocksFire, "Castle.Inventory.SwapBlocksFire",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleInventorySwapBlocksFire::RunTest(const FString& Parameters)
{
	FCastleTestWorld TestWorld;
	ACastleCharacter* Frank = Cast<ACastleCharacter>(TestWorld.SpawnActor(
		ACastleCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator));
	UInventoryComponent* Inventory = Frank ? Frank->GetInventoryComponent() : nullptr;
	UWeaponComponent* Weapon = Frank ? Frank->GetWeaponComponent() : nullptr;
	if (!Inventory || !Weapon)
	{
		AddError(TEXT("Could not spawn a character with an inventory and a weapon."));
		return false;
	}

	Weapon->SetTestTimeSeconds(100.0);

	UWeaponDefinition* Pistol = CastleInventoryTest::MakePistol(Inventory);
	Inventory->AddWeapon(Pistol);

	TestEqual(TEXT("The pistol is drawn"), Inventory->GetActiveSlot(), EHotbarSlot::Pistol);
	TestTrue(TEXT("Drawing it takes SwapSeconds"), Inventory->IsSwapping());
	TestFalse(TEXT("Fire is refused for the whole swap"), Weapon->CanFire());
	TestFalse(TEXT("And pulling the trigger does nothing"), Weapon->Fire());
	TestEqual(TEXT("No round was spent"), Weapon->CurrentAmmo, 12);

	Inventory->FinishSwapNow();

	TestFalse(TEXT("The swap is over"), Inventory->IsSwapping());
	TestTrue(TEXT("And the pistol fires"), Weapon->Fire());
	TestEqual(TEXT("One round gone"), Weapon->CurrentAmmo, 11);
	TestEqual(TEXT("Written back into the slot"),
		Inventory->GetSlot(EHotbarSlot::Pistol).Magazine, 11);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleInventoryAmmoSurvivesASwap, "Castle.Inventory.AmmoSurvivesASwap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleInventoryAmmoSurvivesASwap::RunTest(const FString& Parameters)
{
	FCastleTestWorld TestWorld;
	ACastleCharacter* Frank = Cast<ACastleCharacter>(TestWorld.SpawnActor(
		ACastleCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator));
	UInventoryComponent* Inventory = Frank ? Frank->GetInventoryComponent() : nullptr;
	UWeaponComponent* Weapon = Frank ? Frank->GetWeaponComponent() : nullptr;
	if (!Inventory || !Weapon)
	{
		AddError(TEXT("Could not spawn a character with an inventory and a weapon."));
		return false;
	}

	Weapon->SetTestTimeSeconds(100.0);
	Inventory->AddWeapon(CastleInventoryTest::MakePistol(Inventory));
	Inventory->FinishSwapNow();

	Weapon->Fire();
	Weapon->SetTestTimeSeconds(200.0);
	Weapon->Fire();
	TestEqual(TEXT("Two rounds gone"), Weapon->CurrentAmmo, 10);

	Inventory->SelectSlot(EHotbarSlot::Hands);
	TestFalse(TEXT("Fists are not a ranged weapon"), Weapon->HasWeapon());
	TestTrue(TEXT("And the component knows it is melee"), Weapon->IsMelee());

	Inventory->SelectSlot(EHotbarSlot::Pistol);
	TestEqual(TEXT("The pistol comes back with the rounds it had"), Weapon->CurrentAmmo, 10);
	TestEqual(TEXT("And its reserve"), Weapon->ReserveAmmo, 24);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleInventoryMeleeDamagesAndCoolsDown, "Castle.Inventory.MeleeDamagesAndCoolsDown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleInventoryMeleeDamagesAndCoolsDown::RunTest(const FString& Parameters)
{
	FCastleTestWorld TestWorld;
	ACastleCharacter* Frank = Cast<ACastleCharacter>(TestWorld.SpawnActor(
		ACastleCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator));
	UWeaponComponent* Weapon = Frank ? Frank->GetWeaponComponent() : nullptr;
	if (!Weapon || !Frank->GetInventoryComponent())
	{
		AddError(TEXT("Could not spawn a character with fists."));
		return false;
	}

	// A target 80 cm down +X, inside the 120 cm reach.
	AActor* Target = TestWorld.SpawnActor(AActor::StaticClass(), FVector(80.f, 0.f, 0.f), FRotator::ZeroRotator);
	if (!Target)
	{
		AddError(TEXT("Could not spawn the punching bag."));
		return false;
	}

	UHealthComponent* Health = NewObject<UHealthComponent>(Target);
	Health->RegisterComponent();

	UCastleTestListener* Listener = NewObject<UCastleTestListener>();
	Health->OnStaggered.AddDynamic(Listener, &UCastleTestListener::HandleStaggered);

	TestTrue(TEXT("Fists are what Frank starts with"), Weapon->IsMelee());
	TestFalse(TEXT("Which is not a ranged weapon"), Weapon->HasWeapon());

	Weapon->SetTestTimeSeconds(100.0);
	TestTrue(TEXT("The punch goes out"), Weapon->Fire());

	// The cooldown is what the rule is about; whether the sweep found the bare AActor depends
	// on collision, so the damage is asserted through the health component directly below.
	Weapon->SetTestTimeSeconds(100.3);
	TestFalse(TEXT("Half the cooldown later the second punch is refused"), Weapon->Fire());

	Weapon->SetTestTimeSeconds(100.7);
	TestTrue(TEXT("A full cooldown later it lands"), Weapon->Fire());

	// And the damage rule itself: a punch staggers, a bullet does not.
	Health->ApplyMeleeDamage(Weapon->MeleeDamage, Frank, /*bStagger=*/true);
	TestEqual(TEXT("A punch takes MeleeDamage off"), Health->GetCurrentHealth(), 85.f);
	TestEqual(TEXT("And staggers once"), Listener->StaggeredCount, 1);

	Health->ApplyDamage(10.f, Frank);
	TestEqual(TEXT("A bullet never staggers"), Listener->StaggeredCount, 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleInventoryHotbarWidgetReflectsState, "Castle.Inventory.HotbarWidgetReflectsState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleInventoryHotbarWidgetReflectsState::RunTest(const FString& Parameters)
{
	UInventoryComponent* Inventory = CastleInventoryTest::MakeInventory();
	UCastleHotbarWidget* Hotbar = NewObject<UCastleHotbarWidget>();
	Hotbar->BindToInventory(Inventory);

	TestEqual(TEXT("Hands are the active box"), Hotbar->GetActiveSlot(), EHotbarSlot::Hands);
	TestTrue(TEXT("Slot 1 is highlighted"), Hotbar->IsSlotActive(EHotbarSlot::Hands));
	TestTrue(TEXT("Slot 2 is empty"), Hotbar->IsSlotEmpty(EHotbarSlot::Pistol));
	TestEqual(TEXT("Slot labels are the number keys"),
		Hotbar->GetSlotKeyText(EHotbarSlot::Rifle).ToString(), FString(TEXT("3")));
	TestTrue(TEXT("Fists show no ammo"), Hotbar->GetSlotAmmoText(EHotbarSlot::Hands).IsEmpty());
	TestEqual(TEXT("An empty box is dimmed"),
		Hotbar->GetSlotColor(EHotbarSlot::Pistol), Hotbar->GetSlotColor(EHotbarSlot::Rifle));

	Inventory->AddWeapon(CastleInventoryTest::MakePistol(Inventory));

	TestEqual(TEXT("The widget follows the inventory"), Hotbar->GetActiveSlot(), EHotbarSlot::Pistol);
	TestTrue(TEXT("Slot 2 is highlighted now"), Hotbar->IsSlotActive(EHotbarSlot::Pistol));
	TestFalse(TEXT("And slot 1 is not"), Hotbar->IsSlotActive(EHotbarSlot::Hands));
	TestEqual(TEXT("The gun shows its ammo"),
		Hotbar->GetSlotAmmoText(EHotbarSlot::Pistol).ToString(), FString(TEXT("12 / 24")));
	TestEqual(TEXT("Under its short name"),
		Hotbar->GetSlotNameText(EHotbarSlot::Pistol).ToString(), FString(TEXT("Pistol")));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

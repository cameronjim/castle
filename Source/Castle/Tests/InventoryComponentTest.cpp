// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/ArrowDefinition.h"
#include "Combat/BowDefinition.h"
#include "Combat/HealthComponent.h"
#include "Combat/WeaponComponent.h"
#include "Misc/AutomationTest.h"
#include "Mission/MissionDefinition.h"
#include "Player/CastleCharacter.h"
#include "Player/InventoryComponent.h"
#include "Tests/CastleTestUtils.h"
#include "UI/CastleHotbarWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * The quiver rules from claude-docs/gameplay-semantics.md, "bow and arrows": standard arrows are
 * always slot 1, counts never pass a cap, number keys and the wheel pick filled slots only, and
 * the mission defines the starting bow and arrows. Definitions are built with NewObject.
 */
namespace CastleInventoryTest
{
	static UArrowDefinition* MakeArrow(UObject* Outer, int32 Slot, int32 Cap, EArrowHitEffect Effect,
		const TCHAR* ShortName)
	{
		UArrowDefinition* Arrow = NewObject<UArrowDefinition>(Outer);
		Arrow->Slot = Slot;
		Arrow->Cap = Cap;
		Arrow->OnHitEffect = Effect;
		Arrow->ShortName = FText::FromString(ShortName);
		Arrow->DisplayName = FText::FromString(ShortName);
		return Arrow;
	}

	static UArrowDefinition* MakeStandard(UObject* Outer)
	{
		return MakeArrow(Outer, 1, 30, EArrowHitEffect::None, TEXT("Arrow"));
	}

	static UArrowDefinition* MakeGrapple(UObject* Outer)
	{
		return MakeArrow(Outer, 2, 6, EArrowHitEffect::Grapple, TEXT("Grapple"));
	}

	static FCastleQuiverSlot Grant(UArrowDefinition* Arrow, int32 Count)
	{
		FCastleQuiverSlot Slot;
		Slot.Arrow = Arrow;
		Slot.Count = Count;
		return Slot;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleInventoryStandardInSlotOne, "Castle.Inventory.StandardAlwaysInSlotOne",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleInventoryStandardInSlotOne::RunTest(const FString& Parameters)
{
	UInventoryComponent* Inventory = NewObject<UInventoryComponent>();
	Inventory->Clear();

	TestEqual(TEXT("Slot 1 is active"), Inventory->GetActiveArrowSlot(), 1);
	TestFalse(TEXT("And holds standard arrows"), Inventory->IsArrowSlotEmpty(1));
	TestEqual(TEXT("Even at zero"), Inventory->GetArrowCount(1), 0);
	TestFalse(TEXT("No bow until one is granted"), Inventory->HasBow());
	for (int32 Slot = 2; Slot <= CastleQuiverSlotCount; ++Slot)
	{
		TestTrue(FString::Printf(TEXT("Slot %d starts empty"), Slot), Inventory->IsArrowSlotEmpty(Slot));
	}
	TestTrue(TEXT("Out of range reads as empty"), Inventory->IsArrowSlotEmpty(7));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleInventoryEmptySlotIsNoOp, "Castle.Inventory.EmptySlotIsNoOp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleInventoryEmptySlotIsNoOp::RunTest(const FString& Parameters)
{
	UInventoryComponent* Inventory = NewObject<UInventoryComponent>();
	Inventory->Clear();
	UCastleTestListener* Listener = NewObject<UCastleTestListener>();
	Inventory->OnActiveArrowSlotChanged.AddDynamic(Listener, &UCastleTestListener::HandleActiveSlotChanged);

	TestFalse(TEXT("Key 3 on an empty slot does nothing"), Inventory->SelectArrowSlot(3));
	TestFalse(TEXT("Nor does a key that is no slot"), Inventory->SelectArrowSlot(9));
	TestFalse(TEXT("Nor the slot already active"), Inventory->SelectArrowSlot(1));
	TestEqual(TEXT("Still slot 1"), Inventory->GetActiveArrowSlot(), 1);
	TestEqual(TEXT("And nothing was broadcast"), Listener->ActiveSlotChangedCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleInventorySelectionSkipsEmpty, "Castle.Inventory.SelectionSkipsEmptySlots",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleInventorySelectionSkipsEmpty::RunTest(const FString& Parameters)
{
	using namespace CastleInventoryTest;
	UInventoryComponent* Inventory = NewObject<UInventoryComponent>();
	Inventory->Clear();
	Inventory->AddArrows(MakeArrow(Inventory, 4, 3, EArrowHitEffect::Putty, TEXT("Putty")), 3);
	UCastleTestListener* Listener = NewObject<UCastleTestListener>();
	Inventory->OnActiveArrowSlotChanged.AddDynamic(Listener, &UCastleTestListener::HandleActiveSlotChanged);

	TestTrue(TEXT("Wheel up moves on"), Inventory->SelectNextArrowSlot());
	TestEqual(TEXT("Straight past the empty slots 2 and 3 to 4"), Inventory->GetActiveArrowSlot(), 4);
	TestEqual(TEXT("Broadcast old slot"), Listener->LastOldArrowSlot, 1);
	TestEqual(TEXT("And new slot"), Listener->LastNewArrowSlot, 4);
	TestTrue(TEXT("Wheel up again wraps"), Inventory->SelectNextArrowSlot());
	TestEqual(TEXT("Back to standard arrows"), Inventory->GetActiveArrowSlot(), 1);
	TestTrue(TEXT("Wheel down goes back"), Inventory->SelectPreviousArrowSlot());
	TestEqual(TEXT("To slot 4, not an empty one"), Inventory->GetActiveArrowSlot(), 4);
	TestTrue(TEXT("Key 1 picks standard arrows"), Inventory->SelectArrowSlot(1));
	TestEqual(TEXT("Four changes, each broadcast once"), Listener->ActiveSlotChangedCount, 4);

	UInventoryComponent* OnlyStandard = NewObject<UInventoryComponent>();
	OnlyStandard->Clear();
	TestFalse(TEXT("With only standard arrows the wheel does nothing"), OnlyStandard->SelectNextArrowSlot());
	TestFalse(TEXT("Either way"), OnlyStandard->SelectPreviousArrowSlot());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleInventoryCountsAndCaps, "Castle.Inventory.CountsClampToCap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleInventoryCountsAndCaps::RunTest(const FString& Parameters)
{
	using namespace CastleInventoryTest;
	UInventoryComponent* Inventory = NewObject<UInventoryComponent>();
	Inventory->Clear();
	UArrowDefinition* Standard = MakeStandard(Inventory);
	UArrowDefinition* Grapple = MakeGrapple(Inventory);

	TestEqual(TEXT("Forty offered, thirty fit"), Inventory->AddArrows(Standard, 40), 30);
	TestEqual(TEXT("Standard at its cap"), Inventory->GetArrowCount(1), 30);
	TestEqual(TEXT("A full slot takes nothing more"), Inventory->AddArrows(Standard, 1), 0);
	TestEqual(TEXT("Grapple arrows go in their own slot"), Inventory->AddArrows(Grapple, 6), 6);
	TestEqual(TEXT("Found by effect"), Inventory->FindArrowSlotByEffect(EArrowHitEffect::Grapple), 2);
	TestEqual(TEXT("No putty slot"), Inventory->FindArrowSlotByEffect(EArrowHitEffect::Putty), INDEX_NONE);

	TestTrue(TEXT("One grapple arrow spent"), Inventory->ConsumeArrow(2));
	TestEqual(TEXT("Five left"), Inventory->GetArrowCount(2), 5);
	Inventory->SetArrowCount(2, 0);
	TestFalse(TEXT("None left, none spent"), Inventory->ConsumeArrow(2));
	TestFalse(TEXT("An empty slot spends nothing"), Inventory->ConsumeArrow(5));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleInventoryKeycardsLiveHere, "Castle.Inventory.KeycardsLiveHere",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleInventoryKeycardsLiveHere::RunTest(const FString& Parameters)
{
	FCastleTestWorld TestWorld;
	ACastleCharacter* Kate = Cast<ACastleCharacter>(TestWorld.SpawnActor(
		ACastleCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator));
	if (!Kate || !Kate->GetInventoryComponent())
	{
		AddError(TEXT("Could not spawn a character with an inventory."));
		return false;
	}

	UInventoryComponent* Inventory = Kate->GetInventoryComponent();
	const FName Cellblock(TEXT("cellblock"));

	TestFalse(TEXT("No keycard to start with"), Kate->HasKeycard(Cellblock));
	TestTrue(TEXT("The pawn's GiveKeycard forwards to the inventory"), Kate->GiveKeycard(Cellblock));
	TestTrue(TEXT("The inventory holds it"), Inventory->HasKeycard(Cellblock));
	TestFalse(TEXT("A second grant changes nothing"), Kate->GiveKeycard(Cellblock));
	TestEqual(TEXT("One keycard on the ring"), Kate->GetKeycards().Num(), 1);

	Inventory->Clear();
	TestFalse(TEXT("Clearing the inventory takes the keycards with it"), Kate->HasKeycard(Cellblock));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleInventoryClearResetsToStartingQuiver, "Castle.Inventory.ClearResetsToStartingQuiver",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleInventoryClearResetsToStartingQuiver::RunTest(const FString& Parameters)
{
	using namespace CastleInventoryTest;
	UInventoryComponent* Inventory = NewObject<UInventoryComponent>();
	UBowDefinition* Bow = NewObject<UBowDefinition>(Inventory);
	UArrowDefinition* Standard = MakeStandard(Inventory);
	UArrowDefinition* Grapple = MakeGrapple(Inventory);

	Inventory->ApplyStartingQuiver(Bow, { Grant(Standard, 30), Grant(Grapple, 6) });
	TestTrue(TEXT("The chapter's bow is carried"), Inventory->GetBow() == Bow);

	Inventory->ConsumeArrow(1);
	Inventory->SelectArrowSlot(2);
	Inventory->AddArrows(MakeArrow(Inventory, 3, 3, EArrowHitEffect::Smoke, TEXT("Smoke")), 2);
	Inventory->GiveKeycard(FName(TEXT("cellblock")));
	Inventory->GiveBow(nullptr);

	Inventory->Clear();

	TestTrue(TEXT("The bow comes back"), Inventory->GetBow() == Bow);
	TestEqual(TEXT("Standard arrows back to the grant"), Inventory->GetArrowCount(1), 30);
	TestEqual(TEXT("Grapple back to the grant"), Inventory->GetArrowCount(2), 6);
	TestTrue(TEXT("The smoke arrows found on the way are gone"), Inventory->IsArrowSlotEmpty(3));
	TestEqual(TEXT("Slot 1 active again"), Inventory->GetActiveArrowSlot(), 1);
	TestFalse(TEXT("And the keycards are gone"), Inventory->HasKeycard(FName(TEXT("cellblock"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleInventoryMissionStartGrantsQuiver, "Castle.Inventory.MissionStartGrantsQuiver",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleInventoryMissionStartGrantsQuiver::RunTest(const FString& Parameters)
{
	using namespace CastleInventoryTest;
	// CH01's shape: DA_Bow_Kate, 30 standard, 6 grapple. Built in code; tests never load content.
	UMissionDefinition* Chapter = NewObject<UMissionDefinition>();
	UBowDefinition* Bow = NewObject<UBowDefinition>(Chapter);
	Bow->FullDrawSeconds = 0.8f;
	UArrowDefinition* Standard = MakeStandard(Chapter);
	UArrowDefinition* Grapple = MakeGrapple(Chapter);
	Chapter->StartingBow = Bow;
	FCastleArrowGrant StandardGrant;
	StandardGrant.Arrow = Standard;
	StandardGrant.Count = 30;
	FCastleArrowGrant GrappleGrant;
	GrappleGrant.Arrow = Grapple;
	GrappleGrant.Count = 6;
	Chapter->StartingArrows.Add(StandardGrant);
	Chapter->StartingArrows.Add(GrappleGrant);

	UInventoryComponent* Inventory = NewObject<UInventoryComponent>();
	Inventory->ApplyMissionStart(Chapter);

	TestTrue(TEXT("Kate starts with the chapter's bow"), Inventory->GetBow() == Bow);
	TestTrue(TEXT("Standard arrows in slot 1"), Inventory->GetArrowSlot(1).Arrow == Standard);
	TestEqual(TEXT("Thirty of them"), Inventory->GetArrowCount(1), 30);
	TestTrue(TEXT("Grapple arrows in slot 2"), Inventory->GetArrowSlot(2).Arrow == Grapple);
	TestEqual(TEXT("Six of them"), Inventory->GetArrowCount(2), 6);

	UMissionDefinition* Bare = NewObject<UMissionDefinition>();
	Inventory->ApplyMissionStart(Bare);
	TestFalse(TEXT("A chapter that grants nothing leaves her bowless"), Inventory->HasBow());
	TestFalse(TEXT("With slot 1 still standard arrows"), Inventory->IsArrowSlotEmpty(1));
	TestEqual(TEXT("At zero"), Inventory->GetArrowCount(1), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleInventoryHandsWithoutBow, "Castle.Inventory.HandsWithoutBow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleInventoryHandsWithoutBow::RunTest(const FString& Parameters)
{
	FCastleTestWorld TestWorld;
	ACastleCharacter* Kate = Cast<ACastleCharacter>(TestWorld.SpawnActor(
		ACastleCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator));
	UWeaponComponent* Weapon = Kate ? Kate->GetWeaponComponent() : nullptr;
	if (!Weapon || !Kate->GetInventoryComponent())
	{
		AddError(TEXT("Could not spawn a character with fists."));
		return false;
	}

	AActor* Target = TestWorld.SpawnActor(AActor::StaticClass(), FVector(80.f, 0.f, 0.f), FRotator::ZeroRotator);
	UHealthComponent* Health = Target ? NewObject<UHealthComponent>(Target) : nullptr;
	if (!Health)
	{
		AddError(TEXT("Could not spawn the punching bag."));
		return false;
	}
	Health->RegisterComponent();
	UCastleTestListener* Listener = NewObject<UCastleTestListener>();
	Health->OnStaggered.AddDynamic(Listener, &UCastleTestListener::HandleStaggered);

	TestFalse(TEXT("No bow"), Kate->GetInventoryComponent()->HasBow());
	TestTrue(TEXT("So left click is Hands"), Weapon->IsMelee());
	TestFalse(TEXT("Which is not a ranged weapon"), Weapon->HasWeapon());

	Weapon->SetTestTimeSeconds(100.0);
	TestTrue(TEXT("The punch goes out"), Weapon->Fire());
	Weapon->SetTestTimeSeconds(100.3);
	TestFalse(TEXT("Half the cooldown later the second punch is refused"), Weapon->Fire());
	Weapon->SetTestTimeSeconds(100.7);
	TestTrue(TEXT("A full cooldown later it lands"), Weapon->Fire());

	Health->ApplyMeleeDamage(Weapon->MeleeDamage, Kate, /*bStagger=*/true);
	TestEqual(TEXT("A punch takes MeleeDamage off"), Health->GetCurrentHealth(), 85.f);
	TestEqual(TEXT("And staggers once"), Listener->StaggeredCount, 1);
	Health->ApplyDamage(10.f, Kate);
	TestEqual(TEXT("A bullet never staggers"), Listener->StaggeredCount, 1);
	Health->Stagger(Kate);
	TestEqual(TEXT("An arrow hit staggers through the same path"), Listener->StaggeredCount, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleInventoryHotbarWidgetReflectsState, "Castle.Inventory.HotbarWidgetReflectsState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleInventoryHotbarWidgetReflectsState::RunTest(const FString& Parameters)
{
	using namespace CastleInventoryTest;
	UInventoryComponent* Inventory = NewObject<UInventoryComponent>();
	Inventory->ApplyStartingQuiver(NewObject<UBowDefinition>(Inventory),
		{ Grant(MakeStandard(Inventory), 30), Grant(MakeGrapple(Inventory), 6) });
	UCastleHotbarWidget* Hotbar = NewObject<UCastleHotbarWidget>();
	Hotbar->BindToInventory(Inventory);

	TestEqual(TEXT("Slot 1 is the active box"), Hotbar->GetActiveSlot(), 1);
	TestTrue(TEXT("And highlighted"), Hotbar->IsSlotActive(1));
	TestEqual(TEXT("Standard arrows show a bare count"), Hotbar->GetSlotCountText(1).ToString(), FString(TEXT("30")));
	TestEqual(TEXT("Grapple arrows show count and cap"), Hotbar->GetSlotCountText(2).ToString(), FString(TEXT("6/6")));
	TestEqual(TEXT("Under their short name"), Hotbar->GetSlotNameText(2).ToString(), FString(TEXT("Grapple")));
	TestEqual(TEXT("Labels are the number keys"), Hotbar->GetSlotKeyText(6).ToString(), FString(TEXT("6")));
	TestTrue(TEXT("Slot 3 is empty"), Hotbar->IsSlotEmpty(3));
	TestTrue(TEXT("An empty box shows no count"), Hotbar->GetSlotCountText(3).IsEmpty());
	TestEqual(TEXT("Empty boxes are dimmed alike"), Hotbar->GetSlotColor(3), Hotbar->GetSlotColor(6));

	Inventory->SelectArrowSlot(2);
	Inventory->ConsumeArrow(2);
	TestTrue(TEXT("The widget follows the inventory"), Hotbar->IsSlotActive(2) && !Hotbar->IsSlotActive(1));
	TestEqual(TEXT("And the count"), Hotbar->GetSlotCountText(2).ToString(), FString(TEXT("5/6")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

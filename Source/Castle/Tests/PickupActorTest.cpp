// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/ArrowDefinition.h"
#include "Combat/BowDefinition.h"
#include "Misc/AutomationTest.h"
#include "Player/CastleCharacter.h"
#include "Player/InventoryComponent.h"
#include "Tests/CastleTestUtils.h"
#include "World/Interactable.h"
#include "World/PickupActor.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CastlePickupTest
{
	static APickupActor* SpawnPickup(const FCastleTestWorld& TestWorld, EPickupType Type)
	{
		APickupActor* Pickup = Cast<APickupActor>(
			TestWorld.SpawnActor(APickupActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator));
		if (Pickup)
		{
			Pickup->PickupType = Type;
		}
		return Pickup;
	}

	static ACastleCharacter* SpawnPlayer(const FCastleTestWorld& TestWorld)
	{
		return Cast<ACastleCharacter>(
			TestWorld.SpawnActor(ACastleCharacter::StaticClass(), FVector(500.f, 0.f, 0.f), FRotator::ZeroRotator));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastlePickupBowGivesTheBow, "Castle.Pickup.BowGivesTheBow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastlePickupBowGivesTheBow::RunTest(const FString& Parameters)
{
	FCastleTestWorld TestWorld;
	ACastleCharacter* Player = CastlePickupTest::SpawnPlayer(TestWorld);
	APickupActor* Pickup = CastlePickupTest::SpawnPickup(TestWorld, EPickupType::Bow);
	UInventoryComponent* Inventory = Player ? Player->GetInventoryComponent() : nullptr;
	if (!Inventory || !Pickup)
	{
		AddError(TEXT("Failed to spawn the player or the pickup."));
		return false;
	}

	TestFalse(TEXT("Kate starts without a bow"), Inventory->HasBow());
	TestFalse(TEXT("A bow pickup with no bow set applies nothing"), Pickup->ApplyTo(Player));

	UBowDefinition* Bow = NewObject<UBowDefinition>(Pickup);
	Pickup->Bow = Bow;
	TestTrue(TEXT("The pickup applies"), Pickup->ApplyTo(Player));
	TestTrue(TEXT("She carries that bow"), Inventory->GetBow() == Bow);
	TestTrue(TEXT("The pickup destroys itself"), Pickup->IsActorBeingDestroyed() || !IsValid(Pickup));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastlePickupKeycardJoinsTheRing, "Castle.Pickup.KeycardJoinsTheRing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastlePickupKeycardJoinsTheRing::RunTest(const FString& Parameters)
{
	FCastleTestWorld TestWorld;
	ACastleCharacter* Player = CastlePickupTest::SpawnPlayer(TestWorld);
	APickupActor* Pickup = CastlePickupTest::SpawnPickup(TestWorld, EPickupType::Keycard);
	if (!Player || !Pickup)
	{
		AddError(TEXT("Failed to spawn the player or the pickup."));
		return false;
	}

	const FName Cellblock(TEXT("cellblock"));
	Pickup->KeycardId = Cellblock;

	TestFalse(TEXT("No keycard to start with"), Player->HasKeycard(Cellblock));
	TestTrue(TEXT("The pickup applies"), Pickup->ApplyTo(Player));
	TestTrue(TEXT("The keycard is on the ring"), Player->HasKeycard(Cellblock));
	TestFalse(TEXT("Other ids are unaffected"), Player->HasKeycard(FName(TEXT("infirmary"))));
	TestFalse(TEXT("A second grant of the same id changes nothing"), Player->GiveKeycard(Cellblock));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastlePickupArrowsFillTheQuiver, "Castle.Pickup.ArrowsFillTheQuiver",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastlePickupArrowsFillTheQuiver::RunTest(const FString& Parameters)
{
	FCastleTestWorld TestWorld;
	ACastleCharacter* Player = CastlePickupTest::SpawnPlayer(TestWorld);
	APickupActor* Pickup = CastlePickupTest::SpawnPickup(TestWorld, EPickupType::Arrows);
	UInventoryComponent* Inventory = Player ? Player->GetInventoryComponent() : nullptr;
	if (!Inventory || !Pickup)
	{
		AddError(TEXT("Failed to spawn the player or the pickup."));
		return false;
	}

	UArrowDefinition* Grapple = NewObject<UArrowDefinition>(Pickup);
	Grapple->Slot = 2;
	Grapple->Cap = 6;
	Grapple->OnHitEffect = EArrowHitEffect::Grapple;
	Inventory->AddArrows(Grapple, 4);

	Pickup->Arrow = Grapple;
	Pickup->ArrowCount = 6;
	TestTrue(TEXT("The pickup applies"), Pickup->ApplyTo(Player));
	TestEqual(TEXT("Topped up to the cap, not past it"), Inventory->GetArrowCount(2), 6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastlePickupIgnoresNonPlayers, "Castle.Pickup.IgnoresNonPlayers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastlePickupIgnoresNonPlayers::RunTest(const FString& Parameters)
{
	FCastleTestWorld TestWorld;
	APickupActor* Pickup = CastlePickupTest::SpawnPickup(TestWorld, EPickupType::Keycard);
	AActor* Bystander = TestWorld.SpawnActor(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
	if (!Pickup || !Bystander)
	{
		AddError(TEXT("Failed to spawn the test actors."));
		return false;
	}

	TestFalse(TEXT("A plain actor cannot interact"), IInteractable::Execute_CanInteract(Pickup, Bystander));
	TestFalse(TEXT("And applying to one does nothing"), Pickup->ApplyTo(Bystander));
	TestTrue(TEXT("The pickup is still there"), IsValid(Pickup) && !Pickup->IsActorBeingDestroyed());

	return true;
}

#endif

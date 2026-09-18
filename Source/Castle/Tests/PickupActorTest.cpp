// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/WeaponComponent.h"
#include "Misc/AutomationTest.h"
#include "Player/CastleCharacter.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastlePickupWeaponArmsThePlayer, "Castle.Pickup.WeaponArmsThePlayer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastlePickupWeaponArmsThePlayer::RunTest(const FString& Parameters)
{
	FCastleTestWorld TestWorld;
	ACastleCharacter* Player = CastlePickupTest::SpawnPlayer(TestWorld);
	APickupActor* Pickup = CastlePickupTest::SpawnPickup(TestWorld, EPickupType::Weapon);
	if (!Player || !Pickup)
	{
		AddError(TEXT("Failed to spawn the player or the pickup."));
		return false;
	}

	UWeaponComponent* Weapon = Player->GetWeaponComponent();
	if (!Weapon)
	{
		AddError(TEXT("ACastleCharacter has no UWeaponComponent."));
		return false;
	}

	TestFalse(TEXT("Frank starts unarmed"), Weapon->HasWeapon());
	TestFalse(TEXT("An unarmed Fire is a no-op"), Weapon->Fire());
	TestFalse(TEXT("An unarmed Reload is a no-op"), Weapon->Reload());

	Pickup->MagazineAmount = 12;
	Pickup->AmmoAmount = 24;
	TestTrue(TEXT("The pickup applies"), Pickup->ApplyTo(Player));

	TestTrue(TEXT("Frank is armed"), Weapon->HasWeapon());
	TestEqual(TEXT("Magazine loaded"), Weapon->CurrentAmmo, 12);
	TestEqual(TEXT("Reserve loaded"), Weapon->ReserveAmmo, 24);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastlePickupAmmoAddsToReserve, "Castle.Pickup.AmmoAddsToReserve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastlePickupAmmoAddsToReserve::RunTest(const FString& Parameters)
{
	FCastleTestWorld TestWorld;
	ACastleCharacter* Player = CastlePickupTest::SpawnPlayer(TestWorld);
	APickupActor* Pickup = CastlePickupTest::SpawnPickup(TestWorld, EPickupType::Ammo);
	if (!Player || !Pickup)
	{
		AddError(TEXT("Failed to spawn the player or the pickup."));
		return false;
	}

	UWeaponComponent* Weapon = Player->GetWeaponComponent();
	const int32 Before = Weapon->ReserveAmmo;

	Pickup->AmmoAmount = 12;
	TestTrue(TEXT("The pickup applies"), Pickup->ApplyTo(Player));
	TestEqual(TEXT("Reserve grew by the pickup amount"), Weapon->ReserveAmmo, Before + 12);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastlePickupIgnoresNonPlayers, "Castle.Pickup.IgnoresNonPlayers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastlePickupIgnoresNonPlayers::RunTest(const FString& Parameters)
{
	FCastleTestWorld TestWorld;
	APickupActor* Pickup = CastlePickupTest::SpawnPickup(TestWorld, EPickupType::Weapon);
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

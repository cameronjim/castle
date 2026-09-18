// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/HealthComponent.h"
#include "Combat/WeaponComponent.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Player/CastleCharacter.h"
#include "Tests/CastleTestUtils.h"
#include "World/GuardCharacter.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * The bullets-do-not-kill-guards regression.
 *
 * Every shot used to trace on ECC_Visibility, which the stock Pawn and CharacterMesh collision
 * profiles both ignore, so the trace passed straight through the guard and hit the wall behind
 * him. A whole playthrough produced no shooting death. These tests fire a real weapon at a real
 * guard in a real world, which is the only place that bug was visible.
 */
namespace CastleWeaponDamageTest
{
	/** Frank at the origin looking down +X, armed, with the fire-rate clock under test control. */
	static ACastleCharacter* SpawnArmedPlayer(const FCastleTestWorld& TestWorld, double& Now)
	{
		ACastleCharacter* Player = Cast<ACastleCharacter>(TestWorld.SpawnActor(
			ACastleCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator));
		if (!Player)
		{
			return nullptr;
		}

		UWeaponComponent* Weapon = Player->GetWeaponComponent();
		if (Weapon)
		{
			Weapon->GiveWeapon(12, 24);
			// No spread: the test asserts that a shot aimed at a guard hits him, not that the
			// cone maths is fair.
			Weapon->HipSpreadDegrees = 0.f;
			Weapon->AimSpreadDegrees = 0.f;
			Weapon->SetTestTimeSeconds(Now);
		}

		return Player;
	}

	static void AdvanceClock(UWeaponComponent* Weapon, double& Now, double Seconds)
	{
		Now += Seconds;
		Weapon->SetTestTimeSeconds(Now);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleWeaponShotDamagesGuard, "Castle.Weapon.ShotDamagesGuard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleWeaponShotDamagesGuard::RunTest(const FString& Parameters)
{
	double Now = 0.0;
	FCastleTestWorld TestWorld;

	ACastleCharacter* Player = CastleWeaponDamageTest::SpawnArmedPlayer(TestWorld, Now);
	if (!TestNotNull(TEXT("Player spawned"), Player))
	{
		return false;
	}

	AGuardCharacter* Guard = Cast<AGuardCharacter>(TestWorld.SpawnActor(
		AGuardCharacter::StaticClass(), FVector(300.f, 0.f, 0.f), FRotator::ZeroRotator));
	if (!TestNotNull(TEXT("Guard spawned 3 metres in front"), Guard))
	{
		return false;
	}

	UWeaponComponent* Weapon = Player->GetWeaponComponent();
	UHealthComponent* GuardHealth = Guard->GetHealthComponent();
	if (!TestNotNull(TEXT("Guard has health"), GuardHealth))
	{
		return false;
	}

	const float StartingHealth = GuardHealth->GetCurrentHealth();
	TestTrue(TEXT("The shot goes out"), Weapon->Fire());

	TestEqual(TEXT("The guard lost exactly one body shot of health"),
		GuardHealth->GetCurrentHealth(), StartingHealth - Weapon->Damage);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleWeaponThreeShotsKillGuard, "Castle.Weapon.ThreeShotsKillGuard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleWeaponThreeShotsKillGuard::RunTest(const FString& Parameters)
{
	double Now = 0.0;
	FCastleTestWorld TestWorld;

	ACastleCharacter* Player = CastleWeaponDamageTest::SpawnArmedPlayer(TestWorld, Now);
	AGuardCharacter* Guard = Cast<AGuardCharacter>(TestWorld.SpawnActor(
		AGuardCharacter::StaticClass(), FVector(300.f, 0.f, 0.f), FRotator::ZeroRotator));
	if (!TestNotNull(TEXT("Player spawned"), Player) || !TestNotNull(TEXT("Guard spawned"), Guard))
	{
		return false;
	}

	UWeaponComponent* Weapon = Player->GetWeaponComponent();
	UHealthComponent* GuardHealth = Guard->GetHealthComponent();

	UCastleTestListener* Listener = NewObject<UCastleTestListener>();
	GuardHealth->OnDeath.AddDynamic(Listener, &UCastleTestListener::HandleDeath);

	int32 ShotsFired = 0;
	for (int32 Shot = 0; Shot < 3; ++Shot)
	{
		if (Weapon->Fire())
		{
			++ShotsFired;
		}
		CastleWeaponDamageTest::AdvanceClock(Weapon, Now, 1.0);
	}

	TestEqual(TEXT("Three shots went out"), ShotsFired, 3);
	TestEqual(TEXT("A 100 HP guard is dead after three body shots"),
		GuardHealth->GetCurrentHealth(), 0.f);
	TestTrue(TEXT("And he knows it"), GuardHealth->IsDead());
	TestEqual(TEXT("OnDeath fired exactly once"), Listener->DeathCount, 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleWeaponHitDelegateFires, "Castle.Weapon.HitDelegateFires",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleWeaponHitDelegateFires::RunTest(const FString& Parameters)
{
	double Now = 0.0;
	FCastleTestWorld TestWorld;

	ACastleCharacter* Player = CastleWeaponDamageTest::SpawnArmedPlayer(TestWorld, Now);
	AGuardCharacter* Guard = Cast<AGuardCharacter>(TestWorld.SpawnActor(
		AGuardCharacter::StaticClass(), FVector(300.f, 0.f, 0.f), FRotator::ZeroRotator));
	if (!TestNotNull(TEXT("Player spawned"), Player) || !TestNotNull(TEXT("Guard spawned"), Guard))
	{
		return false;
	}

	UWeaponComponent* Weapon = Player->GetWeaponComponent();
	UCastleTestListener* Listener = NewObject<UCastleTestListener>();
	Weapon->OnHit.AddDynamic(Listener, &UCastleTestListener::HandleWeaponHit);

	Weapon->Fire();

	// The HUD's hit marker hangs off this, so a hit on a damageable actor has to report itself.
	TestEqual(TEXT("OnHit fired once"), Listener->WeaponHitCount, 1);
	TestEqual(TEXT("With the guard as the hit actor"),
		static_cast<AActor*>(Listener->LastWeaponHitActor.Get()), static_cast<AActor*>(Guard));
	TestEqual(TEXT("And the damage that was dealt"), Listener->LastWeaponHitDamage, Weapon->Damage);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleWeaponMissDamagesNothing, "Castle.Weapon.MissDamagesNothing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleWeaponMissDamagesNothing::RunTest(const FString& Parameters)
{
	double Now = 0.0;
	FCastleTestWorld TestWorld;

	ACastleCharacter* Player = CastleWeaponDamageTest::SpawnArmedPlayer(TestWorld, Now);
	// Behind the player: the trace runs down +X and this guard is at -X.
	AGuardCharacter* Guard = Cast<AGuardCharacter>(TestWorld.SpawnActor(
		AGuardCharacter::StaticClass(), FVector(-300.f, 0.f, 0.f), FRotator::ZeroRotator));
	if (!TestNotNull(TEXT("Player spawned"), Player) || !TestNotNull(TEXT("Guard spawned"), Guard))
	{
		return false;
	}

	UWeaponComponent* Weapon = Player->GetWeaponComponent();
	UCastleTestListener* Listener = NewObject<UCastleTestListener>();
	Weapon->OnHit.AddDynamic(Listener, &UCastleTestListener::HandleWeaponHit);

	Weapon->Fire();

	TestEqual(TEXT("A guard behind you takes nothing"),
		Guard->GetHealthComponent()->GetCurrentHealth(), 100.f);
	TestEqual(TEXT("And no hit marker is asked for"), Listener->WeaponHitCount, 0);

	return true;
}

#endif

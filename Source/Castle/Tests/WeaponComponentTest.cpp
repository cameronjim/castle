// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/WeaponComponent.h"
#include "Misc/AutomationTest.h"
#include "Tests/CastleTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CastleWeaponTest
{
	/** A pistol with the default stats and a test clock, so the fire rate is deterministic. */
	static UWeaponComponent* MakeWeapon()
	{
		UWeaponComponent* Weapon = NewObject<UWeaponComponent>();
		Weapon->SetTestTimeSeconds(0.0);
		return Weapon;
	}

	/** Moves the test clock far enough forward that the next shot is always allowed. */
	static void AdvanceClock(UWeaponComponent* Weapon, double& Now, double Seconds)
	{
		Now += Seconds;
		Weapon->SetTestTimeSeconds(Now);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleWeaponDefaultStats, "Castle.Weapon.DefaultStats",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleWeaponDefaultStats::RunTest(const FString& Parameters)
{
	UWeaponComponent* Weapon = CastleWeaponTest::MakeWeapon();

	TestEqual(TEXT("Magazine size"), Weapon->MagazineSize, 12);
	TestEqual(TEXT("Starting magazine"), Weapon->CurrentAmmo, 12);
	TestEqual(TEXT("Reserve"), Weapon->ReserveAmmo, 24);
	TestEqual(TEXT("Body damage"), Weapon->Damage, 34.f);

	// Three body shots or one headshot kill a 100 HP guard.
	TestTrue(TEXT("Three body shots kill a 100 HP guard"), Weapon->ComputeDamageForHit(FName(TEXT("spine_01"))) * 3.f >= 100.f);
	TestTrue(TEXT("Two body shots do not"), Weapon->ComputeDamageForHit(FName(TEXT("spine_01"))) * 2.f < 100.f);
	TestTrue(TEXT("One headshot kills"), Weapon->ComputeDamageForHit(FName(TEXT("head"))) >= 100.f);
	TestEqual(TEXT("neck_01 counts as a head"), Weapon->ComputeDamageForHit(FName(TEXT("neck_01"))), 34.f * 3.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleWeaponFireDecrementsMagazine, "Castle.Weapon.FireDecrementsMagazine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleWeaponFireDecrementsMagazine::RunTest(const FString& Parameters)
{
	double Now = 0.0;
	UWeaponComponent* Weapon = CastleWeaponTest::MakeWeapon();

	UCastleTestListener* Listener = NewObject<UCastleTestListener>();
	Weapon->OnAmmoChanged.AddDynamic(Listener, &UCastleTestListener::HandleAmmoChanged);

	TestTrue(TEXT("First shot goes out"), Weapon->Fire());
	TestEqual(TEXT("Magazine decremented"), Weapon->CurrentAmmo, 11);
	TestEqual(TEXT("OnAmmoChanged fired once"), Listener->AmmoChangedCount, 1);
	TestEqual(TEXT("Magazine reported"), Listener->LastMagazine, 11);
	TestEqual(TEXT("Reserve untouched"), Listener->LastReserve, 24);

	CastleWeaponTest::AdvanceClock(Weapon, Now, 1.0);
	TestTrue(TEXT("Second shot goes out"), Weapon->Fire());
	TestEqual(TEXT("Magazine decremented again"), Weapon->CurrentAmmo, 10);
	TestEqual(TEXT("OnAmmoChanged counted twice"), Listener->AmmoChangedCount, 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleWeaponFireRateBlocksRapidFire, "Castle.Weapon.FireRateBlocksRapidFire",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleWeaponFireRateBlocksRapidFire::RunTest(const FString& Parameters)
{
	double Now = 0.0;
	UWeaponComponent* Weapon = CastleWeaponTest::MakeWeapon();
	Weapon->FireRate = 600.f; // 0.1 s between shots.

	TestTrue(TEXT("First shot goes out"), Weapon->Fire());
	TestFalse(TEXT("Same instant is blocked"), Weapon->Fire());
	TestEqual(TEXT("No ammo spent on the blocked shot"), Weapon->CurrentAmmo, 11);

	CastleWeaponTest::AdvanceClock(Weapon, Now, 0.05);
	TestFalse(TEXT("Half the interval is still blocked"), Weapon->Fire());

	CastleWeaponTest::AdvanceClock(Weapon, Now, 0.05);
	TestTrue(TEXT("A full interval allows the next shot"), Weapon->Fire());
	TestEqual(TEXT("Two rounds spent"), Weapon->CurrentAmmo, 10);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleWeaponFireAtZeroAmmoClicks, "Castle.Weapon.FireAtZeroAmmoClicks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleWeaponFireAtZeroAmmoClicks::RunTest(const FString& Parameters)
{
	double Now = 0.0;
	UWeaponComponent* Weapon = CastleWeaponTest::MakeWeapon();
	Weapon->CurrentAmmo = 0;

	UCastleTestListener* Listener = NewObject<UCastleTestListener>();
	Weapon->OnAmmoChanged.AddDynamic(Listener, &UCastleTestListener::HandleAmmoChanged);
	Weapon->OnEmptyClick.AddDynamic(Listener, &UCastleTestListener::HandleEmptyClick);

	CastleWeaponTest::AdvanceClock(Weapon, Now, 10.0);
	TestFalse(TEXT("Fire refused"), Weapon->Fire());
	TestEqual(TEXT("Magazine stays at zero"), Weapon->CurrentAmmo, 0);
	TestEqual(TEXT("No ammo change broadcast"), Listener->AmmoChangedCount, 0);
	TestEqual(TEXT("OnEmptyClick fired"), Listener->EmptyClickCount, 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleWeaponReloadMath, "Castle.Weapon.ReloadMath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleWeaponReloadMath::RunTest(const FString& Parameters)
{
	UWeaponComponent* Weapon = CastleWeaponTest::MakeWeapon();
	Weapon->CurrentAmmo = 4;
	Weapon->ReserveAmmo = 24;

	UCastleTestListener* Listener = NewObject<UCastleTestListener>();
	Weapon->OnAmmoChanged.AddDynamic(Listener, &UCastleTestListener::HandleAmmoChanged);

	TestTrue(TEXT("Reload starts"), Weapon->Reload());
	TestTrue(TEXT("Weapon is reloading"), Weapon->IsReloading());
	TestFalse(TEXT("Firing during a reload is blocked"), Weapon->Fire());

	Weapon->CompleteReloadNow();

	TestFalse(TEXT("Reload finished"), Weapon->IsReloading());
	TestEqual(TEXT("Magazine full"), Weapon->CurrentAmmo, 12);
	TestEqual(TEXT("Reserve reduced by the 8 rounds moved"), Weapon->ReserveAmmo, 16);
	TestEqual(TEXT("One OnAmmoChanged for the reload"), Listener->AmmoChangedCount, 1);

	// Partial reserve: only what is left moves.
	Weapon->CurrentAmmo = 0;
	Weapon->ReserveAmmo = 5;
	Weapon->Reload();
	Weapon->CompleteReloadNow();

	TestEqual(TEXT("Only the remaining reserve is loaded"), Weapon->CurrentAmmo, 5);
	TestEqual(TEXT("Reserve empty"), Weapon->ReserveAmmo, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleWeaponReloadRefused, "Castle.Weapon.ReloadRefused",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleWeaponReloadRefused::RunTest(const FString& Parameters)
{
	UWeaponComponent* Weapon = CastleWeaponTest::MakeWeapon();

	UCastleTestListener* Listener = NewObject<UCastleTestListener>();
	Weapon->OnAmmoChanged.AddDynamic(Listener, &UCastleTestListener::HandleAmmoChanged);

	TestFalse(TEXT("Reload with a full magazine is a no-op"), Weapon->Reload());

	Weapon->CurrentAmmo = 2;
	Weapon->ReserveAmmo = 0;
	TestFalse(TEXT("Reload with an empty reserve is a no-op"), Weapon->Reload());

	Weapon->ReserveAmmo = 10;
	TestTrue(TEXT("Reload starts"), Weapon->Reload());
	TestFalse(TEXT("Reload while already reloading is a no-op"), Weapon->Reload());

	TestEqual(TEXT("No ammo moved by any refused reload"), Listener->AmmoChangedCount, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleWeaponCancelReloadKeepsAmmo, "Castle.Weapon.CancelReloadKeepsAmmo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleWeaponCancelReloadKeepsAmmo::RunTest(const FString& Parameters)
{
	double Now = 0.0;
	UWeaponComponent* Weapon = CastleWeaponTest::MakeWeapon();
	Weapon->CurrentAmmo = 3;

	Weapon->Reload();
	Weapon->CancelReload();

	TestFalse(TEXT("No longer reloading"), Weapon->IsReloading());
	TestEqual(TEXT("Magazine unchanged"), Weapon->CurrentAmmo, 3);
	TestEqual(TEXT("Reserve unchanged"), Weapon->ReserveAmmo, 24);

	CastleWeaponTest::AdvanceClock(Weapon, Now, 1.0);
	TestTrue(TEXT("Firing works again after cancelling"), Weapon->Fire());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/HealthComponent.h"
#include "Misc/AutomationTest.h"
#include "Tests/CastleTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CastleHealthTest
{
	/** A health component with MaxHealth hit points, no owner and no world. */
	static UHealthComponent* MakeHealth(float MaxHealth = 100.f)
	{
		UHealthComponent* Health = NewObject<UHealthComponent>();
		Health->SetMaxHealth(MaxHealth, /*bResetCurrent=*/true);
		return Health;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleHealthClamped, "Castle.Health.Clamped",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleHealthClamped::RunTest(const FString& Parameters)
{
	UHealthComponent* Health = CastleHealthTest::MakeHealth();

	Health->ApplyDamage(150.f);
	TestEqual(TEXT("Damage past zero clamps to zero"), Health->GetCurrentHealth(), 0.f);

	Health->Revive(500.f);
	TestEqual(TEXT("Revive clamps to MaxHealth"), Health->GetCurrentHealth(), 100.f);

	Health->Heal(50.f);
	TestEqual(TEXT("Healing past MaxHealth clamps"), Health->GetCurrentHealth(), 100.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleHealthNegativeDamageIgnored, "Castle.Health.NegativeDamageIgnored",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleHealthNegativeDamageIgnored::RunTest(const FString& Parameters)
{
	UHealthComponent* Health = CastleHealthTest::MakeHealth();
	UCastleTestListener* Listener = NewObject<UCastleTestListener>();
	Health->OnHealthChanged.AddDynamic(Listener, &UCastleTestListener::HandleHealthChanged);

	TestEqual(TEXT("Zero damage returns nothing"), Health->ApplyDamage(0.f), 0.f);
	TestEqual(TEXT("Negative damage returns nothing"), Health->ApplyDamage(-25.f), 0.f);
	TestEqual(TEXT("Health untouched"), Health->GetCurrentHealth(), 100.f);
	TestEqual(TEXT("No OnHealthChanged fired"), Listener->HealthChangedCount, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleHealthInvulnerableIgnoresDamage, "Castle.Health.InvulnerableIgnoresDamage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleHealthInvulnerableIgnoresDamage::RunTest(const FString& Parameters)
{
	UHealthComponent* Health = CastleHealthTest::MakeHealth();
	UCastleTestListener* Listener = NewObject<UCastleTestListener>();
	Health->OnHealthChanged.AddDynamic(Listener, &UCastleTestListener::HandleHealthChanged);
	Health->OnDeath.AddDynamic(Listener, &UCastleTestListener::HandleDeath);

	Health->SetInvulnerable(true);
	TestTrue(TEXT("IsInvulnerable reports the flag"), Health->IsInvulnerable());

	Health->ApplyDamage(9999.f);

	TestEqual(TEXT("Health untouched"), Health->GetCurrentHealth(), 100.f);
	TestEqual(TEXT("No OnHealthChanged"), Listener->HealthChangedCount, 0);
	TestEqual(TEXT("No OnDeath"), Listener->DeathCount, 0);
	TestTrue(TEXT("Still alive"), Health->IsAlive());

	Health->SetInvulnerable(false);
	Health->ApplyDamage(10.f);
	TestEqual(TEXT("Damage lands once invulnerability is off"), Health->GetCurrentHealth(), 90.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleHealthDeathFiresOnce, "Castle.Health.DeathFiresOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleHealthDeathFiresOnce::RunTest(const FString& Parameters)
{
	UHealthComponent* Health = CastleHealthTest::MakeHealth();
	UCastleTestListener* Listener = NewObject<UCastleTestListener>();
	Health->OnDeath.AddDynamic(Listener, &UCastleTestListener::HandleDeath);

	Health->ApplyDamage(150.f);
	Health->ApplyDamage(10.f);

	TestEqual(TEXT("Health clamped to zero"), Health->GetCurrentHealth(), 0.f);
	TestEqual(TEXT("OnDeath fired exactly once"), Listener->DeathCount, 1);
	TestFalse(TEXT("IsAlive is false"), Health->IsAlive());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleHealthChangedFiresOnHeal, "Castle.Health.ChangedFiresOnHeal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleHealthChangedFiresOnHeal::RunTest(const FString& Parameters)
{
	UHealthComponent* Health = CastleHealthTest::MakeHealth();
	UCastleTestListener* Listener = NewObject<UCastleTestListener>();
	Health->OnHealthChanged.AddDynamic(Listener, &UCastleTestListener::HandleHealthChanged);

	Health->ApplyDamage(40.f);
	TestEqual(TEXT("Damage reported"), Listener->LastHealthDelta, -40.f);

	Health->Heal(15.f);
	TestEqual(TEXT("Two changes so far"), Listener->HealthChangedCount, 2);
	TestEqual(TEXT("Heal reported as a positive delta"), Listener->LastHealthDelta, 15.f);
	TestEqual(TEXT("New health reported"), Listener->LastNewHealth, 75.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleHealthHealAfterDeathIsNoOp, "Castle.Health.HealAfterDeathIsNoOp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleHealthHealAfterDeathIsNoOp::RunTest(const FString& Parameters)
{
	UHealthComponent* Health = CastleHealthTest::MakeHealth();
	Health->ApplyDamage(100.f);

	UCastleTestListener* Listener = NewObject<UCastleTestListener>();
	Health->OnHealthChanged.AddDynamic(Listener, &UCastleTestListener::HandleHealthChanged);

	TestEqual(TEXT("Heal returns nothing"), Health->Heal(50.f), 0.f);
	TestEqual(TEXT("Health still zero"), Health->GetCurrentHealth(), 0.f);
	TestEqual(TEXT("No OnHealthChanged"), Listener->HealthChangedCount, 0);
	TestTrue(TEXT("Still dead"), Health->IsDead());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleHealthReviveResetsDeath, "Castle.Health.ReviveResetsDeath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleHealthReviveResetsDeath::RunTest(const FString& Parameters)
{
	UHealthComponent* Health = CastleHealthTest::MakeHealth();
	UCastleTestListener* Listener = NewObject<UCastleTestListener>();
	Health->OnDeath.AddDynamic(Listener, &UCastleTestListener::HandleDeath);

	Health->ApplyDamage(100.f);
	Health->Revive(40.f);

	TestTrue(TEXT("Alive again"), Health->IsAlive());
	TestEqual(TEXT("Revived at the requested health"), Health->GetCurrentHealth(), 40.f);
	TestEqual(TEXT("Health percent follows"), Health->GetHealthPercent(), 0.4f);

	Health->Heal(10.f);
	TestEqual(TEXT("Heal works again after revive"), Health->GetCurrentHealth(), 50.f);

	Health->ApplyDamage(100.f);
	TestEqual(TEXT("OnDeath fires again for the second death"), Listener->DeathCount, 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleHealthSetMaxHealth, "Castle.Health.SetMaxHealth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleHealthSetMaxHealth::RunTest(const FString& Parameters)
{
	UHealthComponent* Health = CastleHealthTest::MakeHealth();
	Health->ApplyDamage(50.f);

	Health->SetMaxHealth(200.f, /*bResetCurrent=*/false);
	TestEqual(TEXT("Current health kept"), Health->GetCurrentHealth(), 50.f);
	TestEqual(TEXT("Max health raised"), Health->GetMaxHealth(), 200.f);

	Health->SetMaxHealth(40.f, /*bResetCurrent=*/false);
	TestEqual(TEXT("Current health clamped down to the new max"), Health->GetCurrentHealth(), 40.f);

	Health->SetMaxHealth(120.f, /*bResetCurrent=*/true);
	TestEqual(TEXT("Reset fills the bar"), Health->GetCurrentHealth(), 120.f);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

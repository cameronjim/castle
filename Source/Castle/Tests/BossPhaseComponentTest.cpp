// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/BossPhaseComponent.h"
#include "Combat/HealthComponent.h"
#include "Misc/AutomationTest.h"
#include "Tests/CastleTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CastleBossPhaseTest
{
	static FBossPhase MakePhase(const TCHAR* Name, float Threshold, bool bInvulnerable = true)
	{
		FBossPhase Phase;
		Phase.PhaseName = FName(Name);
		Phase.BehaviorTag = FName(Name);
		Phase.HealthThresholdPercent = Threshold;
		Phase.bInvulnerableDuringTransition = bInvulnerable;
		Phase.TransitionSeconds = 2.f;
		return Phase;
	}

	/** Three-phase boss on 100 HP: thresholds 1.0 / 0.75 / 0.5, bound with no world. */
	static UBossPhaseComponent* MakeBoss(UHealthComponent*& OutHealth, bool bInvulnerableTransitions = true)
	{
		OutHealth = NewObject<UHealthComponent>();
		OutHealth->SetMaxHealth(100.f, /*bResetCurrent=*/true);

		UBossPhaseComponent* Boss = NewObject<UBossPhaseComponent>();
		Boss->Phases.Add(MakePhase(TEXT("Gun"), 1.f, bInvulnerableTransitions));
		Boss->Phases.Add(MakePhase(TEXT("Melee"), 0.75f, bInvulnerableTransitions));
		Boss->Phases.Add(MakePhase(TEXT("Desperate"), 0.5f, bInvulnerableTransitions));
		Boss->Bind(OutHealth);
		return Boss;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleBossPhaseSortedOnBind, "Castle.BossPhase.SortedOnBind",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleBossPhaseSortedOnBind::RunTest(const FString& Parameters)
{
	UHealthComponent* Health = NewObject<UHealthComponent>();
	Health->SetMaxHealth(100.f, /*bResetCurrent=*/true);

	UBossPhaseComponent* Boss = NewObject<UBossPhaseComponent>();
	Boss->Phases.Add(CastleBossPhaseTest::MakePhase(TEXT("Desperate"), 0.5f));
	Boss->Phases.Add(CastleBossPhaseTest::MakePhase(TEXT("Gun"), 1.f));
	Boss->Phases.Add(CastleBossPhaseTest::MakePhase(TEXT("Melee"), 0.75f));
	Boss->Bind(Health);

	TestEqual(TEXT("Highest threshold first"), Boss->Phases[0].PhaseName, FName(TEXT("Gun")));
	TestEqual(TEXT("Then the middle phase"), Boss->Phases[1].PhaseName, FName(TEXT("Melee")));
	TestEqual(TEXT("Lowest threshold last"), Boss->Phases[2].PhaseName, FName(TEXT("Desperate")));
	TestEqual(TEXT("Phase 0 is active at start"), Boss->GetCurrentPhaseIndex(), 0);
	TestEqual(TEXT("GetCurrentPhase matches"), Boss->GetCurrentPhase().PhaseName, FName(TEXT("Gun")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleBossPhaseAdvancesInOrder, "Castle.BossPhase.AdvancesInOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleBossPhaseAdvancesInOrder::RunTest(const FString& Parameters)
{
	UHealthComponent* Health = nullptr;
	UBossPhaseComponent* Boss = CastleBossPhaseTest::MakeBoss(Health);

	UCastleTestListener* Listener = NewObject<UCastleTestListener>();
	Boss->OnPhaseChanged.AddDynamic(Listener, &UCastleTestListener::HandlePhaseChanged);

	// 100 -> 80: still above 0.75, so no transition yet.
	Health->ApplyDamage(20.f);
	TestEqual(TEXT("Still phase 0 above the threshold"), Boss->GetCurrentPhaseIndex(), 0);
	TestEqual(TEXT("Nothing broadcast"), Listener->PhaseChangedCount, 0);

	// 80 -> 75: exactly on the threshold does not transition (it must drop strictly below).
	Health->ApplyDamage(5.f);
	TestEqual(TEXT("Exactly on the threshold stays in phase 0"), Boss->GetCurrentPhaseIndex(), 0);

	Health->ApplyDamage(1.f);
	TestEqual(TEXT("Strictly below moves to phase 1"), Boss->GetCurrentPhaseIndex(), 1);
	TestEqual(TEXT("One broadcast"), Listener->PhaseChangedCount, 1);
	TestEqual(TEXT("Behaviour tag follows the phase"), Boss->GetCurrentBehaviorTag(), FName(TEXT("Melee")));

	// 74 -> 49: below 0.5.
	Health->ApplyDamage(25.f);
	TestEqual(TEXT("Below the last threshold moves to phase 2"), Boss->GetCurrentPhaseIndex(), 2);
	TestEqual(TEXT("Three phases produce two transitions"), Listener->PhaseChangedCount, 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleBossPhaseOverkillSkipsPhases, "Castle.BossPhase.OverkillSkipsPhases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleBossPhaseOverkillSkipsPhases::RunTest(const FString& Parameters)
{
	UHealthComponent* Health = nullptr;
	UBossPhaseComponent* Boss = CastleBossPhaseTest::MakeBoss(Health);

	UCastleTestListener* Listener = NewObject<UCastleTestListener>();
	Boss->OnPhaseChanged.AddDynamic(Listener, &UCastleTestListener::HandlePhaseChanged);

	// One hit crossing both thresholds: 100 -> 40.
	Health->ApplyDamage(60.f);

	TestEqual(TEXT("Advanced to the lowest matching phase"), Boss->GetCurrentPhaseIndex(), 2);
	TestEqual(TEXT("Exactly one OnPhaseChanged"), Listener->PhaseChangedCount, 1);
	TestEqual(TEXT("Old index was the starting phase"), Listener->LastOldPhaseIndex, 0);
	TestEqual(TEXT("New index is the final phase"), Listener->LastNewPhaseIndex, 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleBossPhaseTransitionInvulnerabilityRestored, "Castle.BossPhase.TransitionInvulnerabilityRestored",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleBossPhaseTransitionInvulnerabilityRestored::RunTest(const FString& Parameters)
{
	UHealthComponent* Health = nullptr;
	UBossPhaseComponent* Boss = CastleBossPhaseTest::MakeBoss(Health);

	UCastleTestListener* Listener = NewObject<UCastleTestListener>();
	Listener->WatchedHealth = Health;
	Boss->OnPhaseChanged.AddDynamic(Listener, &UCastleTestListener::HandlePhaseChanged);
	Boss->OnPhaseTransitionFinished.AddDynamic(Listener, &UCastleTestListener::HandleTransitionFinished);

	// With no world the transition window collapses to a single call, so the flag is raised and
	// restored inside ApplyDamage; the listener observes it mid-transition.
	Health->ApplyDamage(40.f);

	TestEqual(TEXT("Transition happened"), Listener->PhaseChangedCount, 1);
	TestTrue(TEXT("Invulnerable during the transition"), Listener->bWatchedHealthInvulnerableAtPhaseChange);
	TestEqual(TEXT("Transition finished"), Listener->TransitionFinishedCount, 1);
	TestFalse(TEXT("Restored to not invulnerable"), Health->IsInvulnerable());
	TestFalse(TEXT("No longer transitioning"), Boss->IsTransitioning());

	// A boss already invulnerable for scripted reasons stays that way afterwards.
	UHealthComponent* ScriptedHealth = nullptr;
	UBossPhaseComponent* ScriptedBoss = CastleBossPhaseTest::MakeBoss(ScriptedHealth);
	ScriptedHealth->SetInvulnerable(true);
	// Invulnerable health ignores damage, so drive the phase change directly.
	ScriptedBoss->EnterPhase(1);

	TestTrue(TEXT("Scripted invulnerability survives the transition"), ScriptedHealth->IsInvulnerable());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleBossPhaseNothingOnDeath, "Castle.BossPhase.NothingOnDeath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleBossPhaseNothingOnDeath::RunTest(const FString& Parameters)
{
	UHealthComponent* Health = nullptr;
	UBossPhaseComponent* Boss = CastleBossPhaseTest::MakeBoss(Health);

	UCastleTestListener* Listener = NewObject<UCastleTestListener>();
	Boss->OnPhaseChanged.AddDynamic(Listener, &UCastleTestListener::HandlePhaseChanged);

	Health->ApplyDamage(500.f);

	TestTrue(TEXT("Boss is dead"), Health->IsDead());
	TestEqual(TEXT("Death does not advance phases"), Listener->PhaseChangedCount, 0);
	TestEqual(TEXT("Still in the starting phase"), Boss->GetCurrentPhaseIndex(), 0);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

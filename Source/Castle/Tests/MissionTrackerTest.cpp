// Copyright Epic Games, Inc. All Rights Reserved.

#include "Flashback/FlashbackDefinition.h"
#include "Misc/AutomationTest.h"
#include "Mission/MissionDefinition.h"
#include "Mission/MissionObjective.h"
#include "Mission/MissionTracker.h"
#include "Tests/CastleTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CastleMissionTest
{
	static UMissionObjective* MakeObjective(UObject* Outer, const TCHAR* Id, bool bOptional = false)
	{
		UMissionObjective* Objective = NewObject<UMissionObjective>(Outer);
		Objective->ObjectiveId = FName(Id);
		Objective->bOptional = bOptional;
		return Objective;
	}

	/** find_weapon, reach_stairwell (both required) and free_the_cellmate (optional). */
	static UMissionDefinition* MakeMission()
	{
		UMissionDefinition* Mission = NewObject<UMissionDefinition>();
		Mission->Objectives.Add(MakeObjective(Mission, TEXT("find_weapon")));
		Mission->Objectives.Add(MakeObjective(Mission, TEXT("reach_stairwell")));
		Mission->Objectives.Add(MakeObjective(Mission, TEXT("free_the_cellmate"), /*bOptional=*/true));
		return Mission;
	}

	static UMissionTracker* MakeTracker(UCastleTestListener*& OutListener)
	{
		UMissionTracker* Tracker = NewObject<UMissionTracker>();
		OutListener = NewObject<UCastleTestListener>();
		OutListener->WatchedTracker = Tracker;
		Tracker->OnMissionStarted.AddDynamic(OutListener, &UCastleTestListener::HandleMissionStarted);
		Tracker->OnObjectiveUpdated.AddDynamic(OutListener, &UCastleTestListener::HandleObjectiveUpdated);
		Tracker->OnMissionComplete.AddDynamic(OutListener, &UCastleTestListener::HandleMissionComplete);
		Tracker->OnFlashbackRequested.AddDynamic(OutListener, &UCastleTestListener::HandleFlashbackRequested);
		return Tracker;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleMissionStartFiresOnMissionStartedOnce, "Castle.Mission.StartFiresOnMissionStartedOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleMissionStartFiresOnMissionStartedOnce::RunTest(const FString& Parameters)
{
	UCastleTestListener* Listener = nullptr;
	UMissionTracker* Tracker = CastleMissionTest::MakeTracker(Listener);

	TestEqual(TEXT("Nothing before StartMission"), Listener->MissionStartedCount, 0);

	UMissionDefinition* Mission = CastleMissionTest::MakeMission();
	TestTrue(TEXT("Mission starts"), Tracker->StartMission(Mission));

	TestEqual(TEXT("OnMissionStarted fired once"), Listener->MissionStartedCount, 1);
	TestTrue(TEXT("With the definition that started"), Listener->LastStartedMission == Mission);
	TestEqual(TEXT("And nothing else fired"), Listener->ObjectiveUpdatedCount, 0);
	TestEqual(TEXT("No completion either"), Listener->MissionCompleteCount, 0);

	// The HUD reads the current objective inside the callback, so it must already exist.
	TestTrue(TEXT("The objectives exist by then"), Listener->bCurrentObjectiveSetAtMissionStart);

	// Completing objectives never re-fires it.
	Tracker->CompleteObjective(FName(TEXT("find_weapon")));
	Tracker->CompleteObjective(FName(TEXT("reach_stairwell")));
	TestEqual(TEXT("Still once"), Listener->MissionStartedCount, 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleMissionCompletesWhenRequiredDone, "Castle.Mission.CompletesWhenRequiredDone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleMissionCompletesWhenRequiredDone::RunTest(const FString& Parameters)
{
	UCastleTestListener* Listener = nullptr;
	UMissionTracker* Tracker = CastleMissionTest::MakeTracker(Listener);

	TestTrue(TEXT("Mission starts"), Tracker->StartMission(CastleMissionTest::MakeMission()));

	TestTrue(TEXT("First objective completes"), Tracker->CompleteObjective(FName(TEXT("find_weapon"))));
	TestEqual(TEXT("One objective update"), Listener->ObjectiveUpdatedCount, 1);
	TestEqual(TEXT("Mission not complete yet"), Listener->MissionCompleteCount, 0);

	TestTrue(TEXT("Second objective completes"), Tracker->CompleteObjective(FName(TEXT("reach_stairwell"))));
	TestEqual(TEXT("Two objective updates"), Listener->ObjectiveUpdatedCount, 2);
	TestEqual(TEXT("Mission complete fired once"), Listener->MissionCompleteCount, 1);
	TestTrue(TEXT("IsMissionComplete"), Tracker->IsMissionComplete());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleMissionOptionalNeverCompletesMission, "Castle.Mission.OptionalNeverCompletesMission",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleMissionOptionalNeverCompletesMission::RunTest(const FString& Parameters)
{
	UCastleTestListener* Listener = nullptr;
	UMissionTracker* Tracker = CastleMissionTest::MakeTracker(Listener);
	Tracker->StartMission(CastleMissionTest::MakeMission());

	Tracker->CompleteObjective(FName(TEXT("free_the_cellmate")));
	TestEqual(TEXT("Optional objective reported"), Listener->ObjectiveUpdatedCount, 1);
	TestEqual(TEXT("Optional alone never completes the mission"), Listener->MissionCompleteCount, 0);

	Tracker->CompleteObjective(FName(TEXT("find_weapon")));
	Tracker->CompleteObjective(FName(TEXT("reach_stairwell")));
	TestEqual(TEXT("Mission completed once the required ones are done"), Listener->MissionCompleteCount, 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleMissionOptionalAfterCompletionStillUpdates, "Castle.Mission.OptionalAfterCompletionStillUpdates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleMissionOptionalAfterCompletionStillUpdates::RunTest(const FString& Parameters)
{
	UCastleTestListener* Listener = nullptr;
	UMissionTracker* Tracker = CastleMissionTest::MakeTracker(Listener);
	Tracker->StartMission(CastleMissionTest::MakeMission());

	Tracker->CompleteObjective(FName(TEXT("find_weapon")));
	Tracker->CompleteObjective(FName(TEXT("reach_stairwell")));
	TestEqual(TEXT("Mission complete"), Listener->MissionCompleteCount, 1);

	TestTrue(TEXT("The optional objective still completes"), Tracker->CompleteObjective(FName(TEXT("free_the_cellmate"))));
	TestEqual(TEXT("It fires OnObjectiveUpdated"), Listener->ObjectiveUpdatedCount, 3);
	TestEqual(TEXT("But never OnMissionComplete again"), Listener->MissionCompleteCount, 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleMissionDuplicateCompleteIsSilent, "Castle.Mission.DuplicateCompleteIsSilent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleMissionDuplicateCompleteIsSilent::RunTest(const FString& Parameters)
{
	UCastleTestListener* Listener = nullptr;
	UMissionTracker* Tracker = CastleMissionTest::MakeTracker(Listener);
	Tracker->StartMission(CastleMissionTest::MakeMission());

	TestTrue(TEXT("First call completes it"), Tracker->CompleteObjective(FName(TEXT("find_weapon"))));
	TestFalse(TEXT("Second call does nothing"), Tracker->CompleteObjective(FName(TEXT("find_weapon"))));
	TestEqual(TEXT("Only one update fired"), Listener->ObjectiveUpdatedCount, 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleMissionUnknownIdWarns, "Castle.Mission.UnknownIdWarns",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleMissionUnknownIdWarns::RunTest(const FString& Parameters)
{
	AddExpectedMessagePlain(TEXT("unknown objective id"), ELogVerbosity::Warning);

	UCastleTestListener* Listener = nullptr;
	UMissionTracker* Tracker = CastleMissionTest::MakeTracker(Listener);
	Tracker->StartMission(CastleMissionTest::MakeMission());

	TestFalse(TEXT("Unknown id does nothing"), Tracker->CompleteObjective(FName(TEXT("open_the_gate"))));
	TestEqual(TEXT("No delegate fired"), Listener->ObjectiveUpdatedCount, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleMissionCurrentObjective, "Castle.Mission.CurrentObjective",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleMissionCurrentObjective::RunTest(const FString& Parameters)
{
	UCastleTestListener* Listener = nullptr;
	UMissionTracker* Tracker = CastleMissionTest::MakeTracker(Listener);

	TestNull(TEXT("No mission means no current objective"), Tracker->GetCurrentObjective());

	Tracker->StartMission(CastleMissionTest::MakeMission());
	TestEqual(TEXT("First required objective is current"),
		Tracker->GetCurrentObjective()->ObjectiveId, FName(TEXT("find_weapon")));

	// Completing the optional objective must not move the current one.
	Tracker->CompleteObjective(FName(TEXT("free_the_cellmate")));
	TestEqual(TEXT("Optional completion does not move it"),
		Tracker->GetCurrentObjective()->ObjectiveId, FName(TEXT("find_weapon")));

	Tracker->CompleteObjective(FName(TEXT("find_weapon")));
	TestEqual(TEXT("Moves to the next incomplete required objective"),
		Tracker->GetCurrentObjective()->ObjectiveId, FName(TEXT("reach_stairwell")));

	Tracker->CompleteObjective(FName(TEXT("reach_stairwell")));
	TestNull(TEXT("Null once everything required is done"), Tracker->GetCurrentObjective());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleMissionFlashbackFollowsCompletion, "Castle.Mission.FlashbackFollowsCompletion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleMissionFlashbackFollowsCompletion::RunTest(const FString& Parameters)
{
	// No flashback set: nothing is requested.
	{
		UCastleTestListener* Listener = nullptr;
		UMissionTracker* Tracker = CastleMissionTest::MakeTracker(Listener);
		Tracker->StartMission(CastleMissionTest::MakeMission());
		Tracker->CompleteObjective(FName(TEXT("find_weapon")));
		Tracker->CompleteObjective(FName(TEXT("reach_stairwell")));

		TestEqual(TEXT("Mission completed"), Listener->MissionCompleteCount, 1);
		TestEqual(TEXT("No flashback requested"), Listener->FlashbackRequestedCount, 0);
	}

	// With a flashback: requested exactly once, immediately after OnMissionComplete.
	{
		UCastleTestListener* Listener = nullptr;
		UMissionTracker* Tracker = CastleMissionTest::MakeTracker(Listener);

		UMissionDefinition* Mission = CastleMissionTest::MakeMission();
		Mission->FlashbackToPlay = NewObject<UFlashbackDefinition>();

		Tracker->StartMission(Mission);
		Tracker->CompleteObjective(FName(TEXT("find_weapon")));
		Tracker->CompleteObjective(FName(TEXT("reach_stairwell")));

		TestEqual(TEXT("Flashback requested once"), Listener->FlashbackRequestedCount, 1);
		TestTrue(TEXT("It arrived after OnMissionComplete, in the same call"),
			Listener->bFlashbackFollowedMissionComplete);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleMissionRestartEndsOldSilently, "Castle.Mission.RestartEndsOldSilently",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleMissionRestartEndsOldSilently::RunTest(const FString& Parameters)
{
	AddExpectedMessagePlain(TEXT("is active; the old mission is ended silently"), ELogVerbosity::Warning);

	UCastleTestListener* Listener = nullptr;
	UMissionTracker* Tracker = CastleMissionTest::MakeTracker(Listener);

	Tracker->StartMission(CastleMissionTest::MakeMission());
	Tracker->CompleteObjective(FName(TEXT("find_weapon")));

	UMissionDefinition* SecondMission = CastleMissionTest::MakeMission();
	TestTrue(TEXT("Second mission starts"), Tracker->StartMission(SecondMission));

	TestEqual(TEXT("The abandoned mission never completed"), Listener->MissionCompleteCount, 0);
	TestEqual(TEXT("Current mission is the new one"), Tracker->GetCurrentMission(), SecondMission);
	TestEqual(TEXT("Objectives are fresh"),
		Tracker->GetCurrentObjective()->ObjectiveId, FName(TEXT("find_weapon")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleMissionRefusesAllOptional, "Castle.Mission.RefusesAllOptional",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleMissionRefusesAllOptional::RunTest(const FString& Parameters)
{
	AddExpectedErrorPlain(TEXT("has no non-optional objectives"));

	UCastleTestListener* Listener = nullptr;
	UMissionTracker* Tracker = CastleMissionTest::MakeTracker(Listener);

	UMissionDefinition* Mission = NewObject<UMissionDefinition>();
	Mission->Objectives.Add(CastleMissionTest::MakeObjective(Mission, TEXT("say_hello"), /*bOptional=*/true));

	TestFalse(TEXT("Refused"), Tracker->StartMission(Mission));
	TestNull(TEXT("No mission is active"), Tracker->GetCurrentMission());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleMissionEnforcedOrder, "Castle.Mission.EnforcedOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleMissionEnforcedOrder::RunTest(const FString& Parameters)
{
	AddExpectedMessagePlain(TEXT("enforces order"), ELogVerbosity::Warning);

	UCastleTestListener* Listener = nullptr;
	UMissionTracker* Tracker = CastleMissionTest::MakeTracker(Listener);

	UMissionDefinition* Mission = CastleMissionTest::MakeMission();
	Mission->bEnforceOrder = true;
	Tracker->StartMission(Mission);

	TestFalse(TEXT("Completing out of order is refused"), Tracker->CompleteObjective(FName(TEXT("reach_stairwell"))));
	TestEqual(TEXT("Nothing broadcast"), Listener->ObjectiveUpdatedCount, 0);

	TestTrue(TEXT("Optional objectives are never ordered"), Tracker->CompleteObjective(FName(TEXT("free_the_cellmate"))));

	TestTrue(TEXT("The current objective completes"), Tracker->CompleteObjective(FName(TEXT("find_weapon"))));
	TestTrue(TEXT("Then the next one does"), Tracker->CompleteObjective(FName(TEXT("reach_stairwell"))));
	TestEqual(TEXT("Mission complete"), Listener->MissionCompleteCount, 1);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

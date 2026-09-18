// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Mission/MissionDefinition.h"
#include "Mission/MissionObjective.h"
#include "Mission/MissionSubsystem.h"
#include "Player/CastleCharacter.h"
#include "Tests/CastleTestUtils.h"
#include "World/DoorActor.h"
#include "World/Interactable.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CastleDoorTest
{
	static ADoorActor* SpawnLockedDoor(const FCastleTestWorld& TestWorld)
	{
		ADoorActor* Door = Cast<ADoorActor>(
			TestWorld.SpawnActor(ADoorActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator));
		if (Door)
		{
			Door->bLocked = true;
			Door->RequiredKeycardId = FName(TEXT("cellblock"));
		}
		return Door;
	}

	static ACastleCharacter* SpawnPlayer(const FCastleTestWorld& TestWorld)
	{
		return Cast<ACastleCharacter>(
			TestWorld.SpawnActor(ACastleCharacter::StaticClass(), FVector(300.f, 0.f, 0.f), FRotator::ZeroRotator));
	}

	/** A one-objective mission started on the test world's subsystem, so the door can complete it. */
	static UMissionSubsystem* StartMissionWith(UWorld* World, FName ObjectiveId)
	{
		UMissionSubsystem* Missions = World ? World->GetSubsystem<UMissionSubsystem>() : nullptr;
		if (!Missions)
		{
			return nullptr;
		}

		UMissionDefinition* Mission = NewObject<UMissionDefinition>();
		UMissionObjective* Objective = NewObject<UMissionObjective>(Mission);
		Objective->ObjectiveId = ObjectiveId;
		Mission->Objectives.Add(Objective);
		Missions->StartMission(Mission);
		return Missions;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleDoorRefusesWithoutKeycard, "Castle.Door.RefusesWithoutKeycard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleDoorRefusesWithoutKeycard::RunTest(const FString& Parameters)
{
	FCastleTestWorld TestWorld;
	ADoorActor* Door = CastleDoorTest::SpawnLockedDoor(TestWorld);
	ACastleCharacter* Player = CastleDoorTest::SpawnPlayer(TestWorld);
	if (!Door || !Player)
	{
		AddError(TEXT("Failed to spawn the door or the player."));
		return false;
	}

	TestFalse(TEXT("The door is locked for an empty-handed player"), Door->IsUnlockedFor(Player));
	TestFalse(TEXT("TryOpen refuses"), Door->TryOpen(Player));
	TestFalse(TEXT("And the door stays shut"), Door->IsOpen());
	TestEqual(TEXT("The prompt says why"),
		IInteractable::Execute_GetInteractPrompt(Door).ToString(), FString(TEXT("Locked: keycard required")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleDoorOpensWithKeycard, "Castle.Door.OpensWithKeycard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleDoorOpensWithKeycard::RunTest(const FString& Parameters)
{
	FCastleTestWorld TestWorld;
	ADoorActor* Door = CastleDoorTest::SpawnLockedDoor(TestWorld);
	ACastleCharacter* Player = CastleDoorTest::SpawnPlayer(TestWorld);
	if (!Door || !Player)
	{
		AddError(TEXT("Failed to spawn the door or the player."));
		return false;
	}

	Player->GiveKeycard(FName(TEXT("cellblock")));

	TestTrue(TEXT("The door is unlocked now"), Door->IsUnlockedFor(Player));
	TestTrue(TEXT("TryOpen opens it"), Door->TryOpen(Player));
	TestTrue(TEXT("It is open"), Door->IsOpen());
	TestFalse(TEXT("Opening it again does nothing"), Door->TryOpen(Player));
	TestTrue(TEXT("An open door is no longer interactable"),
		!IInteractable::Execute_CanInteract(Door, Player));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleDoorCompletesObjectiveOnce, "Castle.Door.CompletesObjectiveOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleDoorCompletesObjectiveOnce::RunTest(const FString& Parameters)
{
	FCastleTestWorld TestWorld;
	const FName ObjectiveId(TEXT("security_door"));

	UMissionSubsystem* Missions = CastleDoorTest::StartMissionWith(TestWorld.Get(), ObjectiveId);
	if (!Missions)
	{
		AddError(TEXT("The test world has no UMissionSubsystem."));
		return false;
	}

	UCastleTestListener* Listener = NewObject<UCastleTestListener>();
	Missions->OnObjectiveUpdated.AddDynamic(Listener, &UCastleTestListener::HandleObjectiveUpdated);

	ADoorActor* Door = CastleDoorTest::SpawnLockedDoor(TestWorld);
	ACastleCharacter* Player = CastleDoorTest::SpawnPlayer(TestWorld);
	if (!Door || !Player)
	{
		AddError(TEXT("Failed to spawn the door or the player."));
		return false;
	}
	Door->CompletesObjectiveId = ObjectiveId;

	TestFalse(TEXT("A refused open completes nothing"), Door->TryOpen(Player));
	TestEqual(TEXT("No objective update yet"), Listener->ObjectiveUpdatedCount, 0);

	Player->GiveKeycard(FName(TEXT("cellblock")));
	TestTrue(TEXT("The door opens"), Door->TryOpen(Player));
	TestEqual(TEXT("The objective completed once"), Listener->ObjectiveUpdatedCount, 1);

	// A second open is refused outright, so the objective can never fire twice.
	Door->TryOpen(Player);
	TestEqual(TEXT("Still once"), Listener->ObjectiveUpdatedCount, 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleDoorUnlockedNeedsNoKeycard, "Castle.Door.UnlockedNeedsNoKeycard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleDoorUnlockedNeedsNoKeycard::RunTest(const FString& Parameters)
{
	FCastleTestWorld TestWorld;
	ADoorActor* Door = CastleDoorTest::SpawnLockedDoor(TestWorld);
	ACastleCharacter* Player = CastleDoorTest::SpawnPlayer(TestWorld);
	if (!Door || !Player)
	{
		AddError(TEXT("Failed to spawn the door or the player."));
		return false;
	}

	Door->bLocked = false;

	TestEqual(TEXT("The prompt is the plain one"),
		IInteractable::Execute_GetInteractPrompt(Door).ToString(), FString(TEXT("[E] Open")));
	TestTrue(TEXT("It opens with no keycard at all"), Door->TryOpen(Player));

	return true;
}

#endif

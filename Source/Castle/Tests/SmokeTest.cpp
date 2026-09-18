// Copyright Epic Games, Inc. All Rights Reserved.

#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include "Mission/MissionDefinition.h"
#include "Mission/MissionSubsystem.h"
#include "Player/CastleCharacter.h"
#include "Tests/AutomationCommon.h"
#include "World/DoorActor.h"
#include "World/GuardCharacter.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Boots /Game/Maps/L_M01_CellBlockD for real and asserts the level is playable: a mission is
 * running with its four objectives, the five guards and the keycard door exist, and the pawn
 * the player is driving is an ACastleCharacter.
 *
 * This is the only test that loads Content. It is deliberately shallow - it answers "does the
 * game boot", not "is the game correct" - but it is the one thing the unit tests cannot cover.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleSmokeLoadM01, "Castle.Smoke.LoadM01",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FCastleAssertM01Playable, FCastleSmokeLoadM01*, Test);

bool FCastleAssertM01Playable::Update()
{
	UWorld* World = nullptr;
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.World() && (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE))
		{
			World = Context.World();
			break;
		}
	}

	if (!World)
	{
		Test->AddError(TEXT("No game world after opening L_M01_CellBlockD."));
		return true;
	}

	// --- mission ------------------------------------------------------------------------------
	const UMissionSubsystem* Missions = World->GetSubsystem<UMissionSubsystem>();
	if (!Missions)
	{
		Test->AddError(TEXT("The level has no UMissionSubsystem."));
	}
	else
	{
		const UMissionDefinition* Mission = Missions->GetCurrentMission();
		Test->TestNotNull(TEXT("The game mode started a mission"), Mission);
		if (Mission)
		{
			Test->TestEqual(TEXT("The mission has four objectives"), Missions->GetActiveObjectives().Num(), 4);
			Test->TestNotNull(TEXT("And a current objective to show on the HUD"), Missions->GetCurrentObjective());
		}
	}

	// --- world actors -------------------------------------------------------------------------
	int32 GuardCount = 0;
	for (TActorIterator<AGuardCharacter> It(World); It; ++It)
	{
		++GuardCount;
	}
	Test->TestEqual(TEXT("Five guards are placed"), GuardCount, 5);

	int32 DoorCount = 0;
	for (TActorIterator<ADoorActor> It(World); It; ++It)
	{
		++DoorCount;
	}
	Test->TestTrue(TEXT("At least one door is placed"), DoorCount >= 1);

	// --- player -------------------------------------------------------------------------------
	const APlayerController* PC = World->GetFirstPlayerController();
	Test->TestNotNull(TEXT("There is a player controller"), PC);
	if (PC)
	{
		Test->TestNotNull(TEXT("The pawn is an ACastleCharacter"), Cast<ACastleCharacter>(PC->GetPawn()));
	}

	return true;
}

bool FCastleSmokeLoadM01::RunTest(const FString& Parameters)
{
	AutomationOpenMap(TEXT("/Game/Maps/L_M01_CellBlockD"));

	// Long enough for BeginPlay, the game mode's StartMission and the guards' first think.
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleAssertM01Playable(this));

	return true;
}

#endif

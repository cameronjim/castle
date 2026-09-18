// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/LightComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavigationSystem.h"
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

/** The game world the map was opened into, or null. */
static UWorld* FindCastleGameWorld()
{
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.World() && (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE))
		{
			return Context.World();
		}
	}
	return nullptr;
}

/**
 * Navigation. Every map logged "LogCrowdFollowing: Warning: Unable to find RecastNavMesh
 * instance" and no guard ever moved: the NavMeshBoundsVolume was placed but nav data was never
 * generated, because runtime generation defaults to Static. Generation is asynchronous, so this
 * polls rather than asserting on one frame.
 */
DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(
	FCastleAssertM01Navigation, FCastleSmokeLoadM01*, Test, float, SecondsLeft);

bool FCastleAssertM01Navigation::Update()
{
	UWorld* World = FindCastleGameWorld();
	if (!World)
	{
		Test->AddError(TEXT("No game world while waiting for navigation data."));
		return true;
	}

	const UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(World);
	const bool bHasNavData = NavSystem && NavSystem->GetDefaultNavDataInstance() != nullptr;

	bool bHasRecastNavMesh = false;
	for (TActorIterator<ARecastNavMesh> It(World); It; ++It)
	{
		bHasRecastNavMesh = true;
		break;
	}

	if (bHasNavData && bHasRecastNavMesh)
	{
		Test->TestTrue(TEXT("The level has navigation data"), true);
		return true;
	}

	SecondsLeft -= FApp::GetDeltaTime();
	if (SecondsLeft > 0.f)
	{
		return false;
	}

	Test->TestNotNull(TEXT("A navigation system exists"), NavSystem);
	Test->TestTrue(TEXT("GetDefaultNavDataInstance() is not null"), bHasNavData);
	Test->TestTrue(TEXT("An ARecastNavMesh was generated for L_M01"), bHasRecastNavMesh);
	return true;
}

/**
 * And the payoff: with a navmesh the guards patrol. Passes as soon as any guard is moving,
 * because a guard standing at a patrol point for PatrolWaitSeconds is not a failure.
 */
DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(
	FCastleAssertGuardsPatrol, FCastleSmokeLoadM01*, Test, float, SecondsLeft);

bool FCastleAssertGuardsPatrol::Update()
{
	UWorld* World = FindCastleGameWorld();
	if (!World)
	{
		Test->AddError(TEXT("No game world while waiting for the guards to move."));
		return true;
	}

	for (TActorIterator<AGuardCharacter> It(World); It; ++It)
	{
		if (It->GetVelocity().Size2D() > 1.f)
		{
			Test->TestTrue(TEXT("At least one guard is patrolling"), true);
			return true;
		}
	}

	SecondsLeft -= FApp::GetDeltaTime();
	if (SecondsLeft > 0.f)
	{
		return false;
	}

	Test->AddError(TEXT("No guard moved: they are placed but nothing is patrolling."));
	return true;
}

bool FCastleAssertM01Playable::Update()
{
	UWorld* World = FindCastleGameWorld();

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

	// --- lighting -----------------------------------------------------------------------------
	// Nothing in the generated maps is built lighting, so a Static light renders as a black
	// surface until someone builds it. Every light has to be Movable.
	int32 StaticLightCount = 0;
	for (TObjectIterator<ULightComponent> It; It; ++It)
	{
		const ULightComponent* Light = *It;
		if (Light && Light->GetWorld() == World && Light->Mobility == EComponentMobility::Static)
		{
			++StaticLightCount;
			Test->AddError(FString::Printf(TEXT("Static light component %s in L_M01."), *Light->GetPathName()));
		}
	}
	Test->TestEqual(TEXT("No light in L_M01 has Static mobility"), StaticLightCount, 0);

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

	// Navigation is built asynchronously and the patrol only starts once it is there.
	ADD_LATENT_AUTOMATION_COMMAND(FCastleAssertM01Navigation(this, 10.f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleAssertGuardsPatrol(this, 10.f));

	return true;
}

#endif

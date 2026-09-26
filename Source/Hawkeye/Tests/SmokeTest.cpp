// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/LightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavigationSystem.h"
#include "Mission/MissionDefinition.h"
#include "Mission/MissionSubsystem.h"
#include "Combat/HealthComponent.h"
#include "Combat/WeaponComponent.h"
#include "Player/HawkeyeCharacter.h"
#include "Tests/AutomationCommon.h"
#include "World/DoorActor.h"
#include "World/ThugCharacter.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Boots /Game/Maps/L_M01_CellBlockD for real and asserts the level is playable: a mission is
 * running with its three objectives, the five thugs and the keycard door exist, the pawn the
 * player is driving is an AHawkeyeCharacter, the thugs move, and a fight has consequences: a thug
 * dies to the player's fists or a thug's bullet hurts the player.
 *
 * This is the only test that loads Content. It is deliberately shallow - it answers "does the
 * game boot", not "is the game correct" - but it is the one thing the unit tests cannot cover.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHawkeyeSmokeLoadM01, "Hawkeye.Smoke.LoadM01",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FHawkeyeAssertM01Playable, FHawkeyeSmokeLoadM01*, Test);

/** The game world the map was opened into, or null. */
static UWorld* FindHawkeyeGameWorld()
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
 * instance" and no thug ever moved: the NavMeshBoundsVolume was placed but nav data was never
 * generated, because runtime generation defaults to Static. Generation is asynchronous, so this
 * polls rather than asserting on one frame.
 */
DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(
	FHawkeyeAssertM01Navigation, FHawkeyeSmokeLoadM01*, Test, float, SecondsLeft);

bool FHawkeyeAssertM01Navigation::Update()
{
	UWorld* World = FindHawkeyeGameWorld();
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
 * And the payoff: with a navmesh the thugs patrol. Passes as soon as any thug is moving,
 * because a thug standing at a patrol point for PatrolWaitSeconds is not a failure.
 */
DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(
	FHawkeyeAssertThugsPatrol, FHawkeyeSmokeLoadM01*, Test, float, SecondsLeft);

bool FHawkeyeAssertThugsPatrol::Update()
{
	UWorld* World = FindHawkeyeGameWorld();
	if (!World)
	{
		Test->AddError(TEXT("No game world while waiting for the thugs to move."));
		return true;
	}

	for (TActorIterator<AThugCharacter> It(World); It; ++It)
	{
		if (It->GetVelocity().Size2D() > 1.f)
		{
			Test->TestTrue(TEXT("At least one thug is patrolling"), true);
			return true;
		}
	}

	SecondsLeft -= FApp::GetDeltaTime();
	if (SecondsLeft > 0.f)
	{
		return false;
	}

	Test->AddError(TEXT("No thug moved: they are placed but nothing is patrolling."));
	return true;
}

/**
 * And a fight has consequences. The player walks up to the nearest living thug, square in front
 * of him, and punches until either the thug dies to the fists or the thug's own shots land. Both
 * are the payoff of the third-person rig and the hitscan still working together: the punch sweeps
 * from the body, not the camera 350 cm behind it, and the thug's bullets block on the player.
 */
DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(
	FHawkeyeAssertFightHasConsequences, FHawkeyeSmokeLoadM01*, Test, float, SecondsLeft);

bool FHawkeyeAssertFightHasConsequences::Update()
{
	UWorld* World = FindHawkeyeGameWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	AHawkeyeCharacter* Player = PC ? Cast<AHawkeyeCharacter>(PC->GetPawn()) : nullptr;
	UWeaponComponent* Fists = Player ? Player->GetWeaponComponent() : nullptr;
	UHealthComponent* PlayerHealth = Player ? Player->GetHealthComponent() : nullptr;
	if (!Fists || !PlayerHealth)
	{
		Test->AddError(TEXT("No player pawn with a weapon and health to start a fight with."));
		return true;
	}

	AThugCharacter* Target = nullptr;
	float NearestDistanceSquared = TNumericLimits<float>::Max();
	for (TActorIterator<AThugCharacter> It(World); It; ++It)
	{
		const UHealthComponent* Health = It->GetHealthComponent();
		if (Health && Health->IsDead())
		{
			Test->AddInfo(FString::Printf(TEXT("%s died to the player's fists."), *It->GetName()));
			Test->TestTrue(TEXT("A thug dies to melee"), true);
			return true;
		}
		const float DistanceSquared = FVector::DistSquared(It->GetActorLocation(), Player->GetActorLocation());
		if (Health && DistanceSquared < NearestDistanceSquared)
		{
			NearestDistanceSquared = DistanceSquared;
			Target = *It;
		}
	}

	if (PlayerHealth->GetCurrentHealth() < PlayerHealth->GetMaxHealth())
	{
		Test->AddInfo(FString::Printf(TEXT("A thug shot the player down to %.0f health."),
			PlayerHealth->GetCurrentHealth()));
		Test->TestTrue(TEXT("A thug's bullet hurts the player"), true);
		return true;
	}

	SecondsLeft -= FApp::GetDeltaTime();
	if (!Target || SecondsLeft <= 0.f)
	{
		Test->AddError(TEXT("No thug died to the player's fists and no thug's shot hurt the player."));
		return true;
	}

	// Stand 90 cm in front of him, feet on his floor, facing him, and swing. The fists' cooldown
	// paces the punches; standing in his sight cone is what lets him shoot back.
	const FVector ThugLocation = Target->GetActorLocation();
	const FVector Stand = ThugLocation + Target->GetActorForwardVector().GetSafeNormal2D() * 90.f
		- FVector(0.f, 0.f, 8.f);
	const FRotator Facing = FRotator(0.f, (ThugLocation - Stand).GetSafeNormal2D().Rotation().Yaw, 0.f);
	Player->TeleportTo(Stand, Facing, false, true);
	PC->SetControlRotation(Facing);
	Fists->Fire();
	return false;
}

bool FHawkeyeAssertM01Playable::Update()
{
	UWorld* World = FindHawkeyeGameWorld();

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
			// leave_cell, security_door, reach_stairwell. find_weapon went with the pistol.
			Test->TestEqual(TEXT("The mission has three objectives"), Missions->GetActiveObjectives().Num(), 3);
			Test->TestNotNull(TEXT("And a current objective to show on the HUD"), Missions->GetCurrentObjective());
		}
	}

	// --- world actors -------------------------------------------------------------------------
	int32 ThugCount = 0;
	for (TActorIterator<AThugCharacter> It(World); It; ++It)
	{
		++ThugCount;
	}
	Test->TestEqual(TEXT("Five thugs are placed"), ThugCount, 5);

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
		const AHawkeyeCharacter* Player = Cast<AHawkeyeCharacter>(PC->GetPawn());
		Test->TestNotNull(TEXT("The pawn is an AHawkeyeCharacter"), Player);
		if (Player)
		{
			// Third person: the mannequin is on the body and the camera is on a boom behind it.
			const USkeletalMeshComponent* Body = Player->GetMesh();
			Test->TestTrue(TEXT("The player body wears a skeletal mesh"),
				Body && Body->GetSkeletalMeshAsset() != nullptr);
			Test->TestTrue(TEXT("And its owner can see it"), Body && !Body->bOwnerNoSee && Body->IsVisible());
			Test->TestNotNull(TEXT("The camera sits on a spring arm"), Player->GetCameraBoom());
		}
	}

	return true;
}

bool FHawkeyeSmokeLoadM01::RunTest(const FString& Parameters)
{
	AutomationOpenMap(TEXT("/Game/Maps/L_M01_CellBlockD"));

	// Long enough for BeginPlay, the game mode's StartMission and the thugs' first think.
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.f));
	ADD_LATENT_AUTOMATION_COMMAND(FHawkeyeAssertM01Playable(this));

	// Navigation is built asynchronously and the patrol only starts once it is there.
	ADD_LATENT_AUTOMATION_COMMAND(FHawkeyeAssertM01Navigation(this, 10.f));
	ADD_LATENT_AUTOMATION_COMMAND(FHawkeyeAssertThugsPatrol(this, 10.f));

	// Last, because it moves the player: a fist fight with the nearest thug.
	ADD_LATENT_AUTOMATION_COMMAND(FHawkeyeAssertFightHasConsequences(this, 15.f));

	return true;
}

#endif

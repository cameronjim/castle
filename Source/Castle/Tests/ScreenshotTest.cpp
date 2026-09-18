// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/HealthComponent.h"
#include "Combat/WeaponComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Player/CastleCharacter.h"
#include "Tests/AutomationCommon.h"
#include "UnrealClient.h"
#include "World/GuardCharacter.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Reference shots of Cell Block D, written to Saved/Screenshots/Room/. These are look-at-them
 * tools, not assertions: they exist so the room and the view model can be reviewed without
 * opening the full editor.
 *
 *   Castle.Screenshot.M01Cell       cell.png, corridor.png, doorway.png - the room, pawn hidden
 *   Castle.Screenshot.M01Viewmodel  viewmodel_hip.png, viewmodel_aim.png, viewmodel_fire.png,
 *                                   guard_dead.png - the pawn visible and armed
 *
 * Both need a real RHI, so they are explicit no-ops in the normal -nullrhi suite:
 *
 *   UnrealEditor-Cmd.exe Castle.uproject -ExecCmds="Automation RunTests Castle.Screenshot; Quit"
 *       -unattended -nosplash -nop4 -stdout
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleScreenshotM01Cell, "Castle.Screenshot.M01Cell",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleScreenshotM01Viewmodel, "Castle.Screenshot.M01Viewmodel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

/** Where the PNGs land. Absolute, because FScreenshotRequest does not resolve /Game paths. */
static FString RoomScreenshotPath(const FString& FileName)
{
	return FPaths::ConvertRelativePathToFull(
		FPaths::ProjectSavedDir() / TEXT("Screenshots") / TEXT("Room") / FileName);
}

/** The game world the map was opened into, or null. */
static UWorld* FindScreenshotWorld()
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

static APawn* FindScreenshotPawn()
{
	UWorld* World = FindScreenshotWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	return PC ? PC->GetPawn() : nullptr;
}

/**
 * Teleport the player pawn and point the camera. The shot comes a beat later.
 *
 * bHidePawn hides the whole actor, which is what the room shots want: the view model is
 * parented to the camera and would otherwise sit in front of every wall.
 */
DEFINE_LATENT_AUTOMATION_COMMAND_FOUR_PARAMETER(
	FCastlePlaceCamera, FAutomationTestBase*, Test, FVector, Location, FRotator, Rotation,
	bool, bHidePawn);

bool FCastlePlaceCamera::Update()
{
	UWorld* World = FindScreenshotWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn)
	{
		Test->AddError(TEXT("No player pawn to position for the screenshot."));
		return true;
	}

	Pawn->TeleportTo(Location, Rotation, false, true);
	PC->SetControlRotation(Rotation);
	Pawn->SetActorHiddenInGame(bHidePawn);
	return true;
}

/** Ask for one screenshot. Separate from the teleport so motion blur has settled first. */
DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(
	FCastleTakeRoomShot, FAutomationTestBase*, Test, FString, FileName);

bool FCastleTakeRoomShot::Update()
{
	const FString FullPath = RoomScreenshotPath(FileName);
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.CreateDirectoryTree(*FPaths::GetPath(FullPath));
	FScreenshotRequest::RequestScreenshot(FullPath, /*bInShowUI=*/false, /*bAddFilenameSuffix=*/false);
	Test->AddInfo(FString::Printf(TEXT("Requested %s"), *FullPath));
	return true;
}

/** Arm Frank, so the view model has a pistol in it. */
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(
	FCastleGiveWeapon, FAutomationTestBase*, Test);

bool FCastleGiveWeapon::Update()
{
	ACastleCharacter* Frank = Cast<ACastleCharacter>(FindScreenshotPawn());
	UWeaponComponent* Weapon = Frank ? Frank->GetWeaponComponent() : nullptr;
	if (!Weapon)
	{
		Test->AddError(TEXT("No weapon component on the player pawn."));
		return true;
	}

	Weapon->GiveWeapon(12, 36);
	Frank->RefreshViewModelForWeapon();
	return true;
}

/** Start or stop aiming, so the aimed pose can be captured. */
DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(
	FCastleSetAiming, FAutomationTestBase*, Test, bool, bAiming);

bool FCastleSetAiming::Update()
{
	ACastleCharacter* Frank = Cast<ACastleCharacter>(FindScreenshotPawn());
	if (!Frank)
	{
		Test->AddError(TEXT("No player pawn to aim."));
		return true;
	}

	if (bAiming)
	{
		Frank->StartAim();
	}
	else
	{
		Frank->StopAim();
	}
	return true;
}

/**
 * Fire and request the shot in the same Update, so the capture lands inside the 0.05 s muzzle
 * flash rather than after it.
 */
DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(
	FCastleFireAndShoot, FAutomationTestBase*, Test, FString, FileName);

bool FCastleFireAndShoot::Update()
{
	ACastleCharacter* Frank = Cast<ACastleCharacter>(FindScreenshotPawn());
	UWeaponComponent* Weapon = Frank ? Frank->GetWeaponComponent() : nullptr;
	if (!Weapon)
	{
		Test->AddError(TEXT("No weapon component to fire."));
		return true;
	}

	Weapon->Fire();
	Frank->PlayFireFeedback();

	const FString FullPath = RoomScreenshotPath(FileName);
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.CreateDirectoryTree(*FPaths::GetPath(FullPath));
	FScreenshotRequest::RequestScreenshot(FullPath, /*bInShowUI=*/false, /*bAddFilenameSuffix=*/false);
	Test->AddInfo(FString::Printf(TEXT("Fired and requested %s"), *FullPath));
	return true;
}

/**
 * Kill the guard nearest the player and leave the camera looking at him, so the next shot
 * answers the only question that matters: is he on the floor or still standing up?
 */
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(
	FCastleKillNearestGuard, FAutomationTestBase*, Test);

bool FCastleKillNearestGuard::Update()
{
	UWorld* World = FindScreenshotWorld();
	APawn* Pawn = FindScreenshotPawn();
	if (!World || !Pawn)
	{
		Test->AddError(TEXT("No world or pawn to kill a guard from."));
		return true;
	}

	AGuardCharacter* Nearest = nullptr;
	float NearestDistanceSquared = TNumericLimits<float>::Max();
	for (TActorIterator<AGuardCharacter> It(World); It; ++It)
	{
		const float DistanceSquared = FVector::DistSquared(It->GetActorLocation(), Pawn->GetActorLocation());
		if (DistanceSquared < NearestDistanceSquared)
		{
			NearestDistanceSquared = DistanceSquared;
			Nearest = *It;
		}
	}

	if (!Nearest || !Nearest->GetHealthComponent())
	{
		Test->AddWarning(TEXT("No guard in the map to kill; skipping guard_dead.png."));
		return true;
	}

	// Stand two metres back from him and look at his chest, then kill him.
	const FVector GuardLocation = Nearest->GetActorLocation();
	const FVector Behind = GuardLocation - FVector(220.f, 0.f, 0.f);
	const FVector Eye = FVector(Behind.X, Behind.Y, 170.f);
	Pawn->TeleportTo(Eye, (GuardLocation - Eye).Rotation(), false, true);
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		PC->SetControlRotation((GuardLocation - Eye).Rotation());
	}

	Nearest->GetHealthComponent()->ApplyDamage(9999.f, Pawn);
	Test->AddInfo(FString::Printf(TEXT("Killed %s; ragdolling=%d, collapsing=%d"),
		*Nearest->GetName(), Nearest->IsRagdolling() ? 1 : 0, Nearest->IsCollapsing() ? 1 : 0));
	return true;
}

/** Report which death path the nearest guard actually took, once the dust has settled. */
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(
	FCastleReportGuardDeathPath, FAutomationTestBase*, Test);

bool FCastleReportGuardDeathPath::Update()
{
	UWorld* World = FindScreenshotWorld();
	if (!World)
	{
		return true;
	}

	for (TActorIterator<AGuardCharacter> It(World); It; ++It)
	{
		if (It->IsLimp())
		{
			Test->AddInfo(FString::Printf(
				TEXT("%s went down by %s (collapse alpha %.2f)."),
				*It->GetName(),
				It->IsRagdolling() ? TEXT("ragdoll") : TEXT("procedural collapse"),
				It->GetCollapseAlpha()));
		}
	}
	return true;
}

/** True when this process cannot render, in which case the screenshot tests do nothing. */
static bool SkipWithoutRHI(FAutomationTestBase& Test)
{
	// -nullrhi cannot render, so the normal suite runs these as explicit no-ops rather than
	// failures. FApp::CanEverRender() reads the -nullrhi switch off the command line, which is
	// exactly the condition we care about. Nothing here asserts; the PNGs are the output.
	if (FApp::CanEverRender())
	{
		return false;
	}
	Test.AddInfo(TEXT("No RHI: skipping the screenshots. Re-run without -nullrhi to capture them."));
	return true;
}

bool FCastleScreenshotM01Cell::RunTest(const FString& Parameters)
{
	if (SkipWithoutRHI(*this))
	{
		return true;
	}

	AutomationOpenMap(TEXT("/Game/Maps/L_M01_CellBlockD"));

	// BeginPlay, the game mode's StartMission, and enough frames for the lights to settle.
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(3.f));

	// Eye height is 170 cm. The cell runs x 0..300, corridor 1 x 300..2300, y -150..150.
	// Each shot is placed, given a second for motion blur to settle, then captured.
	ADD_LATENT_AUTOMATION_COMMAND(FCastlePlaceCamera(this, FVector(60.f, 0.f, 170.f), FRotator(0.f, 0.f, 0.f), true));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleTakeRoomShot(this, TEXT("cell.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));

	ADD_LATENT_AUTOMATION_COMMAND(FCastlePlaceCamera(this, FVector(600.f, 0.f, 170.f), FRotator(-5.f, 0.f, 0.f), true));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleTakeRoomShot(this, TEXT("corridor.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));

	ADD_LATENT_AUTOMATION_COMMAND(FCastlePlaceCamera(this, FVector(700.f, 0.f, 170.f), FRotator(0.f, 180.f, 0.f), true));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleTakeRoomShot(this, TEXT("doorway.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));

	return true;
}

bool FCastleScreenshotM01Viewmodel::RunTest(const FString& Parameters)
{
	if (SkipWithoutRHI(*this))
	{
		return true;
	}

	AutomationOpenMap(TEXT("/Game/Maps/L_M01_CellBlockD"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(3.f));

	// Down the corridor, pawn visible: the pistol is parented to the camera, so it only shows
	// up in a shot where the actor is not hidden.
	ADD_LATENT_AUTOMATION_COMMAND(FCastlePlaceCamera(this, FVector(600.f, 0.f, 170.f), FRotator(-3.f, 0.f, 0.f), false));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleGiveWeapon(this));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleTakeRoomShot(this, TEXT("viewmodel_hip.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));

	ADD_LATENT_AUTOMATION_COMMAND(FCastleSetAiming(this, true));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleTakeRoomShot(this, TEXT("viewmodel_aim.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));

	ADD_LATENT_AUTOMATION_COMMAND(FCastleSetAiming(this, false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleFireAndShoot(this, TEXT("viewmodel_fire.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));

	// And the other half of the playtest: a guard who is supposed to end up on the floor.
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKillNearestGuard(this));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleTakeRoomShot(this, TEXT("guard_dead.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleReportGuardDeathPath(this));

	return true;
}

#endif

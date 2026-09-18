// Copyright Epic Games, Inc. All Rights Reserved.

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Tests/AutomationCommon.h"
#include "UnrealClient.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Takes three reference shots of the Cell Block D art pass and writes them to
 * Saved/Screenshots/Room/. This is a look-at-it tool, not an assertion: it exists so the room
 * can be reviewed without opening the full editor.
 *
 *   cell.png      standing in the cell, looking at the open door
 *   corridor.png  in corridor 1, looking down towards the red emergency light
 *   doorway.png   further down the corridor, looking back at the cell door
 *
 * It needs a real RHI, so it is a no-op in the normal -nullrhi suite and only does work when
 * run with rendering enabled:
 *
 *   UnrealEditor-Cmd.exe Castle.uproject -ExecCmds="Automation RunTests Castle.Screenshot; Quit"
 *       -unattended -nosplash -nop4 -stdout
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleScreenshotM01Cell, "Castle.Screenshot.M01Cell",
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

/** Teleport the player pawn, point the camera, and ask for one screenshot. */
DEFINE_LATENT_AUTOMATION_COMMAND_FOUR_PARAMETER(
	FCastleTakeRoomShot, FCastleScreenshotM01Cell*, Test, FVector, Location, FRotator, Rotation,
	FString, FileName);

bool FCastleTakeRoomShot::Update()
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

	const FString FullPath = RoomScreenshotPath(FileName);
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.CreateDirectoryTree(*FPaths::GetPath(FullPath));
	FScreenshotRequest::RequestScreenshot(FullPath, /*bInShowUI=*/false, /*bAddFilenameSuffix=*/false);
	Test->AddInfo(FString::Printf(TEXT("Requested %s"), *FullPath));
	return true;
}

bool FCastleScreenshotM01Cell::RunTest(const FString& Parameters)
{
	// -nullrhi cannot render, so the normal suite runs this as an explicit no-op rather than a
	// failure. FApp::CanEverRender() reads the -nullrhi switch off the command line, which is
	// exactly the condition we care about. Nothing here asserts; the PNGs are the output.
	if (!FApp::CanEverRender())
	{
		AddInfo(TEXT("No RHI: skipping the room screenshots. Re-run without -nullrhi to capture them."));
		return true;
	}

	AutomationOpenMap(TEXT("/Game/Maps/L_M01_CellBlockD"));

	// BeginPlay, the game mode's StartMission, and enough frames for the lights to settle.
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(3.f));

	// Eye height is 170 cm. The cell runs x 0..300, corridor 1 x 300..2300, y -150..150.
	ADD_LATENT_AUTOMATION_COMMAND(FCastleTakeRoomShot(
		this, FVector(60.f, 0.f, 170.f), FRotator(0.f, 0.f, 0.f), TEXT("cell.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));

	ADD_LATENT_AUTOMATION_COMMAND(FCastleTakeRoomShot(
		this, FVector(600.f, 0.f, 170.f), FRotator(-5.f, 0.f, 0.f), TEXT("corridor.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));

	ADD_LATENT_AUTOMATION_COMMAND(FCastleTakeRoomShot(
		this, FVector(700.f, 0.f, 170.f), FRotator(0.f, 180.f, 0.f), TEXT("doorway.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));

	return true;
}

#endif

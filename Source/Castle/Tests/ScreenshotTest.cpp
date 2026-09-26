// Copyright Epic Games, Inc. All Rights Reserved.

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Combat/HealthComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Player/LocomotionAnim.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "CastlePlayerController.h"
#include "Player/CastleCharacter.h"
#include "Tests/AutomationCommon.h"
#include "UnrealClient.h"
#include "World/ThugCharacter.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Reference shots of L_M01_CellBlockD, written to Saved/Screenshots/Room/. These are look-at-them
 * tools, not assertions: they exist so the room, the player and the thugs can be reviewed without
 * opening the full editor.
 *
 *   Castle.Screenshot.M01Cell         cell.png, corridor.png, doorway.png, station.png,
 *                                     corridor2.png, exitroom.png - the room, pawn hidden
 *   Castle.Screenshot.M01ThirdPerson  third_person.png - the player from the placeholder boom;
 *                                     thug_walking.png, thug_dead.png - the thugs
 *   Castle.Screenshot.Settings        UI/settings.png - the pause menu's Settings screen
 *
 * The fixed shots view through a camera actor placed at the shot's eye, because the pawn's own
 * camera now sits on a boom 350 cm behind it. They need a real RHI, so they are explicit no-ops
 * in the normal -nullrhi suite:
 *
 *   UnrealEditor-Cmd.exe Castle.uproject -ExecCmds="Automation RunTests Castle.Screenshot; Quit"
 *       -unattended -nosplash -nop4 -stdout
 */
/**
 * ClientContext as well as EditorContext, so the same pass can be run inside the standalone
 * game. That is the gap this whole set of shots exists to close: an editor render once looked
 * right while the game, which loads assets rather than finding them resident, did not.
 *
 *   UnrealEditor-Cmd.exe Castle.uproject -game -windowed -ResX=1280 -ResY=720 -unattended
 *       -nosplash -log -ExecCmds="Automation RunTests Castle.Screenshot; Quit"
 */
static constexpr EAutomationTestFlags CastleScreenshotFlags =
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext
	| EAutomationTestFlags::ProductFilter;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleScreenshotM01Cell, "Castle.Screenshot.M01Cell",
	CastleScreenshotFlags)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleScreenshotM01ThirdPerson, "Castle.Screenshot.M01ThirdPerson",
	CastleScreenshotFlags)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleScreenshotSettings, "Castle.Screenshot.Settings",
	CastleScreenshotFlags)

/** Tag on the one camera actor the fixed shots look through. */
static const FName CastleScreenshotCameraTag(TEXT("CastleScreenshotCamera"));

/** Where the PNGs land. Absolute, because FScreenshotRequest does not resolve /Game paths. */
static FString RoomScreenshotPath(const FString& FileName)
{
	return FPaths::ConvertRelativePathToFull(
		FPaths::ProjectSavedDir() / TEXT("Screenshots") / TEXT("Room") / FileName);
}

/** Where the UI shots land, kept apart from the room reference shots. */
static FString UiScreenshotPath(const FString& FileName)
{
	return FPaths::ConvertRelativePathToFull(
		FPaths::ProjectSavedDir() / TEXT("Screenshots") / TEXT("UI") / FileName);
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
 * Look through a camera actor at Location/Rotation instead of the pawn's boom. The same actor is
 * reused for every fixed shot so the world does not fill up with cameras.
 */
static void ViewFromFixedCamera(UWorld* World, APlayerController* PC, const FVector& Location, const FRotator& Rotation)
{
	if (!World || !PC)
	{
		return;
	}

	ACameraActor* Camera = nullptr;
	for (TActorIterator<ACameraActor> It(World); It; ++It)
	{
		if (It->ActorHasTag(CastleScreenshotCameraTag))
		{
			Camera = *It;
			break;
		}
	}

	if (!Camera)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Camera = World->SpawnActor<ACameraActor>(Location, Rotation, SpawnParams);
		if (!Camera)
		{
			return;
		}
		Camera->Tags.Add(CastleScreenshotCameraTag);
		if (UCameraComponent* Lens = Camera->GetCameraComponent())
		{
			Lens->SetFieldOfView(90.f);
			Lens->bConstrainAspectRatio = false;
		}
	}

	Camera->SetActorLocationAndRotation(Location, Rotation);
	PC->SetViewTarget(Camera);
}

/**
 * Put the camera at Location/Rotation for a fixed shot. The pawn is moved there too - so thugs
 * react to it as they always have - and hidden when bHidePawn, which is what the room shots
 * want. The shot comes a beat later.
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

	Pawn->TeleportTo(Location, FRotator(0.f, Rotation.Yaw, 0.f), false, true);
	PC->SetControlRotation(Rotation);
	Pawn->SetActorHiddenInGame(bHidePawn);
	ViewFromFixedCamera(World, PC, Location, Rotation);
	return true;
}

/**
 * The player from the placeholder third-person boom: pawn visible, standing in corridor 1
 * facing down it, the view target back on the pawn so the shot is what the player sees.
 */
DEFINE_LATENT_AUTOMATION_COMMAND_THREE_PARAMETER(
	FCastleViewFromPawn, FAutomationTestBase*, Test, FVector, Location, FRotator, ControlRotation);

bool FCastleViewFromPawn::Update()
{
	UWorld* World = FindScreenshotWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	ACastleCharacter* Player = PC ? Cast<ACastleCharacter>(PC->GetPawn()) : nullptr;
	if (!Player)
	{
		Test->AddError(TEXT("No ACastleCharacter to frame from its own camera."));
		return true;
	}

	Player->SetActorHiddenInGame(false);
	Player->TeleportTo(Location, FRotator(0.f, ControlRotation.Yaw, 0.f), false, true);
	PC->SetControlRotation(ControlRotation);
	PC->SetViewTarget(Player);

	const USkeletalMeshComponent* Body = Player->GetMesh();
	Test->AddInfo(FString::Printf(TEXT("Third-person view: body mesh=%s visible=%d"),
		*GetNameSafe(Body ? Body->GetSkeletalMeshAsset() : nullptr), Body && Body->IsVisible() ? 1 : 0));
	return true;
}

/** Ask for one screenshot. Separate from the placement so motion blur and camera lag settle. */
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

/** Frame a thug from behind while he walks, and report whether his body leads or trails. */
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(
	FCastleFrameWalkingThug, FAutomationTestBase*, Test);

bool FCastleFrameWalkingThug::Update()
{
	UWorld* World = FindScreenshotWorld();
	APawn* Pawn = FindScreenshotPawn();
	if (!World || !Pawn)
	{
		return true;
	}

	AThugCharacter* Walking = nullptr;
	float BestSpeed = 20.f;
	for (TActorIterator<AThugCharacter> It(World); It; ++It)
	{
		const float Speed = It->GetVelocity().Size2D();
		if (!It->IsLimp() && Speed > BestSpeed)
		{
			BestSpeed = Speed;
			Walking = *It;
		}
	}

	if (!Walking)
	{
		Test->AddWarning(TEXT("No thug was moving; skipped thug_walking.png."));
		return true;
	}

	const float Facing = CastleLocomotion::GetFacingAlongVelocity(Walking->GetMesh(), Walking->GetVelocity());
	Test->AddInfo(FString::Printf(TEXT("%s at %.0f cm/s, mesh faces travel by %.2f."),
		*Walking->GetName(), BestSpeed, Facing));
	// A thug turning at the end of his patrol leg is legitimately off-axis for half a second,
	// so only a body actually travelling against its own facing is an error.
	if (Facing < -0.2f)
	{
		Test->AddError(FString::Printf(
			TEXT("%s is walking backwards (facing dot %.2f)."), *Walking->GetName(), Facing));
	}
	else if (Facing < 0.7f)
	{
		Test->AddWarning(FString::Printf(
			TEXT("%s is mid-turn (facing dot %.2f); the shot may not show the walk cycle square on."),
			*Walking->GetName(), Facing));
	}

	// Stand off his shoulder so the shot shows which way the body points against which way it
	// travels; straight behind him hides exactly that. The player is hidden at the eye.
	const FVector Travel = Walking->GetVelocity().GetSafeNormal2D();
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Travel);
	const FVector Body = Walking->GetActorLocation();
	const FVector Eye = Body - Travel * 260.f + Side * 90.f + FVector(0.f, 0.f, 70.f);
	const FRotator Look = (Body - Eye).Rotation();
	Pawn->TeleportTo(Eye, FRotator(0.f, Look.Yaw, 0.f), false, true);
	Pawn->SetActorHiddenInGame(true);
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		PC->SetControlRotation(Look);
		ViewFromFixedCamera(World, PC, Eye, Look);
	}
	return true;
}

/**
 * Kill the thug nearest the player and leave the camera looking at him, so the next shot
 * answers the only question that matters: is he on the floor or still standing up?
 */
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(
	FCastleKillNearestThug, FAutomationTestBase*, Test);

bool FCastleKillNearestThug::Update()
{
	UWorld* World = FindScreenshotWorld();
	APawn* Pawn = FindScreenshotPawn();
	if (!World || !Pawn)
	{
		Test->AddError(TEXT("No world or pawn to kill a thug from."));
		return true;
	}

	AThugCharacter* Nearest = nullptr;
	float NearestDistanceSquared = TNumericLimits<float>::Max();
	for (TActorIterator<AThugCharacter> It(World); It; ++It)
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
		Test->AddWarning(TEXT("No thug in the map to kill; skipping thug_dead.png."));
		return true;
	}

	// Stand two metres back from him and look at his chest, then kill him.
	const FVector ThugLocation = Nearest->GetActorLocation();
	const FVector Behind = ThugLocation - FVector(220.f, 0.f, 0.f);
	const FVector Eye = FVector(Behind.X, Behind.Y, 170.f);
	const FRotator Look = (ThugLocation - Eye).Rotation();
	Pawn->TeleportTo(Eye, FRotator(0.f, Look.Yaw, 0.f), false, true);
	Pawn->SetActorHiddenInGame(true);
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		PC->SetControlRotation(Look);
		ViewFromFixedCamera(World, PC, Eye, Look);
	}

	Nearest->GetHealthComponent()->ApplyDamage(9999.f, Pawn);
	// Killing him detaches the mesh, so where he ends up is not where he stood. The next
	// command re-frames him once the body has finished falling.
	Test->AddInfo(FString::Printf(TEXT("Killed %s; ragdolling=%d, collapsing=%d"),
		*Nearest->GetName(), Nearest->IsRagdolling() ? 1 : 0, Nearest->IsCollapsing() ? 1 : 0));
	return true;
}

/** Stand back from the body that just fell and look down at it. */
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(
	FCastleLookAtDeadThug, FAutomationTestBase*, Test);

bool FCastleLookAtDeadThug::Update()
{
	UWorld* World = FindScreenshotWorld();
	APawn* Pawn = FindScreenshotPawn();
	if (!World || !Pawn)
	{
		return true;
	}

	for (TActorIterator<AThugCharacter> It(World); It; ++It)
	{
		if (!It->IsLimp() || !It->GetMesh())
		{
			continue;
		}

		// The mesh is where the body actually is; the actor stayed where he was shot.
		const FVector Body = It->GetMesh()->GetComponentLocation();
		const FVector Eye = Body - FVector(260.f, 0.f, 0.f) + FVector(0.f, 0.f, 160.f);
		const FRotator Look = (Body - Eye).Rotation();
		Pawn->TeleportTo(Eye, FRotator(0.f, Look.Yaw, 0.f), false, true);
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			PC->SetControlRotation(Look);
			ViewFromFixedCamera(World, PC, Eye, Look);
		}
		Test->AddInfo(FString::Printf(TEXT("Framing %s at %s."), *It->GetName(), *Body.ToCompactString()));
		return true;
	}

	Test->AddWarning(TEXT("No limp thug to frame for thug_dead.png."));
	return true;
}

/** Report which death path the nearest thug actually took, once the dust has settled. */
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(
	FCastleReportThugDeathPath, FAutomationTestBase*, Test);

bool FCastleReportThugDeathPath::Update()
{
	UWorld* World = FindScreenshotWorld();
	if (!World)
	{
		return true;
	}

	for (TActorIterator<AThugCharacter> It(World); It; ++It)
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

/** Open the pause menu and then the settings screen, the way the player would. */
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(
	FCastleOpenSettingsScreen, FAutomationTestBase*, Test);

bool FCastleOpenSettingsScreen::Update()
{
	UWorld* World = FindScreenshotWorld();
	ACastlePlayerController* PC = World ? Cast<ACastlePlayerController>(World->GetFirstPlayerController()) : nullptr;
	if (!PC)
	{
		Test->AddError(TEXT("No ACastlePlayerController to open the settings screen with."));
		return true;
	}

	PC->TogglePause();
	PC->OpenSettings();

	if (!PC->IsSettingsOpen())
	{
		Test->AddError(TEXT("OpenSettings did nothing; check SettingsWidgetClass on BP_CastlePlayerController."));
	}
	return true;
}

/** Ask for one screenshot under Saved/Screenshots/UI. */
DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(
	FCastleTakeUiShot, FAutomationTestBase*, Test, FString, FileName);

bool FCastleTakeUiShot::Update()
{
	const FString FullPath = UiScreenshotPath(FileName);
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.CreateDirectoryTree(*FPaths::GetPath(FullPath));
	FScreenshotRequest::RequestScreenshot(FullPath, /*bInShowUI=*/true, /*bAddFilenameSuffix=*/false);
	Test->AddInfo(FString::Printf(TEXT("Requested %s"), *FullPath));
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

	// The end of corridor 1, looking into the station room. This is the shot that answers
	// whether the keycard door at x = 2900 reads as the way forward.
	ADD_LATENT_AUTOMATION_COMMAND(FCastlePlaceCamera(this, FVector(2150.f, 0.f, 170.f), FRotator(-2.f, 0.f, 0.f), true));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleTakeRoomShot(this, TEXT("station.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));

	// Through the keycard door and down corridor 2. Corridor 2 runs x 2900..4900 into the exit
	// room at 4900..5500.
	ADD_LATENT_AUTOMATION_COMMAND(FCastlePlaceCamera(this, FVector(2960.f, 0.f, 170.f), FRotator(-2.f, 0.f, 0.f), true));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleTakeRoomShot(this, TEXT("corridor2.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));

	// And the exit room itself, the far end of the level.
	ADD_LATENT_AUTOMATION_COMMAND(FCastlePlaceCamera(this, FVector(4820.f, 0.f, 170.f), FRotator(-2.f, 0.f, 0.f), true));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleTakeRoomShot(this, TEXT("exitroom.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));

	return true;
}

bool FCastleScreenshotM01ThirdPerson::RunTest(const FString& Parameters)
{
	if (SkipWithoutRHI(*this))
	{
		return true;
	}

	AutomationOpenMap(TEXT("/Game/Maps/L_M01_CellBlockD"));
	// The first second or so still renders the editor's own billboards and volume wireframes
	// over the game view when this runs inside the editor.
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.f));

	// The player in corridor 1, facing down it, seen from the placeholder boom. The boom lags,
	// so it gets a second and a half to catch up with the teleport before the capture.
	ADD_LATENT_AUTOMATION_COMMAND(FCastleViewFromPawn(this, FVector(900.f, 0.f, 100.f), FRotator(-10.f, 0.f, 0.f)));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleTakeRoomShot(this, TEXT("third_person.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));

	// A thug mid-patrol, framed off his shoulder: the shot that answers "do they walk forwards".
	ADD_LATENT_AUTOMATION_COMMAND(FCastleFrameWalkingThug(this));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleTakeRoomShot(this, TEXT("thug_walking.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));

	// And a thug who is supposed to end up on the floor.
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKillNearestThug(this));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleLookAtDeadThug(this));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleTakeRoomShot(this, TEXT("thug_dead.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleReportThugDeathPath(this));

	return true;
}

bool FCastleScreenshotSettings::RunTest(const FString& Parameters)
{
	if (SkipWithoutRHI(*this))
	{
		return true;
	}

	AutomationOpenMap(TEXT("/Game/Maps/L_M01_CellBlockD"));
	// Long enough for the editor's own billboards to stop drawing over the game view.
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.f));

	ADD_LATENT_AUTOMATION_COMMAND(FCastleOpenSettingsScreen(this));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleTakeUiShot(this, TEXT("settings.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));

	return true;
}

#endif

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/HealthComponent.h"
#include "Combat/WeaponComponent.h"
#include "Combat/WeaponDefinition.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Player/InventoryComponent.h"
#include "Player/LocomotionAnim.h"
#include "World/PickupActor.h"
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
#include "World/GuardCharacter.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Reference shots of Cell Block D, written to Saved/Screenshots/Room/. These are look-at-them
 * tools, not assertions: they exist so the room and the view model can be reviewed without
 * opening the full editor.
 *
 *   Castle.Screenshot.M01Cell       cell.png, corridor.png, doorway.png, station.png,
 *                                   corridor2.png, exitroom.png - the room, pawn hidden
 *   Castle.Screenshot.M01Viewmodel  viewmodel_fists.png, viewmodel_lookdown.png,
 *                                   viewmodel_hip.png, viewmodel_aim.png, viewmodel_fire.png,
 *                                   guard_walking.png, guard_dead.png - the pawn visible
 *   Castle.Screenshot.Settings      UI/settings.png - the pause menu's Settings screen
 *
 * Both need a real RHI, so they are explicit no-ops in the normal -nullrhi suite:
 *
 *   UnrealEditor-Cmd.exe Castle.uproject -ExecCmds="Automation RunTests Castle.Screenshot; Quit"
 *       -unattended -nosplash -nop4 -stdout
 */
/**
 * ClientContext as well as EditorContext, so the same pass can be run inside the standalone
 * game. That is the gap this whole set of shots exists to close: an editor render told us the
 * pistol was in Frank's hand while the game, which loads the definition rather than the
 * Blueprint default, showed nothing at all.
 *
 *   UnrealEditor-Cmd.exe Castle.uproject /Game/Maps/L_M01_CellBlockD -game -windowed
 *       -ResX=1280 -ResY=720 -unattended -nosplash
 *       -ExecCmds="Automation RunTests Castle.Screenshot.M01Cell; Quit"
 */
static constexpr EAutomationTestFlags CastleScreenshotFlags =
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext
	| EAutomationTestFlags::ProductFilter;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleScreenshotM01Cell, "Castle.Screenshot.M01Cell",
	CastleScreenshotFlags)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleScreenshotM01Viewmodel, "Castle.Screenshot.M01Viewmodel",
	CastleScreenshotFlags)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleScreenshotSettings, "Castle.Screenshot.Settings",
	CastleScreenshotFlags)

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

	// Yaw only on the actor: a character never pitches (bUseControllerRotationPitch is off), and
	// teleporting one nose-down swings his body out of his own camera, which is precisely what
	// the look-down shot is meant to show.
	Pawn->TeleportTo(Location, FRotator(0.f, Rotation.Yaw, 0.f), false, true);
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

/**
 * Arm Frank the way the game does: through a weapon pickup in the level.
 *
 * This used to call WeaponComponent::GiveWeapon, which arms the component but leaves
 * ActiveDefinition null - so the view model kept the Blueprint's default mesh and its C++ grip
 * rotation, and the shot looked right while the real game, which goes through the definition,
 * showed no gun at all. Anything the pickup path gets wrong now gets it wrong here too.
 */
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(
	FCastleGiveWeapon, FAutomationTestBase*, Test);

bool FCastleGiveWeapon::Update()
{
	UWorld* World = FindScreenshotWorld();
	ACastleCharacter* Frank = Cast<ACastleCharacter>(FindScreenshotPawn());
	if (!World || !Frank || !Frank->GetInventoryComponent())
	{
		Test->AddError(TEXT("No player pawn with an inventory to arm."));
		return true;
	}

	for (TActorIterator<APickupActor> It(World); It; ++It)
	{
		if (It->PickupType != EPickupType::Weapon || It->Weapon.IsNull())
		{
			continue;
		}
		Test->AddInfo(FString::Printf(TEXT("Taking %s through the real pickup path."), *It->GetName()));
		It->ApplyTo(Frank);
		break;
	}

	const UWeaponComponent* Weapon = Frank->GetWeaponComponent();
	if (!Weapon || !Weapon->HasWeapon())
	{
		Test->AddError(TEXT("No weapon pickup in the map armed the player; the hip shot has no gun."));
		return true;
	}

	Frank->RefreshViewModelForWeapon();
	const UStaticMeshComponent* WeaponMesh = Frank->GetWeaponMesh();
	Test->AddInfo(FString::Printf(TEXT("View model: mesh=%s hidden=%d definition=%s"),
		*GetNameSafe(WeaponMesh ? WeaponMesh->GetStaticMesh() : nullptr),
		WeaponMesh && WeaponMesh->bHiddenInGame ? 1 : 0,
		*GetNameSafe(Weapon->GetActiveDefinition())));
	if (WeaponMesh && (!WeaponMesh->GetStaticMesh() || WeaponMesh->bHiddenInGame))
	{
		Test->AddError(TEXT("The view model pistol is missing or hidden after the pickup."));
	}
	return true;
}

/** Frame a guard from behind while he walks, and report whether his body leads or trails. */
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(
	FCastleFrameWalkingGuard, FAutomationTestBase*, Test);

bool FCastleFrameWalkingGuard::Update()
{
	UWorld* World = FindScreenshotWorld();
	APawn* Pawn = FindScreenshotPawn();
	if (!World || !Pawn)
	{
		return true;
	}

	AGuardCharacter* Walking = nullptr;
	float BestSpeed = 20.f;
	for (TActorIterator<AGuardCharacter> It(World); It; ++It)
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
		Test->AddWarning(TEXT("No guard was moving; skipped guard_walking.png."));
		return true;
	}

	const float Facing = CastleLocomotion::GetFacingAlongVelocity(Walking->GetMesh(), Walking->GetVelocity());
	Test->AddInfo(FString::Printf(TEXT("%s at %.0f cm/s, mesh faces travel by %.2f."),
		*Walking->GetName(), BestSpeed, Facing));
	// A guard turning at the end of his patrol leg is legitimately off-axis for half a second,
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
	// travels; straight behind him hides exactly that.
	const FVector Travel = Walking->GetVelocity().GetSafeNormal2D();
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Travel);
	const FVector Body = Walking->GetActorLocation();
	const FVector Eye = Body - Travel * 260.f + Side * 90.f + FVector(0.f, 0.f, 70.f);
	const FRotator Look = (Body - Eye).Rotation();
	Pawn->TeleportTo(Eye, Look, false, true);
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		PC->SetControlRotation(Look);
	}
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
	// Killing him detaches the mesh, so where he ends up is not where he stood. The next
	// command re-frames him once the body has finished falling.
	Test->AddInfo(FString::Printf(TEXT("Killed %s; ragdolling=%d, collapsing=%d"),
		*Nearest->GetName(), Nearest->IsRagdolling() ? 1 : 0, Nearest->IsCollapsing() ? 1 : 0));
	return true;
}

/** Stand back from the body that just fell and look down at it. */
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(
	FCastleLookAtDeadGuard, FAutomationTestBase*, Test);

bool FCastleLookAtDeadGuard::Update()
{
	UWorld* World = FindScreenshotWorld();
	APawn* Pawn = FindScreenshotPawn();
	if (!World || !Pawn)
	{
		return true;
	}

	for (TActorIterator<AGuardCharacter> It(World); It; ++It)
	{
		if (!It->IsLimp() || !It->GetMesh())
		{
			continue;
		}

		// The mesh is where the body actually is; the actor stayed where he was shot.
		const FVector Body = It->GetMesh()->GetComponentLocation();
		const FVector Eye = Body - FVector(260.f, 0.f, 0.f) + FVector(0.f, 0.f, 160.f);
		const FRotator Look = (Body - Eye).Rotation();
		Pawn->TeleportTo(Eye, Look, false, true);
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			PC->SetControlRotation(Look);
		}
		Test->AddInfo(FString::Printf(TEXT("Framing %s at %s."), *It->GetName(), *Body.ToCompactString()));
		return true;
	}

	Test->AddWarning(TEXT("No limp guard to frame for guard_dead.png."));
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

	// The end of corridor 1, looking into the guard station. This is the shot that answers
	// whether the keycard door at x = 2900 reads as the way forward, and whether the art pass's
	// door frame fights with the door Blueprint's own.
	ADD_LATENT_AUTOMATION_COMMAND(FCastlePlaceCamera(this, FVector(2150.f, 0.f, 170.f), FRotator(-2.f, 0.f, 0.f), true));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleTakeRoomShot(this, TEXT("station.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));

	// Through the keycard door and down corridor 2, which is where "second room is absolutely
	// pitch black" was. Corridor 2 runs x 2900..4900 into the exit room at 4900..5500.
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

bool FCastleScreenshotM01Viewmodel::RunTest(const FString& Parameters)
{
	if (SkipWithoutRHI(*this))
	{
		return true;
	}

	AutomationOpenMap(TEXT("/Game/Maps/L_M01_CellBlockD"));
	// Longer than the room pass: the first second or so still renders the editor's own
	// billboards and volume wireframes over the game view, which spoils a view model shot.
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.f));

	// Down the corridor, pawn visible: the view model hangs off the camera, so it only shows
	// up in a shot where the actor is not hidden.
	ADD_LATENT_AUTOMATION_COMMAND(FCastlePlaceCamera(this, FVector(600.f, 0.f, 170.f), FRotator(-3.f, 0.f, 0.f), false));

	// Empty-handed first: this is the shot that says whether the fists read as fists.
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleTakeRoomShot(this, TEXT("viewmodel_fists.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));

	// Looking at his own feet: the one shot that shows the body under the camera, so a white
	// mannequin leg or a missing torso is visible before a playtest finds it.
	ADD_LATENT_AUTOMATION_COMMAND(FCastlePlaceCamera(this, FVector(600.f, 0.f, 170.f), FRotator(-70.f, 0.f, 0.f), false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleTakeRoomShot(this, TEXT("viewmodel_lookdown.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));

	ADD_LATENT_AUTOMATION_COMMAND(FCastlePlaceCamera(this, FVector(600.f, 0.f, 170.f), FRotator(-3.f, 0.f, 0.f), false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.3f));
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

	// A guard mid-patrol, framed off his shoulder: the shot that answers "do they walk forwards".
	ADD_LATENT_AUTOMATION_COMMAND(FCastleFrameWalkingGuard(this));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleTakeRoomShot(this, TEXT("guard_walking.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));

	// And the other half of the playtest: a guard who is supposed to end up on the floor.
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKillNearestGuard(this));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleLookAtDeadGuard(this));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleTakeRoomShot(this, TEXT("guard_dead.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleReportGuardDeathPath(this));

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

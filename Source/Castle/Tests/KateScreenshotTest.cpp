// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimSingleNodeInstance.h"
#include "Camera/CameraComponent.h"
#include "CollisionQueryParams.h"
#include "Combat/HealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/SpringArmComponent.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Player/CastleCharacter.h"
#include "Tests/AutomationCommon.h"
#include "UnrealClient.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Kate on the East Village block, seen through her own third-person camera, written to
 * Saved/Screenshots/Kate/. Look-at-them tools for tuning the camera, not assertions:
 *
 *   kate_street.png  hip camera, Kate on the street beside Tompkins Square Park, looking along it
 *   kate_aim.png     the same spot, aiming (camera in over the right shoulder)
 *   kate_roof.png    on the tallest roof near the start, looking across to the park
 *   kate_wall.png    back against a tenement, looking out: the arm has to pull in rather than
 *                    put the lens inside the building, and the body hides once the lens is on it
 *
 * Each shot reports how far the arm pulled in and warns if the lens is inside geometry. The pass
 * ends with a 12 m drop onto the street and reports the landing height and health it cost.
 * Needs a real RHI; run from the standalone game (claude-docs/testing.md section 2b):
 *
 *   UnrealEditor-Cmd.exe Castle.uproject -game -windowed -ResX=1280 -ResY=720 -unattended
 *       -nosplash -log -ExecCmds="Automation RunTests Castle.Screenshot.Kate; Quit"
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleScreenshotKate, "Castle.Screenshot.Kate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext
	| EAutomationTestFlags::ProductFilter)

namespace CastleKateShots
{
	static const FName BuildingTag(TEXT("CityBuilding"));
	static const FName ParkTag(TEXT("CityPark"));
	static constexpr float StreetOffsetFromPark = 450.f;
	static constexpr float RooftopSearchRadius = 12000.f;
	static constexpr float HipPitch = -12.f;

	enum class EShot : uint8
	{
		Street,
		Aim,
		Roof,
		Wall,
	};

	static FString ShotPath(const FString& FileName)
	{
		return FPaths::ConvertRelativePathToFull(
			FPaths::ProjectSavedDir() / TEXT("Screenshots") / TEXT("Kate") / FileName);
	}

	static UWorld* FindWorld()
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

	static bool FindParkBounds(UWorld* World, FBox& OutBox)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->Tags.Contains(ParkTag))
			{
				OutBox = It->GetComponentsBoundingBox();
				return true;
			}
		}
		return false;
	}

	static bool FindTallestRoofNear(UWorld* World, const FVector& Origin, FVector& OutTopCentre)
	{
		float BestTop = -BIG_NUMBER;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (!It->Tags.Contains(BuildingTag))
			{
				continue;
			}
			FVector Centre, Extent;
			It->GetActorBounds(false, Centre, Extent);
			if (FVector::Dist2D(Centre, Origin) <= RooftopSearchRadius && Centre.Z + Extent.Z > BestTop)
			{
				BestTop = Centre.Z + Extent.Z;
				OutTopCentre = FVector(Centre.X, Centre.Y, BestTop);
			}
		}
		return BestTop > -BIG_NUMBER;
	}

	/** The street spot beside the park on the side nearest the start, and the outward direction. */
	static bool FindStreetSpot(UWorld* World, FVector& OutSpot, FVector& OutAwayFromPark)
	{
		FBox Park;
		TActorIterator<APlayerStart> StartIt(World);
		if (!StartIt || !FindParkBounds(World, Park))
		{
			return false;
		}
		const FVector Start = StartIt->GetActorLocation();
		const FVector Edge = Park.GetClosestPointTo(Start);
		OutAwayFromPark = (Start - Edge).GetSafeNormal2D();
		if (OutAwayFromPark.IsNearlyZero())
		{
			OutAwayFromPark = FVector(0.f, 1.f, 0.f);
		}
		OutSpot = Edge + OutAwayFromPark * StreetOffsetFromPark;
		return true;
	}

	/** Drops a line from high above XY to whatever is below and returns the surface point. */
	static bool FindGround(UWorld* World, const FVector& XY, float FromZ, const AActor* Ignore, FVector& OutGround)
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(KateShotGround), false, Ignore);
		FHitResult Hit;
		const FVector From(XY.X, XY.Y, FromZ);
		const FVector To(XY.X, XY.Y, -5000.f);
		if (!World->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Params))
		{
			return false;
		}
		OutGround = Hit.ImpactPoint;
		return true;
	}

	/** Stands Kate on Ground facing Yaw and looks through her own camera. */
	static void PlaceKate(ACastleCharacter* Kate, APlayerController* PC, const FVector& Ground, float Yaw, float Pitch)
	{
		const float HalfHeight = Kate->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		Kate->StopAim();
		Kate->SetActorHiddenInGame(false);
		Kate->TeleportTo(Ground + FVector(0.f, 0.f, HalfHeight + 2.f), FRotator(0.f, Yaw, 0.f), false, true);
		PC->SetControlRotation(FRotator(Pitch, Yaw, 0.f));
		PC->SetViewTarget(Kate);
	}
}

/** Frame one shot: move Kate, set the view. The capture comes a beat later so lag settles. */
DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FCastleKateFrameShot, FAutomationTestBase*, Test, uint8, Shot);

bool FCastleKateFrameShot::Update()
{
	using namespace CastleKateShots;

	UWorld* World = FindWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	ACastleCharacter* Kate = PC ? Cast<ACastleCharacter>(PC->GetPawn()) : nullptr;
	FVector Spot, Away;
	if (!Kate || !FindStreetSpot(World, Spot, Away))
	{
		Test->AddError(TEXT("No Kate, PlayerStart or CityPark in L_District_EastVillage."));
		return true;
	}

	// Along the street, with the park on Kate's left so the camera looks past it down the block.
	const float AlongYaw = FVector::CrossProduct(FVector::UpVector, Away).Rotation().Yaw;
	FVector Ground;
	switch (static_cast<EShot>(Shot))
	{
	case EShot::Street:
		if (FindGround(World, Spot, 3000.f, Kate, Ground))
		{
			PlaceKate(Kate, PC, Ground, AlongYaw, HipPitch);
		}
		break;

	case EShot::Aim:
		Kate->StartAim();
		break;

	case EShot::Roof:
	{
		FVector Top;
		if (FindTallestRoofNear(World, Spot, Top) && FindGround(World, Top, Top.Z + 500.f, Kate, Ground))
		{
			const float ParkYaw = (-Away).Rotation().Yaw;
			PlaceKate(Kate, PC, Ground, ParkYaw, -15.f);
			Test->AddInfo(FString::Printf(TEXT("Roof at %.0f cm."), Ground.Z));
		}
		break;
	}

	case EShot::Wall:
	{
		// Walk out from the street spot, away from the park, until a tenement stops the line.
		FCollisionQueryParams Params(SCENE_QUERY_STAT(KateShotWall), false, Kate);
		FHitResult Hit;
		FVector Street;
		const bool bHasStreet = FindGround(World, Spot, 3000.f, Kate, Street);
		const FVector Chest = Street + FVector(0.f, 0.f, 120.f);
		if (bHasStreet && World->LineTraceSingleByChannel(Hit, Chest, Chest + Away * 5000.f, ECC_Visibility, Params)
			&& FindGround(World, Hit.ImpactPoint - Away * 60.f, Hit.ImpactPoint.Z + 200.f, Kate, Ground))
		{
			PlaceKate(Kate, PC, Ground, (-Away).Rotation().Yaw, HipPitch);
			Test->AddInfo(FString::Printf(TEXT("Wall hit on %s at %s."),
				*GetNameSafe(Hit.GetActor()), *Hit.ImpactPoint.ToCompactString()));
		}
		else
		{
			Test->AddWarning(TEXT("No wall found across the street; kate_wall.png shows the street."));
		}
		break;
	}
	}

	Test->AddInfo(FString::Printf(TEXT("Kate at %s, control %s."),
		*Kate->GetActorLocation().ToCompactString(), *PC->GetControlRotation().ToCompactString()));
	return true;
}

/** Reports where the lens ended up: arm length after the probe, and whether it is inside anything. */
DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FCastleKateReportCamera, FAutomationTestBase*, Test, FString, Label);

bool FCastleKateReportCamera::Update()
{
	UWorld* World = CastleKateShots::FindWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	ACastleCharacter* Kate = PC ? Cast<ACastleCharacter>(PC->GetPawn()) : nullptr;
	if (!Kate || !Kate->GetFollowCamera() || !Kate->GetCameraBoom())
	{
		return true;
	}

	const FVector Lens = Kate->GetFollowCamera()->GetComponentLocation();
	const float Distance = FVector::Dist(Lens, Kate->GetCameraBoom()->GetComponentLocation());
	FCollisionQueryParams Params(SCENE_QUERY_STAT(KateShotLens), false, Kate);
	const bool bInside = World->OverlapAnyTestByChannel(
		Lens, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(4.f), Params);

	Test->AddInfo(FString::Printf(TEXT("%s: arm %.0f, lens %.0f cm from pivot, FOV %.0f, in geometry=%d, body hidden=%d"),
		*Label, Kate->GetCameraBoom()->TargetArmLength, Distance, Kate->GetCurrentFOV(), bInside ? 1 : 0,
		Kate->GetMesh() && Kate->GetMesh()->bOwnerNoSee ? 1 : 0));
	Test->AddInfo(FString::Printf(TEXT("%s: %s, mesh playing %s"), *Label, *Kate->GetMovementDebugText(),
		*GetNameSafe(Kate->GetMesh() && Kate->GetMesh()->GetSingleNodeInstance()
			? Kate->GetMesh()->GetSingleNodeInstance()->GetAnimationAsset() : nullptr)));
	if (bInside)
	{
		Test->AddWarning(FString::Printf(TEXT("%s: the camera is inside geometry."), *Label));
	}
	return true;
}

/**
 * Drops Kate from DropHeight above the street spot. With bReport false it only lifts her; with
 * bReport true (seconds later) it reads back the landing the character measured and the health
 * it cost, so the Landed path is exercised in a real world and not only in a unit test.
 */
DEFINE_LATENT_AUTOMATION_COMMAND_THREE_PARAMETER(
	FCastleKateDrop, FAutomationTestBase*, Test, float, DropHeight, bool, bReport);

bool FCastleKateDrop::Update()
{
	using namespace CastleKateShots;

	UWorld* World = FindWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	ACastleCharacter* Kate = PC ? Cast<ACastleCharacter>(PC->GetPawn()) : nullptr;
	FVector Spot, Away, Ground;
	if (!Kate || !FindStreetSpot(World, Spot, Away) || !FindGround(World, Spot, 3000.f, Kate, Ground))
	{
		Test->AddWarning(TEXT("Could not set up the drop."));
		return true;
	}

	const UHealthComponent* Health = Kate->GetHealthComponent();
	if (!bReport)
	{
		PlaceKate(Kate, PC, Ground + FVector(0.f, 0.f, DropHeight), (-Away).Rotation().Yaw, HipPitch);
		return true;
	}

	Test->AddInfo(FString::Printf(TEXT("Drop from %.0f cm: landing measured %.0f cm, health %.1f / %.1f"),
		DropHeight, Kate->GetLastFallHeight(), Health ? Health->GetCurrentHealth() : -1.f,
		Health ? Health->GetMaxHealth() : -1.f));
	if (FMath::Abs(Kate->GetLastFallHeight() - DropHeight) > 50.f)
	{
		Test->AddWarning(TEXT("The measured landing is not the drop height; check Landed and the apex tracking."));
	}
	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FCastleKateTakeShot, FAutomationTestBase*, Test, FString, FileName);

bool FCastleKateTakeShot::Update()
{
	const FString FullPath = CastleKateShots::ShotPath(FileName);
	FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(FullPath));
	// With UI, so the aim shot shows the reticle and the hip shots show there is none.
	FScreenshotRequest::RequestScreenshot(FullPath, /*bInShowUI=*/true, /*bAddFilenameSuffix=*/false);
	Test->AddInfo(FString::Printf(TEXT("Requested %s"), *FullPath));
	return true;
}

bool FCastleScreenshotKate::RunTest(const FString& Parameters)
{
	using namespace CastleKateShots;

	if (!FApp::CanEverRender())
	{
		AddInfo(TEXT("No RHI: skipping the Kate screenshots. Re-run without -nullrhi to capture them."));
		return true;
	}

	AutomationOpenMap(TEXT("/Game/Maps/L_District_EastVillage"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(6.f));

	// The debug line goes into the shots so gait and fall height can be read off them.
	ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(TEXT("castle.DebugMovement 1")));

	const TPair<EShot, const TCHAR*> Shots[] = {
		{ EShot::Street, TEXT("kate_street.png") },
		{ EShot::Aim, TEXT("kate_aim.png") },
		{ EShot::Roof, TEXT("kate_roof.png") },
		{ EShot::Wall, TEXT("kate_wall.png") },
	};
	for (const TPair<EShot, const TCHAR*>& ShotAndFile : Shots)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FCastleKateFrameShot(this, static_cast<uint8>(ShotAndFile.Key)));
		// Camera lag and the aim blend both need to arrive before the capture.
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.f));
		ADD_LATENT_AUTOMATION_COMMAND(FCastleKateReportCamera(this, FString(ShotAndFile.Value)));
		ADD_LATENT_AUTOMATION_COMMAND(FCastleKateTakeShot(this, FString(ShotAndFile.Value)));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));
	}
	// A 12 m drop: past the roll height and the fall-damage threshold.
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateDrop(this, 1200.f, false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(3.f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateDrop(this, 1200.f, true));

	ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(TEXT("castle.DebugMovement 0")));
	return true;
}

#endif

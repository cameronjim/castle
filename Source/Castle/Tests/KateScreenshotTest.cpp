// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimInstance.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Camera/CameraComponent.h"
#include "CastlePlayerController.h"
#include "CollisionQueryParams.h"
#include "Combat/ArrowDefinition.h"
#include "Combat/ArrowProjectile.h"
#include "Combat/BowComponent.h"
#include "Combat/BowDefinition.h"
#include "Combat/HealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/SpringArmComponent.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Mission/ObjectiveTriggerVolume.h"
#include "Player/CastleCharacter.h"
#include "Player/GrappleComponent.h"
#include "Player/InventoryComponent.h"
#include "Player/ParkourComponent.h"
#include "Tests/AutomationCommon.h"
#include "UI/CastleHudWidget.h"
#include "UnrealClient.h"
#include "World/GrappleAnchor.h"
#include "World/ThugCharacter.h"

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
 *   kate_run.png     1.5 s into a run along the street, seen side on (IA_Move injected, so it goes
 *                    through the real input path and gait selection): the motion-matched run
 *                    pose, leaning, arms swinging
 *   kate_stop.png    0.3 s after letting go: the stop, not a snap to idle
 *   kate_aimstrafe.png aiming and strafing right, facing the camera's way
 *
 *   grapple_marker.png  back on the street, looking up at a tenement anchor with its marker showing
 *   grapple_mid.png     part way along the zip, the camera following
 *   grapple_roof.png    landed on the anchor's roof
 *
 *   kate_lookup.png  on the street 13 m out from a facade, the camera pitched straight at a rooftop
 *                    anchor: the arm shortens and lifts, so Kate stays out of the centre and the lens
 *                    off the pavement (the report gives the lens height and Kate's screen box)
 *   vault_mid.png    sprinting at City_Test_Vault (90 cm): the auto vault, mid-move
 *   mantle_mid.png   the jump key 36 cm from City_Test_Mantle (150 cm): hands on the top
 *   ledge_hang.png   dropped in against a tenement 230 cm under its parapet top: caught, hanging
 *   climb_top.png    the jump key from the hang: over the parapet onto the roof
 *
 *   bow_holstered.png  hip camera on the street: DA_Bow_Kate across Kate's back
 *   quiver_hud.png     the same frame for the hotbar: standard arrows "30", grapple "6/6"
 *   bow_draw.png       half drawn: bow in the left hand, the spread ring and the draw bar
 *   bow_hit.png        a full draw loosed at a thug 15 m down the street, stuck in him, reticle flashing
 *
 * At the end two thugs are put on the cross_block roof (with their AI) and reported.
 *
 * Each shot reports how far the arm pulled in and warns if the lens is inside geometry. The pass
 * drops Kate 12 m onto the street and reports the landing height and health it cost, then fires
 * the grapple and reports the target, the zip's length, time and speed, and where she landed.
 * The average frame time over 60 frames is logged on the street and again on the roof.
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
		Run,
		Stop,
		AimStrafe,
		EndMove,
	};

	static const TCHAR* MoveActionPath = TEXT("/Game/Input/IA_Move.IA_Move");

	/**
	 * Holds IA_Move at Value through Enhanced Input's continuous injection (every tick until
	 * stopped), or stops it when bHold is false. Injection runs the same triggers and bindings a
	 * key press would, so Input_Move picks the gait exactly as it does in play.
	 */
	static void HoldMove(APlayerController* PC, const FVector2D& Value, bool bHold)
	{
		const UInputAction* Move = LoadObject<UInputAction>(nullptr, MoveActionPath);
		ULocalPlayer* Player = PC ? PC->GetLocalPlayer() : nullptr;
		UEnhancedInputLocalPlayerSubsystem* Input = Player
			? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Player) : nullptr;
		if (!Move || !Input)
		{
			return;
		}
		if (bHold)
		{
			Input->StartContinuousInputInjectionForAction(Move, FInputActionValue(Value), {}, {});
		}
		else
		{
			Input->StopContinuousInputInjectionForAction(Move);
		}
	}

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

	case EShot::Run:
		// Camera looking at the park, so "right" on the stick runs her along the street, side on.
		if (FindGround(World, Spot, 3000.f, Kate, Ground))
		{
			PlaceKate(Kate, PC, Ground, (-Away).Rotation().Yaw, HipPitch);
			HoldMove(PC, FVector2D(1.f, 0.f), true);
		}
		break;

	case EShot::Stop:
		HoldMove(PC, FVector2D::ZeroVector, false);
		break;

	case EShot::AimStrafe:
		Kate->StartAim();
		HoldMove(PC, FVector2D(1.f, 0.f), true);
		break;

	case EShot::EndMove:
		HoldMove(PC, FVector2D::ZeroVector, false);
		Kate->StopAim();
		break;
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
	Test->AddInfo(FString::Printf(TEXT("%s: %s, mesh playing %s, anim instance %s, max walk speed %.0f"),
		*Label, *Kate->GetMovementDebugText(),
		*GetNameSafe(Kate->GetMesh() && Kate->GetMesh()->GetSingleNodeInstance()
			? Kate->GetMesh()->GetSingleNodeInstance()->GetAnimationAsset() : nullptr),
		*GetNameSafe(Kate->GetMesh() && Kate->GetMesh()->GetAnimInstance()
			? Kate->GetMesh()->GetAnimInstance()->GetClass() : nullptr),
		Kate->GetCharacterMovement() ? Kate->GetCharacterMovement()->MaxWalkSpeed : 0.f));
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

namespace CastleKateShots
{
	static ACastleCharacter* FindKate(APlayerController*& OutPC)
	{
		UWorld* World = FindWorld();
		OutPC = World ? World->GetFirstPlayerController() : nullptr;
		return OutPC ? Cast<ACastleCharacter>(OutPC->GetPawn()) : nullptr;
	}

	/** What the grapple shots measure between commands. */
	struct FGrappleRun
	{
		TWeakObjectPtr<AGrappleAnchor> Anchor;
		double ZipStartTime = -1.0;
		FVector ZipStartLocation = FVector::ZeroVector;
		float ZipLength = 0.f;
	};
	static FGrappleRun GrappleRun;
}

/** Averages FApp::GetDeltaTime over Frames frames (after a few to settle) and logs it in ms. */
class FCastleKateFrameTime : public IAutomationLatentCommand
{
public:
	FCastleKateFrameTime(FAutomationTestBase* InTest, const FString& InLabel, int32 InFrames)
		: Test(InTest), Label(InLabel), Frames(InFrames)
	{
	}

	virtual bool Update() override
	{
		if (SettleFrames > 0)
		{
			--SettleFrames;
			return false;
		}
		const double Delta = FApp::GetDeltaTime();
		Sum += Delta;
		Worst = FMath::Max(Worst, Delta);
		if (++Count < Frames)
		{
			return false;
		}
		const double AverageMs = Sum / Count * 1000.0;
		Test->AddInfo(FString::Printf(
			TEXT("Frame time on the %s: %.2f ms average over %d frames (%.1f fps), worst %.2f ms"),
			*Label, AverageMs, Count, 1000.0 / AverageMs, Worst * 1000.0));
		UE_LOG(LogTemp, Display, TEXT("CastleFrameTime %s avg_ms=%.2f worst_ms=%.2f frames=%d"),
			*Label, AverageMs, Worst * 1000.0, Count);
		return true;
	}

private:
	FAutomationTestBase* Test;
	FString Label;
	int32 Frames;
	int32 SettleFrames = 5;
	int32 Count = 0;
	double Sum = 0.0;
	double Worst = 0.0;
};

/**
 * Stands Kate on the street and turns the camera up at the tenement anchors across from the
 * park, nearest the middle of the grapple's range first, until the grapple targets one.
 */
class FCastleKateAimAtAnchor : public IAutomationLatentCommand
{
public:
	explicit FCastleKateAimAtAnchor(FAutomationTestBase* InTest) : Test(InTest) {}

	virtual bool Update() override
	{
		using namespace CastleKateShots;
		APlayerController* PC = nullptr;
		ACastleCharacter* Kate = FindKate(PC);
		UGrappleComponent* Grapple = Kate ? Kate->GetGrappleComponent() : nullptr;
		UWorld* World = FindWorld();
		if (!Grapple || !World)
		{
			Test->AddError(TEXT("No Kate with a grapple component."));
			return true;
		}
		if (!bListed)
		{
			ListCandidates(World, Kate);
			bListed = true;
		}
		if (Index >= 0 && World->GetTimeSeconds() < AimedAt + 1.0)
		{
			return false;
		}
		if (Index >= 0 && Grapple->GetTargetAnchor())
		{
			Report(Kate, Grapple);
			return true;
		}
		if (Index >= 0)
		{
			Diagnose(World, Kate, Candidates[Index].Get());
		}
		if (++Index >= Candidates.Num() || Index >= 20)
		{
			Test->AddWarning(TEXT("No tenement anchor became a grapple target from the street."));
			return true;
		}
		if (const AGrappleAnchor* Anchor = Candidates[Index].Get())
		{
			const float Yaw = (Anchor->GetMarkerLocation() - Stands[Index]).Rotation().Yaw;
			CastleKateShots::PlaceKate(Kate, PC, Stands[Index], Yaw, 0.f);
			Aim(PC, Kate, Anchor);
		}
		AimedAt = World->GetTimeSeconds();
		return false;
	}

private:
	/**
	 * Anchors near the street spot, each paired with a place on the street out from its facade
	 * as far as the grapple's range allows (at most 17 m), so the camera looks up as shallowly
	 * as it can.
	 */
	void ListCandidates(UWorld* World, const ACastleCharacter* Kate)
	{
		static constexpr float MaxStandBack = 1700.f;
		static constexpr float Reach = 2350.f;
		FVector Spot, Away;
		CastleKateShots::FindStreetSpot(World, Spot, Away);
		FVector Street;
		const float StreetZ = CastleKateShots::FindGround(World, Spot, 3000.f, Kate, Street) ? Street.Z : 0.f;
		TArray<TPair<float, int32>> Order;
		for (TActorIterator<AGrappleAnchor> It(World); It; ++It)
		{
			const FVector Marker = It->GetMarkerLocation();
			const float Height = Marker.Z - StreetZ;
			if (FVector::Dist2D(Marker, Spot) > 15000.f || Height < 800.f || Height > 2250.f)
			{
				continue;
			}
			const float StandBack = FMath::Min(MaxStandBack, FMath::Sqrt(Reach * Reach - Height * Height));
			const FVector Outward = -It->GetActorForwardVector().GetSafeNormal2D();
			const FVector StandXY = Marker + Outward * StandBack;
			FHitResult Ground;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(KateShotStand), false, Kate);
			if (!World->LineTraceSingleByChannel(Ground, FVector(StandXY.X, StandXY.Y, StreetZ + 300.f),
					FVector(StandXY.X, StandXY.Y, StreetZ - 300.f), ECC_Visibility, Params)
				|| !Ground.GetActor() || Ground.GetActor()->Tags.Contains(CastleKateShots::BuildingTag))
			{
				continue;
			}
			Order.Emplace(FVector::Dist2D(Marker, Spot), Candidates.Num());
			Candidates.Add(*It);
			Stands.Add(Ground.ImpactPoint);
		}
		Order.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; });
		TArray<TWeakObjectPtr<AGrappleAnchor>> SortedAnchors;
		TArray<FVector> SortedStands;
		for (const TPair<float, int32>& Pair : Order)
		{
			SortedAnchors.Add(Candidates[Pair.Value]);
			SortedStands.Add(Stands[Pair.Value]);
		}
		Candidates = MoveTemp(SortedAnchors);
		Stands = MoveTemp(SortedStands);
		Test->AddInfo(FString::Printf(TEXT("%d tenement anchors with a street spot out in front of their facade."),
			Candidates.Num()));
	}

	/** Points the control rotation so the camera, which hangs behind and right of Kate, faces Anchor. */
	static void Aim(APlayerController* PC, ACastleCharacter* Kate, const AGrappleAnchor* Anchor)
	{
		if (!Anchor)
		{
			return;
		}
		const FVector Target = Anchor->GetMarkerLocation();
		FRotator Rotation = (Target - Kate->GetActorLocation()).Rotation();
		for (int32 Pass = 0; Pass < 4; ++Pass)
		{
			const FVector Lens = Kate->GetActorLocation() + Rotation.RotateVector(FVector(-350.f, 70.f, 60.f));
			Rotation = (Target - Lens).Rotation();
		}
		// Looking up from the street pulls the lens in against the pavement behind her, so her body
		// fills the middle of the frame. Aim a little left of and below the anchor (well inside the
		// 30 degree cone) so the marker shows up and to the right, clear of her.
		Kate->SetActorRotation(FRotator(0.f, Rotation.Yaw, 0.f));
		PC->SetControlRotation(FRotator(Rotation.Pitch - 8.f, Rotation.Yaw - 15.f, 0.f));
	}

	/** Why a candidate was not picked: its angle from the camera, its range, and what the sight line hits. */
	void Diagnose(UWorld* World, const ACastleCharacter* Kate, const AGrappleAnchor* Anchor)
	{
		if (!Anchor)
		{
			return;
		}
		const UCameraComponent* Camera = Kate->GetFollowCamera();
		const FVector Lens = Camera->GetComponentLocation();
		const FVector Marker = Anchor->GetMarkerLocation();
		const float Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
			FVector::DotProduct(Camera->GetForwardVector(), (Marker - Lens).GetSafeNormal()), -1.f, 1.f)));
		FCollisionQueryParams Params(SCENE_QUERY_STAT(KateShotSight), false, Kate);
		Params.AddIgnoredActor(Anchor);
		FHitResult Hit;
		const bool bBlocked = World->LineTraceSingleByChannel(Hit, Lens, Marker, ECC_Visibility, Params);
		Test->AddInfo(FString::Printf(
			TEXT("Candidate %s not targeted: %.1f deg off the camera, %.0f cm from Kate, sight %s %s at %.0f of %.0f cm"),
			*Anchor->GetActorLabel(), Angle, FVector::Dist(Kate->GetActorLocation(), Marker),
			bBlocked ? TEXT("hits") : TEXT("clear"), *GetNameSafe(Hit.GetActor()), Hit.Distance,
			FVector::Dist(Lens, Marker)));
	}

	void Report(ACastleCharacter* Kate, UGrappleComponent* Grapple)
	{
		AGrappleAnchor* Target = Grapple->GetTargetAnchor();
		CastleKateShots::GrappleRun.Anchor = Target;
		const UCameraComponent* Camera = Kate->GetFollowCamera();
		const FVector ToAnchor = (Target->GetMarkerLocation() - Camera->GetComponentLocation()).GetSafeNormal();
		const float Angle = FMath::RadiansToDegrees(FMath::Acos(
			FMath::Clamp(FVector::DotProduct(Camera->GetForwardVector(), ToAnchor), -1.f, 1.f)));
		Test->AddInfo(FString::Printf(
			TEXT("Grapple target %s (%s): %.0f cm from Kate, %.0f cm above her, %.1f deg off the camera"),
			*GetNameSafe(Target), *Target->GetActorLabel(),
			FVector::Dist(Kate->GetActorLocation(), Target->GetMarkerLocation()),
			Target->GetMarkerLocation().Z - Kate->GetActorLocation().Z, Angle));
	}

	FAutomationTestBase* Test;
	TArray<TWeakObjectPtr<AGrappleAnchor>> Candidates;
	TArray<FVector> Stands;
	bool bListed = false;
	int32 Index = -1;
	double AimedAt = 0.0;
};

/** Where the HUD drew the marker against where the anchor projects, both in viewport pixels. */
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FCastleKateReportMarker, FAutomationTestBase*, Test);

bool FCastleKateReportMarker::Update()
{
	using namespace CastleKateShots;
	APlayerController* PC = nullptr;
	ACastleCharacter* Kate = FindKate(PC);
	const ACastlePlayerController* CastlePC = Cast<ACastlePlayerController>(PC);
	const UCastleHudWidget* Hud = CastlePC ? CastlePC->GetCastleHud() : nullptr;
	const UGrappleComponent* Grapple = Kate ? Kate->GetGrappleComponent() : nullptr;
	const AGrappleAnchor* Target = Grapple ? Grapple->GetTargetAnchor() : nullptr;
	if (!Hud || !Target)
	{
		Test->AddWarning(TEXT("No HUD or no grapple target for the marker shot."));
		return true;
	}
	FVector2D AnchorPixel;
	PC->ProjectWorldLocationToScreen(Target->GetMarkerLocation(), AnchorPixel, false);
	const FVector2D MarkerPixel = Hud->GetGrappleMarkerPosition() * UWidgetLayoutLibrary::GetViewportScale(PC);
	Test->AddInfo(FString::Printf(
		TEXT("Marker visible=%d at (%.0f, %.0f) px, anchor projects to (%.0f, %.0f) px, hint=%d"),
		Hud->IsGrappleMarkerVisible() ? 1 : 0, MarkerPixel.X, MarkerPixel.Y, AnchorPixel.X, AnchorPixel.Y,
		Hud->IsGrappleHintVisible() ? 1 : 0));
	if (!Hud->IsGrappleMarkerVisible() || FVector2D::Distance(MarkerPixel, AnchorPixel) > 3.f)
	{
		Test->AddWarning(TEXT("The grapple marker is not on the anchor."));
	}
	return true;
}

/** Presses Q. */
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FCastleKateFireGrapple, FAutomationTestBase*, Test);

bool FCastleKateFireGrapple::Update()
{
	APlayerController* PC = nullptr;
	ACastleCharacter* Kate = CastleKateShots::FindKate(PC);
	UGrappleComponent* Grapple = Kate ? Kate->GetGrappleComponent() : nullptr;
	CastleKateShots::GrappleRun.ZipStartTime = -1.0;
	const bool bFired = Grapple && Grapple->TryFire();
	Test->AddInfo(FString::Printf(TEXT("Grapple fired=%d, arrows left %d"), bFired ? 1 : 0,
		Grapple ? Grapple->GetGrappleArrows() : -1));
	if (!bFired)
	{
		Test->AddWarning(TEXT("The grapple did not fire."));
	}
	return true;
}

/**
 * Waits (up to TimeoutSeconds) for the zip to pass Progress, noting when it started. With
 * Progress above 1 it waits for the landing instead and reports the zip's numbers.
 */
class FCastleKateWaitZip : public IAutomationLatentCommand
{
public:
	FCastleKateWaitZip(FAutomationTestBase* InTest, float InProgress, float InTimeoutSeconds)
		: Test(InTest), Progress(InProgress), TimeoutSeconds(InTimeoutSeconds)
	{
	}

	virtual bool Update() override;

private:
	FAutomationTestBase* Test;
	float Progress;
	float TimeoutSeconds;
	double StartTime = -1.0;
};

bool FCastleKateWaitZip::Update()
{
	using namespace CastleKateShots;
	APlayerController* PC = nullptr;
	ACastleCharacter* Kate = FindKate(PC);
	UGrappleComponent* Grapple = Kate ? Kate->GetGrappleComponent() : nullptr;
	UWorld* World = FindWorld();
	if (!Grapple || !World)
	{
		return true;
	}
	const double Now = World->GetTimeSeconds();
	FGrappleRun& Run = GrappleRun;
	if (Grapple->IsZipping() && Run.ZipStartTime < 0.0)
	{
		// The first frame seen zipping has already moved one step; back the start out of it.
		Run.ZipStartTime = Now - Grapple->GetZipProgress() * Grapple->GetZipLength() / Grapple->ZipSpeed;
		Run.ZipLength = Grapple->GetZipLength();
		Run.ZipStartLocation = Kate->GetActorLocation();
	}
	if (StartTime < 0.0)
	{
		StartTime = Now;
	}
	const bool bTimedOut = Now - StartTime > TimeoutSeconds;

	if (Progress <= 1.f)
	{
		if (Grapple->IsZipping() && Grapple->GetZipProgress() >= Progress)
		{
			Test->AddInfo(FString::Printf(TEXT("Mid-zip: %.0f%% along, at %s, %.2f s in"),
				Grapple->GetZipProgress() * 100.f, *Kate->GetActorLocation().ToCompactString(),
				Now - Run.ZipStartTime));
			return true;
		}
		if (bTimedOut)
		{
			Test->AddWarning(FString::Printf(TEXT("The zip never reached %.0f%%."), Progress * 100.f));
		}
		return bTimedOut;
	}

	if (Grapple->IsZipping() || Grapple->IsArrowInFlight())
	{
		if (bTimedOut)
		{
			Test->AddWarning(TEXT("Kate never landed from the zip."));
		}
		return bTimedOut;
	}
	const double Duration = Now - Run.ZipStartTime;
	const AGrappleAnchor* Anchor = Run.Anchor.Get();
	const float Feet = Kate->GetActorLocation().Z - Kate->GetSimpleCollisionHalfHeight();
	FHitResult Floor;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(KateShotRoof), false, Kate);
	World->LineTraceSingleByChannel(Floor, Kate->GetActorLocation(),
		Kate->GetActorLocation() - FVector(0.f, 0.f, 500.f), ECC_Visibility, Params);
	Test->AddInfo(FString::Printf(
		TEXT("Zip: %.0f cm in %.2f s = %.0f cm/s; landed at %s, feet %.1f cm above the landing point, %.1f cm above %s, walking=%d"),
		Run.ZipLength, Duration, Duration > 0.0 ? Run.ZipLength / Duration : 0.0,
		*Kate->GetActorLocation().ToCompactString(),
		Anchor ? Feet - Anchor->GetLandingLocation().Z : -1.f, Floor.bBlockingHit ? Feet - Floor.ImpactPoint.Z : -1.f,
		*GetNameSafe(Floor.GetActor()), Kate->GetCharacterMovement()->IsMovingOnGround() ? 1 : 0));
	return true;
}

/** After landing: level the camera to look across the roof the way she is facing. */
DEFINE_LATENT_AUTOMATION_COMMAND(FCastleKateLookAcrossRoof);

bool FCastleKateLookAcrossRoof::Update()
{
	APlayerController* PC = nullptr;
	if (ACastleCharacter* Kate = CastleKateShots::FindKate(PC))
	{
		PC->SetControlRotation(FRotator(-15.f, Kate->GetActorRotation().Yaw, 0.f));
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

// --- Parkour and the look-up camera -------------------------------------------------------------

namespace CastleKateShots
{
	static const TCHAR* SprintActionPath = TEXT("/Game/Input/IA_Sprint.IA_Sprint");

	enum class EParkourShot : uint8
	{
		Lookup,
		VaultRun,
		MantleStand,
		MantleJump,
		LedgeFall,
		Climb,
		EndInput,
	};

	/** Holds or releases IA_Sprint through Enhanced Input injection, like HoldMove. */
	static void HoldSprint(APlayerController* PC, bool bHold)
	{
		const UInputAction* Sprint = LoadObject<UInputAction>(nullptr, SprintActionPath);
		ULocalPlayer* Player = PC ? PC->GetLocalPlayer() : nullptr;
		UEnhancedInputLocalPlayerSubsystem* Input = Player
			? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Player) : nullptr;
		if (!Sprint || !Input)
		{
			return;
		}
		if (bHold)
		{
			Input->StartContinuousInputInjectionForAction(Sprint, FInputActionValue(true), {}, {});
		}
		else
		{
			Input->StopContinuousInputInjectionForAction(Sprint);
		}
	}

	static AActor* FindTagged(UWorld* World, FName Tag)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->Tags.Contains(Tag))
			{
				return *It;
			}
		}
		return nullptr;
	}

	/** The IA_Move value that walks along WorldDirection with the camera at CameraYaw. */
	static FVector2D MoveTowards(const FVector& WorldDirection, float CameraYaw)
	{
		const float Angle = FMath::DegreesToRadians(WorldDirection.Rotation().Yaw - CameraYaw);
		return FVector2D(FMath::Sin(Angle), FMath::Cos(Angle));
	}

	/**
	 * Stands Kate Back cm in front of a test block's near face (the block's local -Y side, its depth
	 * axis), facing it, with the camera Side degrees round from behind her.
	 */
	static bool FaceTestBlock(UWorld* World, ACastleCharacter* Kate, APlayerController* PC, FName Tag, float Back,
		float Side, FVector& OutDirection)
	{
		const AActor* Block = FindTagged(World, Tag);
		FVector Ground;
		if (!Block)
		{
			return false;
		}
		const FVector Axis = Block->GetActorRightVector().GetSafeNormal2D();
		const FVector Near = Block->GetActorLocation() + Block->GetActorForwardVector() * Block->GetActorScale3D().X * 50.f;
		const FVector Stand = Near - Axis * Back;
		if (!FindGround(World, Stand, Near.Z + 500.f, Kate, Ground))
		{
			return false;
		}
		OutDirection = Axis;
		PlaceKate(Kate, PC, Ground, Axis.Rotation().Yaw, -10.f);
		PC->SetControlRotation(FRotator(-10.f, Axis.Rotation().Yaw + Side, 0.f));
		return true;
	}

	/** A tenement face out from the street spot, with the top of its parapet. */
	static bool FindTenementLedge(UWorld* World, const ACastleCharacter* Kate, FVector& OutFace, FVector& OutNormal,
		float& OutTopZ)
	{
		FVector Spot, Away, Street;
		if (!FindStreetSpot(World, Spot, Away) || !FindGround(World, Spot, 3000.f, Kate, Street))
		{
			return false;
		}
		FCollisionQueryParams Params(SCENE_QUERY_STAT(KateShotLedge), false, Kate);
		FHitResult Hit;
		const FVector Chest = Street + FVector(0.f, 0.f, 120.f);
		if (!World->LineTraceSingleByChannel(Hit, Chest, Chest + Away * 5000.f, ECC_Visibility, Params)
			|| !Hit.GetActor() || !Hit.GetActor()->Tags.Contains(BuildingTag))
		{
			return false;
		}
		OutFace = Hit.ImpactPoint;
		OutNormal = Hit.ImpactNormal.GetSafeNormal2D();
		FHitResult Top;
		const FVector Inside = OutFace - OutNormal * 10.f;
		if (!World->LineTraceSingleByChannel(Top, Inside + FVector(0.f, 0.f, 6000.f), Inside, ECC_Visibility, Params))
		{
			return false;
		}
		OutTopZ = Top.ImpactPoint.Z;
		return true;
	}

	/** An anchor on a facade seen from the street at StandBack cm out, and where to stand. */
	static bool FindLookupSpot(UWorld* World, const ACastleCharacter* Kate, FVector& OutStand, FVector& OutTarget)
	{
		static constexpr float StandBack = 1300.f;
		FVector Spot, Away, Street;
		if (!FindStreetSpot(World, Spot, Away) || !FindGround(World, Spot, 3000.f, Kate, Street))
		{
			return false;
		}
		float Best = BIG_NUMBER;
		for (TActorIterator<AGrappleAnchor> It(World); It; ++It)
		{
			const FVector Marker = It->GetMarkerLocation();
			const float Height = Marker.Z - Street.Z;
			const float Distance = FVector::Dist2D(Marker, Spot);
			if (Height < 1500.f || Height > 2200.f || Distance > 12000.f || Distance > Best)
			{
				continue;
			}
			const FVector StandXY = Marker - It->GetActorForwardVector().GetSafeNormal2D() * StandBack;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(KateShotLookup), false, Kate);
			FHitResult Ground;
			if (!World->LineTraceSingleByChannel(Ground, FVector(StandXY.X, StandXY.Y, Street.Z + 300.f),
					FVector(StandXY.X, StandXY.Y, Street.Z - 300.f), ECC_Visibility, Params)
				|| !Ground.GetActor() || Ground.GetActor()->Tags.Contains(BuildingTag))
			{
				continue;
			}
			Best = Distance;
			OutStand = Ground.ImpactPoint;
			OutTarget = Marker;
		}
		return Best < BIG_NUMBER;
	}

	/** Kate's projected screen box, in viewport pixels; false when she is off screen. */
	static bool KateScreenBox(APlayerController* PC, const ACastleCharacter* Kate, FBox2D& OutBox)
	{
		const float Radius = Kate->GetCapsuleComponent()->GetScaledCapsuleRadius();
		const float Half = Kate->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		OutBox = FBox2D(ForceInit);
		for (int32 Corner = 0; Corner < 8; ++Corner)
		{
			const FVector Offset((Corner & 1) ? Radius : -Radius, (Corner & 2) ? Radius : -Radius, (Corner & 4) ? Half : -Half);
			FVector2D Pixel;
			if (PC->ProjectWorldLocationToScreen(Kate->GetActorLocation() + Offset, Pixel, false))
			{
				OutBox += Pixel;
			}
		}
		return OutBox.bIsValid;
	}
}

/** Sets up one parkour or look-up shot. */
DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FCastleKateParkourShot, FAutomationTestBase*, Test, uint8, Shot);

bool FCastleKateParkourShot::Update()
{
	using namespace CastleKateShots;
	UWorld* World = FindWorld();
	APlayerController* PC = nullptr;
	ACastleCharacter* Kate = FindKate(PC);
	if (!Kate || !World)
	{
		Test->AddError(TEXT("No Kate for the parkour shots."));
		return true;
	}
	FVector Direction;
	switch (static_cast<EParkourShot>(Shot))
	{
	case EParkourShot::Lookup:
	{
		FVector Stand, Target;
		if (!FindLookupSpot(World, Kate, Stand, Target))
		{
			Test->AddWarning(TEXT("No rooftop anchor with a street spot 13 m out; kate_lookup.png shows the street."));
			break;
		}
		const FVector Pivot = Stand + FVector(0.f, 0.f, Kate->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		const FRotator Look = (Target - Pivot).Rotation();
		PlaceKate(Kate, PC, Stand, Look.Yaw, 0.f);
		PC->SetControlRotation(FRotator(Kate->ClampCameraPitch(Look.Pitch), Look.Yaw, 0.f));
		Test->AddInfo(FString::Printf(TEXT("Lookup: anchor %.0f cm up, %.0f cm out, pitch %.1f deg"),
			Target.Z - Stand.Z, FVector::Dist2D(Target, Stand), Look.Pitch));
		break;
	}
	case EParkourShot::VaultRun:
		if (FaceTestBlock(World, Kate, PC, TEXT("CityTestVault"), 600.f, -70.f, Direction))
		{
			HoldSprint(PC, true);
			HoldMove(PC, MoveTowards(Direction, PC->GetControlRotation().Yaw), true);
		}
		else
		{
			Test->AddWarning(TEXT("No City_Test_Vault block."));
		}
		break;
	case EParkourShot::MantleStand:
		if (!FaceTestBlock(World, Kate, PC, TEXT("CityTestMantle"), 70.f, -75.f, Direction))
		{
			Test->AddWarning(TEXT("No City_Test_Mantle block."));
		}
		break;
	case EParkourShot::MantleJump:
		Kate->Jump();
		break;
	case EParkourShot::LedgeFall:
	{
		FVector Face, Normal;
		float TopZ = 0.f;
		if (!FindTenementLedge(World, Kate, Face, Normal, TopZ))
		{
			Test->AddWarning(TEXT("No tenement face across the street for the ledge shot."));
			break;
		}
		// In the air against the wall, feet 230 cm under the parapet top (a fire escape landing's
		// height): falling, she should catch the edge.
		const float Half = Kate->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const FVector Air(Face.X + Normal.X * 45.f, Face.Y + Normal.Y * 45.f, TopZ - 230.f + Half);
		Kate->StopAim();
		Kate->TeleportTo(Air, FRotator(0.f, (-Normal).Rotation().Yaw, 0.f), false, true);
		Kate->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		Kate->GetCharacterMovement()->Velocity = FVector(0.f, 0.f, -10.f);
		PC->SetControlRotation(FRotator(12.f, (-Normal).Rotation().Yaw + 35.f, 0.f));
		Test->AddInfo(FString::Printf(TEXT("Ledge: parapet top %.0f cm, Kate's feet at %.0f, face at %s"),
			TopZ, Air.Z - Half, *Face.ToCompactString()));
		break;
	}
	case EParkourShot::Climb:
		Kate->Jump();
		break;
	case EParkourShot::EndInput:
		HoldSprint(PC, false);
		HoldMove(PC, FVector2D::ZeroVector, false);
		break;
	}
	return true;
}

/** Waits (up to TimeoutSeconds) for a traversal move to start, or with bHang for a hang. */
class FCastleKateWaitTraversal : public IAutomationLatentCommand
{
public:
	FCastleKateWaitTraversal(FAutomationTestBase* InTest, float InTimeoutSeconds, bool bInHang)
		: Test(InTest), TimeoutSeconds(InTimeoutSeconds), bHang(bInHang)
	{
	}

	virtual bool Update() override
	{
		APlayerController* PC = nullptr;
		ACastleCharacter* Kate = CastleKateShots::FindKate(PC);
		const UParkourComponent* Parkour = Kate ? Kate->GetParkourComponent() : nullptr;
		UWorld* World = CastleKateShots::FindWorld();
		if (!Parkour || !World)
		{
			return true;
		}
		if (StartTime < 0.0)
		{
			StartTime = World->GetTimeSeconds();
		}
		const bool bReady = bHang ? Parkour->IsHanging() : Kate->IsTraversing();
		if (bReady)
		{
			const FCastleParkourObstacle Obstacle = Parkour->GetLastObstacle();
			Test->AddInfo(FString::Printf(
				TEXT("Traversal after %.2f s: %s by %s; obstacle %.0f cm high, %.0f cm away, depth %.0f, on %s"),
				World->GetTimeSeconds() - StartTime, *UEnum::GetValueAsString(Parkour->GetLastMove()),
				*UEnum::GetValueAsString(Parkour->GetLastRoute()), Obstacle.Height, Obstacle.Distance, Obstacle.Depth,
				*GetNameSafe(Obstacle.Actor)));
			return true;
		}
		if (World->GetTimeSeconds() - StartTime > TimeoutSeconds)
		{
			Test->AddWarning(FString::Printf(TEXT("No %s within %.1f s; Kate at %s, %s"), bHang ? TEXT("hang") : TEXT("traversal"),
				TimeoutSeconds, *Kate->GetActorLocation().ToCompactString(), *Kate->GetMovementDebugText()));
			return true;
		}
		return false;
	}

private:
	FAutomationTestBase* Test;
	float TimeoutSeconds;
	bool bHang;
	double StartTime = -1.0;
};

/** Where the hands and capsule are against the obstacle, and whether the capsule is inside anything. */
DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FCastleKateReportParkour, FAutomationTestBase*, Test, FString, Label);

bool FCastleKateReportParkour::Update()
{
	UWorld* World = CastleKateShots::FindWorld();
	APlayerController* PC = nullptr;
	ACastleCharacter* Kate = CastleKateShots::FindKate(PC);
	const UParkourComponent* Parkour = Kate ? Kate->GetParkourComponent() : nullptr;
	if (!Parkour || !World)
	{
		return true;
	}
	const FCastleParkourObstacle Obstacle = Parkour->GetLastObstacle();
	const USkeletalMeshComponent* Body = Kate->GetMesh();
	const FVector HandL = Body ? Body->GetSocketLocation(TEXT("hand_l")) : FVector::ZeroVector;
	const FVector HandR = Body ? Body->GetSocketLocation(TEXT("hand_r")) : FVector::ZeroVector;
	const UCapsuleComponent* Capsule = Kate->GetCapsuleComponent();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(KateShotClip), false, Kate);
	const bool bClipping = World->OverlapBlockingTestByChannel(Kate->GetActorLocation(), FQuat::Identity, ECC_Pawn,
		FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius() - 4.f, Capsule->GetScaledCapsuleHalfHeight() - 4.f),
		Params);
	const float Feet = Kate->GetActorLocation().Z - Capsule->GetScaledCapsuleHalfHeight();
	Test->AddInfo(FString::Printf(
		TEXT("%s: move %s (%s) hanging=%d sample=%d; feet %.0f cm vs ledge top %.0f; hands %.0f / %.0f cm above the top, "
			 "%.0f / %.0f cm out from the face; capsule %.0f cm out; in geometry=%d"),
		*Label, *UEnum::GetValueAsString(Parkour->GetActiveMove()), *UEnum::GetValueAsString(Parkour->GetLastRoute()),
		Parkour->IsHanging() ? 1 : 0, Parkour->IsSampleTraversalActive() ? 1 : 0, Feet, Obstacle.LedgePoint.Z,
		HandL.Z - Obstacle.LedgePoint.Z, HandR.Z - Obstacle.LedgePoint.Z,
		FVector::DotProduct(HandL - Obstacle.WallPoint, Obstacle.WallNormal),
		FVector::DotProduct(HandR - Obstacle.WallPoint, Obstacle.WallNormal),
		FVector::DotProduct(Kate->GetActorLocation() - Obstacle.WallPoint, Obstacle.WallNormal), bClipping ? 1 : 0));
	if (bClipping)
	{
		Test->AddWarning(FString::Printf(TEXT("%s: Kate's capsule is inside geometry."), *Label));
	}
	return true;
}

/** Where the look-up camera ended up: lens height over the ground, arm, and whether Kate covers the centre. */
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FCastleKateReportLookup, FAutomationTestBase*, Test);

bool FCastleKateReportLookup::Update()
{
	UWorld* World = CastleKateShots::FindWorld();
	APlayerController* PC = nullptr;
	ACastleCharacter* Kate = CastleKateShots::FindKate(PC);
	if (!Kate || !World || !PC)
	{
		return true;
	}
	const FVector Lens = Kate->GetFollowCamera()->GetComponentLocation();
	FVector Ground;
	const bool bGround = CastleKateShots::FindGround(World, Lens + FVector(0.f, 0.f, 50.f), Lens.Z + 50.f, Kate, Ground);
	int32 SizeX = 0;
	int32 SizeY = 0;
	PC->GetViewportSize(SizeX, SizeY);
	FBox2D Box;
	const bool bOnScreen = CastleKateShots::KateScreenBox(PC, Kate, Box);
	const FVector2D Centre(SizeX * 0.5f, SizeY * 0.5f);
	const bool bCoversCentre = bOnScreen && Box.IsInside(Centre);
	const float Coverage = bOnScreen && SizeX > 0 && SizeY > 0
		? (FMath::Min<float>(Box.Max.X, SizeX) - FMath::Max<float>(Box.Min.X, 0.f))
			* (FMath::Min<float>(Box.Max.Y, SizeY) - FMath::Max<float>(Box.Min.Y, 0.f)) / (SizeX * SizeY)
		: 0.f;
	Test->AddInfo(FString::Printf(
		TEXT("kate_lookup: pitch %.1f, arm %.0f, socket Z %.0f, lens %.0f cm above the ground, %.0f cm from the pivot; "
			 "Kate's box (%.0f,%.0f)-(%.0f,%.0f) of %dx%d, covers centre=%d, %.0f%% of the frame"),
		FRotator::NormalizeAxis(PC->GetControlRotation().Pitch), Kate->GetCameraBoom()->TargetArmLength,
		Kate->GetCameraBoom()->SocketOffset.Z, bGround ? Lens.Z - Ground.Z : -1.f,
		FVector::Dist(Lens, Kate->GetCameraBoom()->GetComponentLocation()), Box.Min.X, Box.Min.Y, Box.Max.X, Box.Max.Y,
		SizeX, SizeY, bCoversCentre ? 1 : 0, Coverage * 100.f));
	if (bCoversCentre)
	{
		Test->AddWarning(TEXT("kate_lookup: Kate covers the centre of the frame."));
	}
	if (bGround && Lens.Z - Ground.Z < 20.f)
	{
		Test->AddWarning(TEXT("kate_lookup: the lens is down on the pavement."));
	}
	return true;
}


// --- The bow ------------------------------------------------------------------------------------

namespace CastleKateShots
{
	static const TCHAR* ThugClassPath = TEXT("/Game/Blueprints/AI/BP_Thug.BP_Thug_C");
	static const TCHAR* BowAssetPath = TEXT("/Game/Blueprints/Weapons/DA_Bow_Kate.DA_Bow_Kate");
	static const TCHAR* StandardAssetPath = TEXT("/Game/Blueprints/Weapons/DA_Arrow_Standard.DA_Arrow_Standard");
	static const TCHAR* GrappleAssetPath = TEXT("/Game/Blueprints/Weapons/DA_Arrow_Grapple.DA_Arrow_Grapple");
	static constexpr float ThugDistance = 1500.f;

	enum class EBowShot : uint8
	{
		Holster,
		DrawStart,
		DrawCancel,
		HitSetup,
		HitAim,
		HitDraw,
		HitRelease,
		Cleanup,
		RoofThugs,
	};

	/** The thug the hit shot aims at. */
	static TWeakObjectPtr<AThugCharacter> TargetThug;

	static AThugCharacter* SpawnThug(UWorld* World, const FVector& Ground, float Yaw, bool bWithBrain)
	{
		UClass* ThugClass = LoadClass<AThugCharacter>(nullptr, ThugClassPath);
		if (!ThugClass)
		{
			ThugClass = AThugCharacter::StaticClass();
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		const float HalfHeight = GetDefault<AThugCharacter>()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		AThugCharacter* Thug = World->SpawnActor<AThugCharacter>(ThugClass,
			Ground + FVector(0.f, 0.f, HalfHeight + 2.f), FRotator(0.f, Yaw, 0.f), Params);
		if (Thug && !bWithBrain)
		{
			// A target, not a fight: without its brain it stands still and never shoots back.
			if (AController* Brain = Thug->GetController())
			{
				Brain->UnPossess();
				Brain->Destroy();
			}
		}
		return Thug;
	}

	/** Makes sure Kate carries the chapter's quiver; warns when the mission did not grant it. */
	static void EnsureQuiver(FAutomationTestBase* Test, UInventoryComponent* Inventory)
	{
		if (Inventory->HasBow() && Inventory->GetArrowCount(1) > 0)
		{
			return;
		}
		Test->AddWarning(TEXT("Kate had no bow or arrows on the district; DA_CH01_Rooftops should grant them."));
		UBowDefinition* Bow = LoadObject<UBowDefinition>(nullptr, BowAssetPath);
		FCastleQuiverSlot Standard;
		Standard.Arrow = LoadObject<UArrowDefinition>(nullptr, StandardAssetPath);
		Standard.Count = 30;
		FCastleQuiverSlot Grapple;
		Grapple.Arrow = LoadObject<UArrowDefinition>(nullptr, GrappleAssetPath);
		Grapple.Count = 6;
		Inventory->ApplyStartingQuiver(Bow, { Standard, Grapple });
	}

	/** Where the bow sits against the bones it hangs off, for tuning the holster and the grip. */
	static void ReportBow(FAutomationTestBase* Test, ACastleCharacter* Kate, const TCHAR* Label)
	{
		const UBowComponent* Bow = Kate->GetBowComponent();
		const UStaticMeshComponent* Mesh = Bow ? Bow->GetBowMeshComponent() : nullptr;
		const UInventoryComponent* Inventory = Kate->GetInventoryComponent();
		if (!Mesh || !Inventory)
		{
			Test->AddWarning(FString::Printf(TEXT("%s: no bow mesh on Kate."), Label));
			return;
		}
		const FTransform Actor = Kate->GetActorTransform();
		const FVector Spine = Kate->GetMesh()->GetSocketLocation(Bow->HolsterBone);
		const FVector Hand = Kate->GetMesh()->GetSocketLocation(Inventory->GetBow()->HandSocket);
		Test->AddInfo(FString::Printf(
			TEXT("%s: bow %s mesh %s at %s (actor frame %s), rot %s; spine_03 %s, hand %s (actor frame); draw %.2f, spread %.2f; quiver %d / %d, slot %d"),
			Label, *GetNameSafe(Inventory->GetBow()), *GetNameSafe(Mesh->GetStaticMesh()),
			*Mesh->GetComponentLocation().ToCompactString(),
			*Actor.InverseTransformPosition(Mesh->GetComponentLocation()).ToCompactString(),
			*Mesh->GetComponentRotation().ToCompactString(),
			*Actor.InverseTransformPosition(Spine).ToCompactString(),
			*Actor.InverseTransformPosition(Hand).ToCompactString(),
			Bow->GetDrawFraction(), Bow->GetCurrentSpreadDegrees(),
			Inventory->GetArrowCount(1), Inventory->GetArrowCount(2), Inventory->GetActiveArrowSlot()));
	}

	/** Points the camera from where it is now at Target. The arm follows, so do it twice. */
	static void AimCameraAt(APlayerController* PC, ACastleCharacter* Kate, const FVector& Target)
	{
		const FVector Lens = Kate->GetFollowCamera()->GetComponentLocation();
		const FRotator Rotation = (Target - Lens).Rotation();
		PC->SetControlRotation(FRotator(Rotation.Pitch, Rotation.Yaw, 0.f));
		Kate->SetActorRotation(FRotator(0.f, Rotation.Yaw, 0.f));
	}
}

DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FCastleKateBowShot, FAutomationTestBase*, Test, uint8, Shot);

bool FCastleKateBowShot::Update()
{
	using namespace CastleKateShots;
	UWorld* World = FindWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	ACastleCharacter* Kate = PC ? Cast<ACastleCharacter>(PC->GetPawn()) : nullptr;
	UBowComponent* Bow = Kate ? Kate->GetBowComponent() : nullptr;
	UInventoryComponent* Inventory = Kate ? Kate->GetInventoryComponent() : nullptr;
	if (!Bow || !Inventory)
	{
		Test->AddError(TEXT("No Kate with a bow component and an inventory."));
		return true;
	}

	switch (static_cast<EBowShot>(Shot))
	{
	case EBowShot::Holster:
		EnsureQuiver(Test, Inventory);
		ReportBow(Test, Kate, TEXT("bow_holstered.png"));
		break;

	case EBowShot::DrawStart:
		if (!Bow->StartDraw())
		{
			Test->AddWarning(TEXT("The bow would not draw."));
		}
		break;

	case EBowShot::DrawCancel:
		ReportBow(Test, Kate, TEXT("bow_draw.png"));
		Bow->CancelDraw();
		break;

	case EBowShot::HitSetup:
	{
		FVector Spot, Away, Ground, ThugGround;
		if (!FindStreetSpot(World, Spot, Away) || !FindGround(World, Spot, 3000.f, Kate, Ground))
		{
			Test->AddWarning(TEXT("No street spot for the bow hit."));
			break;
		}
		const float AlongYaw = FVector::CrossProduct(FVector::UpVector, Away).Rotation().Yaw;
		PlaceKate(Kate, PC, Ground, AlongYaw, 0.f);
		const FVector Along = FRotator(0.f, AlongYaw, 0.f).Vector();
		if (!FindGround(World, Ground + Along * ThugDistance, Ground.Z + 400.f, Kate, ThugGround))
		{
			Test->AddWarning(TEXT("No ground 15 m down the street for the thug."));
			break;
		}
		TargetThug = SpawnThug(World, ThugGround, AlongYaw + 180.f, false);
		Kate->StartAim();
		Test->AddInfo(FString::Printf(TEXT("Thug %s at %s, %.0f cm from Kate."), *GetNameSafe(TargetThug.Get()),
			*ThugGround.ToCompactString(), FVector::Dist2D(ThugGround, Ground)));
		break;
	}

	case EBowShot::HitAim:
		if (AThugCharacter* Thug = TargetThug.Get())
		{
			// The chest, plus the 30 cm an arrow drops over 15 m at full draw.
			AimCameraAt(PC, Kate, Thug->GetActorLocation() + FVector(0.f, 0.f, 60.f));
		}
		break;

	case EBowShot::HitDraw:
		if (AThugCharacter* Thug = TargetThug.Get())
		{
			AimCameraAt(PC, Kate, Thug->GetActorLocation() + FVector(0.f, 0.f, 60.f));
		}
		Bow->StartDraw();
		break;

	case EBowShot::HitRelease:
		ReportBow(Test, Kate, TEXT("bow_hit (release)"));
		if (!Bow->ReleaseDraw())
		{
			Test->AddWarning(TEXT("The release did not fire."));
		}
		// The player keeps the aim button held through the shot, so the camera stays in.
		Kate->StartAim();
		break;

	case EBowShot::Cleanup:
		Kate->StopAim();
		for (TActorIterator<AArrowProjectile> It(World); It; ++It)
		{
			It->Destroy();
		}
		if (AThugCharacter* Thug = TargetThug.Get())
		{
			Thug->Destroy();
		}
		TargetThug.Reset();
		break;

	case EBowShot::RoofThugs:
	{
		// TODO(stage2): placing thugs on the district belongs to the combat task; generate_city.py
		// is not the place. Until then the shot pass puts two on the cross_block roof to look at.
		for (TActorIterator<AObjectiveTriggerVolume> It(World); It; ++It)
		{
			if (It->ObjectiveId != FName(TEXT("cross_block")))
			{
				continue;
			}
			for (const float Offset : { -150.f, 150.f })
			{
				FVector Roof;
				const FVector At = It->GetActorLocation() + FVector(Offset, 0.f, 0.f);
				if (FindGround(World, At, At.Z + 300.f, Kate, Roof))
				{
					AThugCharacter* Thug = SpawnThug(World, Roof, 0.f, true);
					Test->AddInfo(FString::Printf(TEXT("Roof thug %s on cross_block at %s."), *GetNameSafe(Thug),
						*Roof.ToCompactString()));
				}
			}
			break;
		}
		break;
	}
	}
	return true;
}

/** Waits (up to TimeoutSeconds) for an arrow to stick, then reports what it hit. */
class FCastleKateWaitArrowHit : public IAutomationLatentCommand
{
public:
	FCastleKateWaitArrowHit(FAutomationTestBase* InTest, float InTimeoutSeconds)
		: Test(InTest), TimeoutSeconds(InTimeoutSeconds)
	{
	}

	virtual bool Update() override
	{
		UWorld* World = CastleKateShots::FindWorld();
		if (!World)
		{
			return true;
		}
		if (StartTime < 0.0)
		{
			StartTime = World->GetTimeSeconds();
		}
		for (TActorIterator<AArrowProjectile> It(World); It; ++It)
		{
			if (It->IsStuck())
			{
				const AThugCharacter* Thug = CastleKateShots::TargetThug.Get();
				Test->AddInfo(FString::Printf(TEXT("Arrow stuck in %s at %s after %.2f s; thug health %.1f, staggered %d."),
					*GetNameSafe(It->GetStuckInActor()), *It->GetActorLocation().ToCompactString(),
					World->GetTimeSeconds() - StartTime,
					Thug && Thug->GetHealthComponent() ? Thug->GetHealthComponent()->GetCurrentHealth() : -1.f,
					Thug && Thug->IsStaggered() ? 1 : 0));
				if (It->GetStuckInActor() != Thug)
				{
					Test->AddWarning(TEXT("The arrow did not stick in the thug."));
				}
				return true;
			}
		}
		if (World->GetTimeSeconds() - StartTime > TimeoutSeconds)
		{
			Test->AddWarning(TEXT("No arrow stuck anywhere."));
			return true;
		}
		return false;
	}

private:
	FAutomationTestBase* Test;
	float TimeoutSeconds;
	double StartTime = -1.0;
};

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
	// The bow: on her back, the quiver, half drawn, then an arrow into a thug 15 m away.
	using EBow = CastleKateShots::EBowShot;
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateFrameShot(this, static_cast<uint8>(EShot::Street)));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateBowShot(this, static_cast<uint8>(EBow::Holster)));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateTakeShot(this, TEXT("bow_holstered.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateTakeShot(this, TEXT("quiver_hud.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateBowShot(this, static_cast<uint8>(EBow::DrawStart)));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.4f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateTakeShot(this, TEXT("bow_draw.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.1f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateBowShot(this, static_cast<uint8>(EBow::DrawCancel)));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateBowShot(this, static_cast<uint8>(EBow::HitSetup)));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateBowShot(this, static_cast<uint8>(EBow::HitAim)));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateBowShot(this, static_cast<uint8>(EBow::HitDraw)));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.85f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateBowShot(this, static_cast<uint8>(EBow::HitRelease)));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateWaitArrowHit(this, 2.f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateTakeShot(this, TEXT("bow_hit.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateBowShot(this, static_cast<uint8>(EBow::Cleanup)));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));

	// Locomotion: a run, the stop after it, and an aimed strafe, captured mid-move.
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateFrameShot(this, static_cast<uint8>(EShot::Run)));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateReportCamera(this, TEXT("kate_run.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateTakeShot(this, TEXT("kate_run.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.1f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateFrameShot(this, static_cast<uint8>(EShot::Stop)));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateReportCamera(this, TEXT("kate_stop.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateTakeShot(this, TEXT("kate_stop.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateFrameShot(this, static_cast<uint8>(EShot::AimStrafe)));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateReportCamera(this, TEXT("kate_aimstrafe.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateTakeShot(this, TEXT("kate_aimstrafe.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.1f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateFrameShot(this, static_cast<uint8>(EShot::EndMove)));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));

	// A 12 m drop: past the roll height and the fall-damage threshold.
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateDrop(this, 1200.f, false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(3.f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateDrop(this, 1200.f, true));

	// Frame time at the street start, before anything moves.
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateFrameShot(this, static_cast<uint8>(EShot::Street)));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateFrameTime(this, TEXT("street"), 60));

	// The grapple: marker, mid-zip, landed.
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateAimAtAnchor(this));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateReportMarker(this));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateTakeShot(this, TEXT("grapple_marker.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateFireGrapple(this));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateWaitZip(this, 0.45f, 3.f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateTakeShot(this, TEXT("grapple_mid.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateWaitZip(this, 2.f, 5.f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateLookAcrossRoof());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateReportCamera(this, TEXT("grapple_roof.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateTakeShot(this, TEXT("grapple_roof.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateFrameTime(this, TEXT("roof"), 60));

	// Looking steeply up from the street, then the parkour moves.
	using EParkour = CastleKateShots::EParkourShot;
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateParkourShot(this, static_cast<uint8>(EParkour::Lookup)));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateReportLookup(this));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateReportCamera(this, TEXT("kate_lookup.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateTakeShot(this, TEXT("kate_lookup.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));

	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateParkourShot(this, static_cast<uint8>(EParkour::VaultRun)));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateWaitTraversal(this, 4.f, false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.4f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateReportParkour(this, TEXT("vault_mid.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateTakeShot(this, TEXT("vault_mid.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateParkourShot(this, static_cast<uint8>(EParkour::EndInput)));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));

	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateParkourShot(this, static_cast<uint8>(EParkour::MantleStand)));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateParkourShot(this, static_cast<uint8>(EParkour::MantleJump)));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateWaitTraversal(this, 2.f, false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.45f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateReportParkour(this, TEXT("mantle_mid.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateTakeShot(this, TEXT("mantle_mid.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.f));

	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateParkourShot(this, static_cast<uint8>(EParkour::LedgeFall)));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateWaitTraversal(this, 3.f, true));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.8f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateReportParkour(this, TEXT("ledge_hang.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateTakeShot(this, TEXT("ledge_hang.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateParkourShot(this, static_cast<uint8>(EParkour::Climb)));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.4f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateLookAcrossRoof());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateReportParkour(this, TEXT("climb_top.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateReportCamera(this, TEXT("climb_top.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateTakeShot(this, TEXT("climb_top.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));

	ADD_LATENT_AUTOMATION_COMMAND(FCastleKateBowShot(this, static_cast<uint8>(EBow::RoofThugs)));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));

	ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(TEXT("castle.DebugMovement 0")));
	return true;
}

#endif

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Camera/CameraComponent.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Player/HawkeyeCharacter.h"
#include "Player/ParkourComponent.h"
#include "Tests/AutomationCommon.h"
#include "Tests/HangScreenshots.h"
#include "Tests/HawkeyeShots.h"
#include "World/CityLedgeSpawner.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * The hang on the district's own ledges (claude-docs/gameplay-semantics.md, traversal, "Hang"): a real tenement roof
 * corner found from the spawned ledges' Ledge_1 splines, a hang on one face 200 cm from it, a shimmy into it, the
 * turn onto the other face and back. Needs the district as a game world: run it from the standalone game (-game).
 * The Kate pass's hang shots (hang_shimmy.png, hang_corner.png) use the same corner.
 */
namespace HawkeyeHangDistrict
{
	static const TCHAR* MoveActionPath = TEXT("/Game/Input/IA_Move.IA_Move");
	/** Hang this far from the corner to start with, cm. */
	static constexpr float StartBack = 200.f;

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

	static AHawkeyeCharacter* FindKate(APlayerController*& OutPC)
	{
		UWorld* World = FindWorld();
		OutPC = World ? World->GetFirstPlayerController() : nullptr;
		return OutPC ? Cast<AHawkeyeCharacter>(OutPC->GetPawn()) : nullptr;
	}

	/** One roof edge's Ledge_1: its two ends on the parapet top and its outward normal. */
	struct FEdge
	{
		FVector A = FVector::ZeroVector;
		FVector B = FVector::ZeroVector;
		FVector Normal = FVector::ZeroVector;
		AActor* Actor = nullptr;
	};

	/** An outside roof corner two ledges share, and where along the first one to hang. */
	struct FCorner
	{
		bool bFound = false;
		FVector Corner = FVector::ZeroVector;
		FVector NormalA = FVector::ZeroVector;
		FVector NormalB = FVector::ZeroVector;
		/** Along A's edge toward the corner. */
		FVector TowardCorner = FVector::ZeroVector;
		float TopZ = 0.f;
		FString LedgeA;
		FString LedgeB;
		float LengthA = 0.f;
		float LengthB = 0.f;
		float Ground = 0.f;
	};
	static FCorner Found;

	static bool ReadEdge(AActor* Actor, FEdge& Out)
	{
		TInlineComponentArray<USplineComponent*> Splines(Actor);
		for (const USplineComponent* Spline : Splines)
		{
			if (Spline && Spline->GetName() == TEXT("Ledge_1") && Spline->GetNumberOfSplinePoints() >= 2)
			{
				Out.A = Spline->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World);
				Out.B = Spline->GetLocationAtSplinePoint(Spline->GetNumberOfSplinePoints() - 1, ESplineCoordinateSpace::World);
				Out.Normal = Spline->GetUpVectorAtSplinePoint(0, ESplineCoordinateSpace::World).GetSafeNormal2D();
				Out.Actor = Actor;
				return !Out.Normal.IsNearlyZero();
			}
		}
		return false;
	}

	static bool CapsuleFree(UWorld* World, const AActor* Ignore, const FVector& Centre)
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(HangDistrictFit), false, Ignore);
		return !World->OverlapBlockingTestByChannel(Centre, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(34.f, 88.f),
			Params);
	}

	/**
	 * The outside roof corner nearest Near where both faces are open: a hang fits on A from 250 cm out to the corner
	 * and on B 25 cm round it, half way round the corner too, with air out in front for the camera and the street a
	 * floor or more below.
	 */
	static FCorner FindCorner(UWorld* World, const AHawkeyeCharacter* Kate, const FVector& Near, FString& OutReport)
	{
		FCorner Best;
		TArray<FEdge> Edges;
		for (TActorIterator<ACityLedgeSpawner> It(World); It; ++It)
		{
			for (AActor* Ledge : It->GetSpawnedLedges())
			{
				FEdge Edge;
				if (Ledge && ReadEdge(Ledge, Edge))
				{
					Edges.Add(Edge);
				}
			}
		}
		// Ends on a 100 cm grid, so each end only meets its neighbours.
		TMultiMap<FIntPoint, int32> Grid;
		auto Cell = [](const FVector& P) { return FIntPoint(FMath::FloorToInt(P.X / 100.f), FMath::FloorToInt(P.Y / 100.f)); };
		for (int32 Index = 0; Index < Edges.Num(); ++Index)
		{
			Grid.Add(Cell(Edges[Index].A), Index);
			Grid.Add(Cell(Edges[Index].B), Index);
		}
		int32 Candidates = 0;
		float BestDistance = TNumericLimits<float>::Max();
		const float Half = 88.f;
		for (int32 I = 0; I < Edges.Num(); ++I)
		{
			const FEdge& EdgeA = Edges[I];
			for (int32 End = 0; End < 2; ++End)
			{
				const FVector Corner = End == 0 ? EdgeA.A : EdgeA.B;
				const FVector Other = End == 0 ? EdgeA.B : EdgeA.A;
				const FVector Toward = (Corner - Other).GetSafeNormal2D();
				const float LengthA = FVector::Dist2D(Corner, Other);
				if (LengthA < 300.f)
				{
					continue;
				}
				const FIntPoint Here = Cell(Corner);
				for (int32 DX = -1; DX <= 1; ++DX)
				{
					for (int32 DY = -1; DY <= 1; ++DY)
					{
						TArray<int32> Near3;
						Grid.MultiFind(FIntPoint(Here.X + DX, Here.Y + DY), Near3);
						for (const int32 J : Near3)
						{
							if (J == I)
							{
								continue;
							}
							const FEdge& EdgeB = Edges[J];
							const bool bBAtA = FVector::Dist(EdgeB.A, Corner) < 8.f;
							const bool bBAtB = FVector::Dist(EdgeB.B, Corner) < 8.f;
							if (!bBAtA && !bBAtB)
							{
								continue;
							}
							const FVector OtherB = bBAtA ? EdgeB.B : EdgeB.A;
							const FVector TowardB = (Corner - OtherB).GetSafeNormal2D();
							// Outside: each face's normal points along the other's way to the corner.
							if (FMath::Abs(FVector::DotProduct(EdgeA.Normal, EdgeB.Normal)) > 0.1f
								|| FVector::DotProduct(EdgeB.Normal, Toward) < 0.9f || FVector::DotProduct(EdgeA.Normal, TowardB) < 0.9f
								|| FVector::Dist2D(Corner, OtherB) < 150.f)
							{
								continue;
							}
							++Candidates;
							const float Z = Corner.Z + Half - 145.f;
							const FVector HangA0 = Corner - Toward * 250.f + EdgeA.Normal * 38.f;
							const FVector HangA1 = Corner - Toward * 30.f + EdgeA.Normal * 38.f;
							const FVector HangB = Corner - TowardB * 30.f + EdgeB.Normal * 38.f;
							const FVector Diagonal = Corner + (EdgeA.Normal + EdgeB.Normal).GetSafeNormal2D() * 50.f;
							const FVector Air = Corner - Toward * StartBack + EdgeA.Normal * 45.f;
							bool bFree = true;
							for (const FVector& Point : { HangA0, HangA1, HangB, Diagonal })
							{
								bFree &= CapsuleFree(World, Kate, FVector(Point.X, Point.Y, Z));
							}
							bFree &= CapsuleFree(World, Kate, FVector(Air.X, Air.Y, Corner.Z - 200.f + Half + 2.f));
							if (!bFree)
							{
								continue;
							}
							// The camera's room out in front, and the ground well below.
							FCollisionQueryParams Params(SCENE_QUERY_STAT(HangDistrictAir), false, Kate);
							FHitResult Hit;
							const FVector Eye(HangA0.X, HangA0.Y, Z + 60.f);
							if (World->LineTraceSingleByChannel(Hit, Eye, Eye + EdgeA.Normal * 450.f, ECC_Camera, Params))
							{
								continue;
							}
							const FVector Mid = Corner - Toward * StartBack + EdgeA.Normal * 60.f;
							if (!World->LineTraceSingleByChannel(Hit, FVector(Mid.X, Mid.Y, Z), FVector(Mid.X, Mid.Y, Z - 5000.f),
									ECC_Visibility, Params)
								|| Corner.Z - Hit.ImpactPoint.Z < 800.f)
							{
								continue;
							}
							const float Distance = FVector::Dist2D(Corner, Near);
							if (Distance < BestDistance)
							{
								BestDistance = Distance;
								Best.bFound = true;
								Best.Corner = Corner;
								Best.NormalA = EdgeA.Normal;
								Best.NormalB = EdgeB.Normal;
								Best.TowardCorner = Toward;
								Best.TopZ = Corner.Z;
								Best.LedgeA = EdgeA.Actor->GetActorNameOrLabel();
								Best.LedgeB = EdgeB.Actor->GetActorNameOrLabel();
								Best.LengthA = LengthA;
								Best.LengthB = FVector::Dist2D(Corner, OtherB);
								Best.Ground = Hit.ImpactPoint.Z;
							}
						}
					}
				}
			}
		}
		OutReport = FString::Printf(TEXT("%d ledges, %d outside corners between two of them; %s"), Edges.Num(), Candidates,
			Best.bFound ? *FString::Printf(TEXT("the nearest open one at %s (%.0f m off), %s (%.0f cm) meets %s (%.0f cm), top %.0f cm above the street"),
				*Best.Corner.ToCompactString(), BestDistance / 100.f, *Best.LedgeA, Best.LengthA, *Best.LedgeB, Best.LengthB,
				Best.TopZ - Best.Ground) : TEXT("none open"));
		return Best;
	}

	/** Kate in the air against face A, BackFromCorner from the corner, falling onto its edge; the camera set. */
	static void DropOntoFaceA(AHawkeyeCharacter* Kate, APlayerController* PC, const FCorner& Corner, float BackFromCorner,
		float CameraYawOffset, float CameraPitch)
	{
		const float Half = Kate->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const FVector Edge = Corner.Corner - Corner.TowardCorner * BackFromCorner;
		const FVector Air = FVector(Edge.X, Edge.Y, Corner.TopZ - 200.f + Half + 2.f) + Corner.NormalA * 45.f;
		const float FaceYaw = (-Corner.NormalA).Rotation().Yaw;
		if (Kate->GetParkourComponent()->IsHanging())
		{
			Kate->GetParkourComponent()->DropFromHang();
		}
		Kate->StopAim();
		Kate->TeleportTo(Air, FRotator(0.f, FaceYaw, 0.f), false, true);
		Kate->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		Kate->GetCharacterMovement()->Velocity = FVector(0.f, 0.f, -10.f);
		PC->SetControlRotation(FRotator(CameraPitch, FaceYaw + CameraYawOffset, 0.f));
		PC->SetViewTarget(Kate);
	}

	/** +1 when the corner is to her right as she hangs on A, -1 to her left. */
	static float CornerSide(const FCorner& Corner)
	{
		const FVector Right = FVector::CrossProduct(FVector::UpVector, -Corner.NormalA).GetSafeNormal2D();
		return FVector::DotProduct(Right, Corner.TowardCorner) >= 0.f ? 1.f : -1.f;
	}

	static void HoldMove(APlayerController* PC, const FVector2D& Value, bool bHold)
	{
		const UInputAction* Move = LoadObject<UInputAction>(nullptr, MoveActionPath);
		ULocalPlayer* Player = PC ? PC->GetLocalPlayer() : nullptr;
		UEnhancedInputLocalPlayerSubsystem* Input = Player ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Player) : nullptr;
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

	/** The spring arm, frame by frame: how far the lens is from the pivot and how much that jumps between frames. */
	struct FArmWatch
	{
		int32 Frames = 0;
		int32 CollisionFrames = 0;
		float MinLength = TNumericLimits<float>::Max();
		float MaxLength = 0.f;
		float MaxJump = 0.f;
		float Last = -1.f;

		void Reset() { *this = FArmWatch(); }

		void Sample(const AHawkeyeCharacter* Kate)
		{
			const USpringArmComponent* Boom = Kate ? Kate->GetCameraBoom() : nullptr;
			const UCameraComponent* Lens = Kate ? Kate->GetFollowCamera() : nullptr;
			if (!Boom || !Lens)
			{
				return;
			}
			const float Length = FVector::Dist(Lens->GetComponentLocation(), Boom->GetComponentLocation() + Boom->TargetOffset);
			if (Last >= 0.f)
			{
				MaxJump = FMath::Max(MaxJump, FMath::Abs(Length - Last));
			}
			Last = Length;
			MinLength = FMath::Min(MinLength, Length);
			MaxLength = FMath::Max(MaxLength, Length);
			CollisionFrames += Boom->IsCollisionFixApplied() ? 1 : 0;
			++Frames;
		}

		FString Describe() const
		{
			return FString::Printf(TEXT("%d frames: the lens %.0f to %.0f cm from the pivot, the largest jump between frames %.1f cm, the arm pulled in by collision on %d"),
				Frames, MinLength, MaxLength, MaxJump, CollisionFrames);
		}
	};
	static FArmWatch Arm;

	static FString ShotPath(const FString& FileName)
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Screenshots") / TEXT("Kate") / FileName);
	}

	static FString DescribeHang(const AHawkeyeCharacter* Kate)
	{
		const UParkourComponent* Parkour = Kate->GetParkourComponent();
		const FVector2D Hands = Parkour->GetHangHandOffsets();
		const USkeletalMeshComponent* Body = Kate->GetMesh();
		const float TopZ = Parkour->GetHangLine().Origin.Z;
		const FVector HandL = Body ? Body->GetSocketLocation(TEXT("hand_l")) : FVector::ZeroVector;
		const FVector HandR = Body ? Body->GetSocketLocation(TEXT("hand_r")) : FVector::ZeroVector;
		const FVector Right = Kate->GetActorRightVector();
		return FString::Printf(
			TEXT("move %s, hanging %d, shimmying %d, %.0f cm along %s; hand targets %.0f / %.0f cm along from the capsule; hand bones %.0f / %.0f cm along, %.0f / %.0f cm above the top; feet %.0f under the top; yaw %.0f"),
			*UEnum::GetValueAsString(Parkour->GetActiveMove()), Parkour->IsHanging() ? 1 : 0, Parkour->IsShimmying() ? 1 : 0,
			Parkour->GetHangAlong(), *Parkour->GetHangLine().Source, Hands.X, Hands.Y,
			FVector::DotProduct(HandL - Kate->GetActorLocation(), Right), FVector::DotProduct(HandR - Kate->GetActorLocation(), Right),
			HandL.Z - TopZ, HandR.Z - TopZ, TopZ - (Kate->GetActorLocation().Z - Kate->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()),
			Kate->GetActorRotation().Yaw);
	}

	static FVector NearPoint(UWorld* World, const AHawkeyeCharacter* Kate)
	{
		TActorIterator<APlayerStart> Start(World);
		return Start ? Start->GetActorLocation() : Kate->GetActorLocation();
	}
}

/** The district test: shimmy along a real tenement parapet, round its corner, and back. */
class FHawkeyeHangDistrictRun : public IAutomationLatentCommand
{
public:
	explicit FHawkeyeHangDistrictRun(FAutomationTestBase* InTest) : Test(InTest) {}

	virtual bool Update() override
	{
		using namespace HawkeyeHangDistrict;
		UWorld* World = FindWorld();
		APlayerController* PC = nullptr;
		AHawkeyeCharacter* Kate = FindKate(PC);
		UParkourComponent* Parkour = Kate ? Kate->GetParkourComponent() : nullptr;
		if (!World || !PC || !Parkour)
		{
			Test->AddError(TEXT("No Kate in the district."));
			return true;
		}
		const double Now = World->GetTimeSeconds();
		if (PhaseStart < 0.0)
		{
			PhaseStart = Now;
		}
		const double InPhase = Now - PhaseStart;
		auto Next = [this, Now](int32 NewPhase)
		{
			Phase = NewPhase;
			PhaseStart = Now;
		};
		// Never inside anything, the whole way.
		if (Phase >= 2)
		{
			FCollisionQueryParams Params(SCENE_QUERY_STAT(HangDistrictClip), false, Kate);
			const UCapsuleComponent* Capsule = Kate->GetCapsuleComponent();
			if (World->OverlapBlockingTestByChannel(Kate->GetActorLocation(), FQuat::Identity, ECC_Pawn,
					FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius() - 4.f, Capsule->GetScaledCapsuleHalfHeight() - 4.f), Params))
			{
				++ClippingFrames;
			}
		}
		switch (Phase)
		{
		case 0:
		{
			// Every ledge out first.
			bool bDone = true;
			for (TActorIterator<ACityLedgeSpawner> It(World); It; ++It)
			{
				bDone &= It->IsSpawnComplete();
			}
			if (!bDone && InPhase < 15.0)
			{
				return false;
			}
			FString Report;
			Found = FindCorner(World, Kate, NearPoint(World, Kate), Report);
			Test->AddInfo(TEXT("Hang corner: ") + Report);
			if (!Found.bFound)
			{
				Test->AddError(TEXT("No open outside roof corner in the district's ledge data."));
				return true;
			}
			DropOntoFaceA(Kate, PC, Found, StartBack, 0.f, 10.f);
			Next(1);
			return false;
		}
		case 1:
			if (Parkour->IsHanging())
			{
				const FHawkeyeLedgeLine& Line = Parkour->GetHangLine();
				Test->AddInfo(FString::Printf(TEXT("Hanging on %s: line %s (%d data segments), %.0f to %.0f cm, at %.0f"), *Found.LedgeA,
					*Line.Source, Line.DataSegments, Line.MinAlong, Line.MaxAlong, Parkour->GetHangAlong()));
				Test->TestTrue(TEXT("The hang's line comes from the ledge data"), Line.DataSegments >= 1 && Line.Source.Contains(TEXT("Ledge_")));
				Test->TestTrue(TEXT("It runs to the corner"),
					FVector::Dist2D(Line.PointAt(CornerSide(Found) > 0.f ? Line.MaxAlong : Line.MinAlong), Found.Corner) < 10.f);
				StartAlong = Parkour->GetHangAlong();
				Next(2);
			}
			else if (InPhase > 3.0)
			{
				Test->AddError(FString::Printf(TEXT("No hang on %s within 3 s: %s"), *Found.LedgeA, *Kate->GetMovementDebugText()));
				return true;
			}
			return false;
		case 2:
			// Toward the corner: 175 cm of shimmy at 120 cm/s, then the turn.
			Parkour->SetHangInput(Found.TowardCorner);
			if (!bSpeedTaken && InPhase >= 1.0)
			{
				bSpeedTaken = true;
				const float Speed = FMath::Abs(Parkour->GetHangAlong() - StartAlong) / static_cast<float>(InPhase);
				Test->AddInfo(FString::Printf(TEXT("Shimmy speed over the first second: %.0f cm/s"), Speed));
				Test->TestEqual(TEXT("The shimmy runs at about 120 cm/s"), Speed, 120.f, 12.f);
			}
			if (Parkour->GetActiveMove() == EHawkeyeParkourMove::HangCorner)
			{
				Test->AddInfo(FString::Printf(TEXT("Round the corner %.2f s after the shimmy began"), InPhase));
				Next(3);
			}
			else if (InPhase > 4.0)
			{
				Test->AddError(FString::Printf(TEXT("No corner turn within 4 s: %s (%s)"), *DescribeHang(Kate), *Parkour->GetLastHangRefusal()));
				return true;
			}
			return false;
		case 3:
			if (Parkour->IsHanging())
			{
				const FVector Location = Kate->GetActorLocation();
				Test->AddInfo(FString::Printf(TEXT("Round the corner in %.2f s: %s"), InPhase, *DescribeHang(Kate)));
				Test->TestTrue(TEXT("Facing the other face"), FVector::DotProduct(Kate->GetActorForwardVector().GetSafeNormal2D(), -Found.NormalB) > 0.98f);
				Test->TestEqual(TEXT("36 cm off it"), static_cast<float>(FVector::DotProduct(Location - Found.Corner, Found.NormalB)), 36.f, 4.f);
				Test->TestEqual(TEXT("Feet 145 cm under the top"),
					Found.TopZ - static_cast<float>(Location.Z - Kate->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), 145.f, 3.f);
				Test->TestTrue(TEXT("Its line is the other ledge's"), Parkour->GetHangLine().Source.Contains(Found.LedgeB));
				Next(4);
			}
			else if (InPhase > 1.0)
			{
				Test->AddError(FString::Printf(TEXT("The corner turn did not end in a hang: %s"), *DescribeHang(Kate)));
				return true;
			}
			return false;
		case 4:
			// Back the way she came, into the corner again from this side.
			Parkour->SetHangInput(Found.NormalA);
			if (Parkour->GetActiveMove() == EHawkeyeParkourMove::HangCorner)
			{
				Next(5);
			}
			else if (InPhase > 2.0)
			{
				Test->AddError(FString::Printf(TEXT("No turn back round the corner: %s (%s)"), *DescribeHang(Kate), *Parkour->GetLastHangRefusal()));
				return true;
			}
			return false;
		case 5:
			if (Parkour->IsHanging())
			{
				Test->TestTrue(TEXT("Back on the first face"),
					FVector::DotProduct(Kate->GetActorForwardVector().GetSafeNormal2D(), -Found.NormalA) > 0.98f);
				Test->TestEqual(TEXT("Capsule never inside anything"), ClippingFrames, 0);
				Test->AddInfo(FString::Printf(TEXT("And back: %s; capsule in geometry on %d frames"), *DescribeHang(Kate), ClippingFrames));
				Parkour->DropFromHang();
				return true;
			}
			if (InPhase > 1.0)
			{
				Test->AddError(TEXT("The turn back did not end in a hang."));
				return true;
			}
			return false;
		default:
			return true;
		}
	}

private:
	FAutomationTestBase* Test;
	int32 Phase = 0;
	double PhaseStart = -1.0;
	float StartAlong = 0.f;
	bool bSpeedTaken = false;
	int32 ClippingFrames = 0;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHawkeyeParkourDistrictHangCorner, "Hawkeye.Parkour.DistrictHangCorner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FHawkeyeParkourDistrictHangCorner::RunTest(const FString& Parameters)
{
	if (GIsEditor)
	{
		AddInfo(TEXT("Needs the district as a game world: skipped in the editor. Run it from the standalone game (-game)."));
		return true;
	}
	AutomationOpenMap(TEXT("/Game/Maps/L_District_EastVillage"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(6.f));
	ADD_LATENT_AUTOMATION_COMMAND(FHawkeyeHangDistrictRun(this));
	return true;
}

void HawkeyeAddHangShots(FAutomationTestBase* Test)
{
	using namespace HawkeyeHangDistrict;
	static double Mark = 0.0;
	auto Clock = []() { UWorld* World = FindWorld(); return World ? World->GetTimeSeconds() : 0.0; };
	// Off any hang the pass left her in first: a drop's regrab cooldown (0.6 s) would miss the catch below.
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([]()
	{
		APlayerController* PC = nullptr;
		AHawkeyeCharacter* Kate = FindKate(PC);
		if (Kate && Kate->GetParkourComponent()->IsHanging())
		{
			Kate->GetParkourComponent()->DropFromHang();
		}
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.8f));
	// The corner, and Kate dropped onto its first face 200 cm from it, the camera behind her and a little toward it.
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Test, Clock]()
	{
		APlayerController* PC = nullptr;
		AHawkeyeCharacter* Kate = FindKate(PC);
		UWorld* World = FindWorld();
		if (!Kate || !PC || !World)
		{
			Test->AddError(TEXT("No Kate for the hang shots."));
			return true;
		}
		FString Report;
		Found = FindCorner(World, Kate, NearPoint(World, Kate), Report);
		Test->AddInfo(TEXT("hang_shimmy.png: ") + Report);
		if (!Found.bFound)
		{
			Test->AddWarning(TEXT("hang_shimmy.png: no open roof corner."));
			return true;
		}
		HoldMove(PC, FVector2D::ZeroVector, false);
		DropOntoFaceA(Kate, PC, Found, StartBack, 30.f * CornerSide(Found), 12.f);
		Mark = Clock();
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Test, Clock]()
	{
		APlayerController* PC = nullptr;
		AHawkeyeCharacter* Kate = FindKate(PC);
		if (!Found.bFound || !Kate)
		{
			return true;
		}
		if (Kate->GetParkourComponent()->IsHanging())
		{
			// A beat to settle, then the stick toward the corner through IA_Move, as a pad would.
			if (Clock() - Mark > 0.6)
			{
				Arm.Reset();
				HoldMove(PC, FVector2D(CornerSide(Found), 0.f), true);
				Mark = Clock();
				return true;
			}
			return false;
		}
		if (Clock() - Mark > 3.0)
		{
			Test->AddWarning(FString::Printf(TEXT("hang_shimmy.png: no hang: %s"), *Kate->GetMovementDebugText()));
			return true;
		}
		return false;
	}));
	// 0.9 s into the shimmy.
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Test, Clock]()
	{
		APlayerController* PC = nullptr;
		AHawkeyeCharacter* Kate = FindKate(PC);
		if (!Found.bFound || !Kate)
		{
			return true;
		}
		Arm.Sample(Kate);
		if (Clock() - Mark < 0.9)
		{
			return false;
		}
		Test->AddInfo(TEXT("hang_shimmy.png: ") + DescribeHang(Kate));
		if (!Kate->GetParkourComponent()->IsShimmying())
		{
			Test->AddWarning(TEXT("hang_shimmy.png: Kate is not shimmying."));
		}
		HawkeyeShots::Request(Test, ShotPath(TEXT("hang_shimmy.png")), /*bShowUI=*/true);
		Mark = Clock();
		return true;
	}));
	// On into the corner; the shot 0.18 s into the 0.4 s turn.
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Test, Clock]()
	{
		APlayerController* PC = nullptr;
		AHawkeyeCharacter* Kate = FindKate(PC);
		if (!Found.bFound || !Kate)
		{
			return true;
		}
		Arm.Sample(Kate);
		if (Kate->GetParkourComponent()->GetActiveMove() == EHawkeyeParkourMove::HangCorner)
		{
			Mark = Clock();
			return true;
		}
		if (Clock() - Mark > 4.0)
		{
			Test->AddWarning(FString::Printf(TEXT("hang_corner.png: no corner turn: %s (%s)"), *DescribeHang(Kate),
				*Kate->GetParkourComponent()->GetLastHangRefusal()));
			Mark = -100.0;
			return true;
		}
		return false;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Test, Clock]()
	{
		APlayerController* PC = nullptr;
		AHawkeyeCharacter* Kate = FindKate(PC);
		if (!Found.bFound || !Kate || Mark < 0.0)
		{
			return true;
		}
		Arm.Sample(Kate);
		if (Clock() - Mark < 0.18)
		{
			return false;
		}
		Test->AddInfo(TEXT("hang_corner.png: ") + DescribeHang(Kate));
		HawkeyeShots::Request(Test, ShotPath(TEXT("hang_corner.png")), /*bShowUI=*/true);
		Mark = Clock();
		return true;
	}));
	// Round, a moment on the new face, then let go.
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Test, Clock]()
	{
		APlayerController* PC = nullptr;
		AHawkeyeCharacter* Kate = FindKate(PC);
		if (!Found.bFound || !Kate)
		{
			return true;
		}
		Arm.Sample(Kate);
		if (Clock() - Mark < 0.8)
		{
			return false;
		}
		HoldMove(PC, FVector2D::ZeroVector, false);
		Test->AddInfo(TEXT("Hang shots, the spring arm: ") + Arm.Describe());
		if (Arm.MaxJump > 30.f)
		{
			Test->AddWarning(FString::Printf(TEXT("The spring arm jumped %.0f cm in one frame while she hung."), Arm.MaxJump));
		}
		Test->AddInfo(TEXT("After the corner: ") + DescribeHang(Kate));
		if (Kate->GetParkourComponent()->IsHanging())
		{
			Kate->GetParkourComponent()->DropFromHang();
		}
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.5f));
}

/** The hang shots on their own. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHawkeyeScreenshotHang, "Hawkeye.Screenshot.Hang",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FHawkeyeScreenshotHang::RunTest(const FString& Parameters)
{
	if (!FApp::CanEverRender())
	{
		AddInfo(TEXT("No RHI: skipping the hang screenshots."));
		return true;
	}
	AutomationOpenMap(TEXT("/Game/Maps/L_District_EastVillage"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(8.f));
	HawkeyeAddHangShots(this);
	ADD_LATENT_AUTOMATION_COMMAND(FHawkeyeWaitForShots(this));
	return true;
}

#endif

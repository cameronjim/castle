// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/ParkourComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Castle.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/Paths.h"
#include "Player/CastleCharacter.h"
#include "Player/GaspTraversal.h"

namespace CastleParkour
{
	static const TCHAR* TraversalAnims = TEXT("/Game/Characters/UEFN_Mannequin/Animations/Traversal/");

	static FCastleParkourClip MakeClip(const TCHAR* RelativePath, float StartTime, float EndTime)
	{
		FCastleParkourClip Clip;
		const FString Package = FString(TraversalAnims) + RelativePath;
		Clip.Sequence = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(Package + TEXT(".") + FPaths::GetBaseFilename(Package)));
		Clip.StartTime = StartTime;
		Clip.EndTime = EndTime;
		return Clip;
	}

	/** Heights the front face is probed at, above the feet. The lowest skips kerbs and steps. */
	static constexpr float FaceProbeLowest = 35.f;
	static constexpr float FaceProbeStep = 35.f;
	/** How far behind the front face the top is looked for, cm. Less than a fire-escape rail is thick. */
	static constexpr float TopInset = 4.f;
	/** A top this steep is not a top. */
	static constexpr float MinFloorNormalZ = 0.7f;
	/** Step between back-edge probes, cm. */
	static constexpr float BeyondStep = 15.f;
	/** Past the back edge, the landing capsule is this much further out, cm. */
	static constexpr float LandingClearance = 25.f;
	/** On top, the standing capsule is this much further in than its radius, cm. */
	static constexpr float StandInset = 15.f;
	/** The top of the landing search below the feet, cm. */
	static constexpr float MaxProbeDrop = 800.f;
	/** Capsule centres are placed this far above the floor so the first floor check finds it. */
	static constexpr float FloorGap = 2.f;
	/** Step between the probes that walk out to an edge, cm. */
	static constexpr float EdgeStep = 5.f;
	/** How far below the feet the drop beyond an edge is measured, cm. */
	static constexpr float EdgeProbeDepth = 1000.f;
	/** A floor within this of the feet is still the floor being stood on, cm. */
	static constexpr float FloorTolerance = 20.f;
	/** The drop to hang hops this far over the lip on its way over, cm. */
	static constexpr float DropHopClearance = 30.f;
	/** A ledge within this of the one just let go of is the same ledge, cm. */
	static constexpr float SameLedgeTolerance = 40.f;
}

UParkourComponent::UParkourComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	// The Game Animation Sample's clips (copied by import_gasp.py). Times from the root motion:
	// the hurdle clears 1 m between 0.1 and 1.0 s, the mantle rises 1 m and steps on between 0.3 and
	// 1.4 s, and the 2.5 m climb has both hands on the edge at 0.75 s and stands on top by 2.1 s.
	VaultClip = CastleParkour::MakeClip(TEXT("Hurdle/M_Neutral_Traversal_Hurdle_1_0_stand_F_V2_Lfoot"), 0.1f, 1.0f);
	MantleClip = CastleParkour::MakeClip(TEXT("Mantle/M_Neutral_Traversal_Mantle_1_0_stand_F_Lfoot"), 0.3f, 1.4f);
	GrabClip = CastleParkour::MakeClip(TEXT("Climb/M_Neutral_Traversal_Climb_Start_2_5_stand_F_Lfoot"), 0.3f, 0.75f);
	CatchClip = CastleParkour::MakeClip(TEXT("Climb/M_Neutral_Traversal_Climb_Start_2_5_stand_F_Lfoot"), 0.65f, 0.75f);
	ClimbClip = CastleParkour::MakeClip(TEXT("Climb/M_Neutral_Traversal_Climb_Start_2_5_stand_F_Lfoot"), 0.75f, 2.1f);
}

void UParkourComponent::BeginPlay()
{
	Super::BeginPlay();
	SampleTraversal = CastleGaspTraversal::FindTraversalLogic(GetOwner());
}

void UParkourComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RestoreRootMotionMode();
	Super::EndPlay(EndPlayReason);
}

ACharacter* UParkourComponent::GetCharacter() const
{
	return Cast<ACharacter>(GetOwner());
}

UCharacterMovementComponent* UParkourComponent::GetMovement() const
{
	const ACharacter* Character = GetCharacter();
	return Character ? Character->GetCharacterMovement() : nullptr;
}

bool UParkourComponent::IsSampleTraversalActive() const
{
	return CastleGaspTraversal::IsDoingTraversal(SampleTraversal.Get());
}

float UParkourComponent::GetMoveSeconds(ECastleParkourMove Move) const
{
	switch (Move)
	{
	case ECastleParkourMove::Vault: return VaultSeconds;
	case ECastleParkourMove::Mantle: return MantleSeconds;
	case ECastleParkourMove::LedgeGrab: return GrabSeconds;
	case ECastleParkourMove::Climb: return ClimbSeconds;
	case ECastleParkourMove::DropToHang: return DropToHangSeconds;
	default: return 0.f;
	}
}

// --- Choosing -----------------------------------------------------------------------------------

ECastleParkourMove UParkourComponent::ChooseMove(float Height, bool bClearBeyond, bool bStandingSurface) const
{
	if (Height < VaultMinHeight || Height > LedgeMaxHeight)
	{
		return ECastleParkourMove::None;
	}
	if (Height <= VaultMaxHeight)
	{
		if (bClearBeyond)
		{
			return ECastleParkourMove::Vault;
		}
		return bStandingSurface ? ECastleParkourMove::Mantle : ECastleParkourMove::None;
	}
	if (Height <= MantleMaxHeight)
	{
		return bStandingSurface ? ECastleParkourMove::Mantle : ECastleParkourMove::None;
	}
	return ECastleParkourMove::LedgeGrab;
}

ECastleParkourMove UParkourComponent::ChooseMoveFor(const FCastleParkourObstacle& Obstacle, bool bAuto) const
{
	if (!Obstacle.bFound)
	{
		return ECastleParkourMove::None;
	}
	const float MaxDrop = bAuto ? MaxAutoVaultDrop : MaxVaultDrop;
	const bool bClear = Obstacle.bClearBeyond && Obstacle.LandingDrop <= MaxDrop;
	return ChooseMove(Obstacle.Height, bClear, Obstacle.bStandingSurface);
}

// --- Detection ----------------------------------------------------------------------------------

bool UParkourComponent::TraceLine(const FVector& From, const FVector& To, FHitResult& OutHit) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CastleParkour), false, GetOwner());
	return World->LineTraceSingleByChannel(OutHit, From, To, ECC_Visibility, Params);
}

bool UParkourComponent::CapsuleFits(const FVector& Centre) const
{
	const ACharacter* Character = GetCharacter();
	const UWorld* World = GetWorld();
	if (!Character || !World)
	{
		return false;
	}
	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const float Radius = Capsule->GetScaledCapsuleRadius() - 2.f;
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight() - 2.f;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CastleParkourFit), false, Character);
	return !World->OverlapBlockingTestByChannel(Centre, FQuat::Identity, ECC_Pawn,
		FCollisionShape::MakeCapsule(Radius, HalfHeight), Params);
}

bool UParkourComponent::FindFrontFace(const FVector& Feet, const FVector& Forward, float Reach, float MaxHeight,
	FHitResult& OutHit) const
{
	bool bFound = false;
	for (float Height = CastleParkour::FaceProbeLowest; Height <= MaxHeight; Height += CastleParkour::FaceProbeStep)
	{
		const FVector From = Feet + FVector(0.f, 0.f, Height);
		FHitResult Hit;
		if (!TraceLine(From, From + Forward * Reach, Hit) || FMath::Abs(Hit.ImpactNormal.Z) > 0.5f)
		{
			continue;
		}
		if (!bFound || Hit.Distance < OutHit.Distance - 1.f)
		{
			OutHit = Hit;
			bFound = true;
		}
	}
	return bFound;
}

bool UParkourComponent::FindTop(const FHitResult& Face, const FVector& Normal, const FVector& Feet, float MaxHeight,
	FVector& OutTop) const
{
	const FVector Inside = Face.ImpactPoint - Normal * CastleParkour::TopInset;
	FHitResult Top;
	const FVector From(Inside.X, Inside.Y, Feet.Z + MaxHeight + 50.f);
	const FVector To(Inside.X, Inside.Y, Face.ImpactPoint.Z - 1.f);
	if (!TraceLine(From, To, Top) || Top.ImpactNormal.Z < CastleParkour::MinFloorNormalZ)
	{
		return false;
	}

	// Open air above the edge: from the character's side over the top, 30 cm up. A wall that
	// carries on above the probe (the trace started inside it) fails here.
	FHitResult Above;
	const float AboveZ = Top.ImpactPoint.Z + 30.f;
	const FVector Outside = Face.ImpactPoint + Normal * 40.f;
	if (TraceLine(FVector(Outside.X, Outside.Y, AboveZ), FVector(Inside.X, Inside.Y, AboveZ), Above))
	{
		return false;
	}
	OutTop = Top.ImpactPoint;
	return true;
}

void UParkourComponent::ProbeBeyond(const FVector& Feet, FCastleParkourObstacle& Obstacle) const
{
	const FVector& N = Obstacle.WallNormal;
	const float TopZ = Obstacle.LedgePoint.Z;
	float BackEdge = 0.f;
	for (float Depth = CastleParkour::BeyondStep; Depth <= MaxVaultDepth; Depth += CastleParkour::BeyondStep)
	{
		const FVector Probe = Obstacle.LedgePoint - N * Depth;
		FHitResult Hit;
		const bool bHit = TraceLine(FVector(Probe.X, Probe.Y, TopZ + 30.f), FVector(Probe.X, Probe.Y, TopZ - 30.f), Hit);
		if (!bHit || Hit.ImpactPoint.Z < TopZ - 20.f)
		{
			BackEdge = Depth;
			break;
		}
	}
	if (BackEdge <= 0.f)
	{
		return;
	}
	Obstacle.Depth = BackEdge;

	const ACharacter* Character = GetCharacter();
	const float Radius = Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const float HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Land = Obstacle.LedgePoint - N * (BackEdge + Radius + CastleParkour::LandingClearance);
	FHitResult Floor;
	if (!TraceLine(FVector(Land.X, Land.Y, TopZ + 30.f), FVector(Land.X, Land.Y, Feet.Z - CastleParkour::MaxProbeDrop), Floor)
		|| Floor.ImpactNormal.Z < CastleParkour::MinFloorNormalZ
		|| !CapsuleFits(Floor.ImpactPoint + FVector(0.f, 0.f, HalfHeight + CastleParkour::FloorGap)))
	{
		return;
	}
	Obstacle.bClearBeyond = true;
	Obstacle.LandingPoint = Floor.ImpactPoint;
	Obstacle.LandingDrop = Feet.Z - Floor.ImpactPoint.Z;
}

void UParkourComponent::ProbeStanding(FCastleParkourObstacle& Obstacle) const
{
	const ACharacter* Character = GetCharacter();
	const float Radius = Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const float HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float TopZ = Obstacle.LedgePoint.Z;
	const FVector Stand = Obstacle.LedgePoint - Obstacle.WallNormal * (Radius + CastleParkour::StandInset);
	FHitResult Floor;
	const bool bOnTop = TraceLine(FVector(Stand.X, Stand.Y, TopZ + 30.f), FVector(Stand.X, Stand.Y, TopZ - 30.f), Floor)
		&& Floor.ImpactNormal.Z >= CastleParkour::MinFloorNormalZ
		&& FMath::Abs(Floor.ImpactPoint.Z - TopZ) <= 25.f
		&& CapsuleFits(Floor.ImpactPoint + FVector(0.f, 0.f, HalfHeight + CastleParkour::FloorGap));
	if (bOnTop)
	{
		Obstacle.bStandingSurface = true;
		Obstacle.StandPoint = Floor.ImpactPoint;
		return;
	}
	// A parapet: too thin to stand on, but the roof behind it is close below the top.
	if (Obstacle.bClearBeyond && Obstacle.LandingPoint.Z >= TopZ - ClimbOverMaxDrop)
	{
		Obstacle.bStandingSurface = true;
		Obstacle.StandPoint = Obstacle.LandingPoint;
	}
}

bool UParkourComponent::DetectObstacle(float MaxDistance, FCastleParkourObstacle& OutObstacle) const
{
	return DetectObstacleUpTo(MaxDistance, LedgeMaxHeight, OutObstacle);
}

bool UParkourComponent::DetectObstacleUpTo(float MaxDistance, float MaxHeight, FCastleParkourObstacle& OutObstacle) const
{
	OutObstacle = FCastleParkourObstacle();
	const ACharacter* Character = GetCharacter();
	if (!Character || !GetWorld())
	{
		return false;
	}
	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const FVector Feet = Character->GetActorLocation() - FVector(0.f, 0.f, Capsule->GetScaledCapsuleHalfHeight());
	const FVector Forward = Character->GetActorForwardVector().GetSafeNormal2D();

	FHitResult Face;
	if (!FindFrontFace(Feet, Forward, Radius + MaxDistance, MaxHeight, Face))
	{
		return false;
	}
	const FVector Normal = Face.ImpactNormal.GetSafeNormal2D();
	FVector Top;
	if (FVector::DotProduct(Normal, -Forward) < 0.5f || !FindTop(Face, Normal, Feet, MaxHeight, Top))
	{
		return false;
	}

	OutObstacle.bFound = true;
	OutObstacle.WallPoint = Face.ImpactPoint;
	OutObstacle.WallNormal = Normal;
	OutObstacle.LedgePoint = FVector(Face.ImpactPoint.X, Face.ImpactPoint.Y, Top.Z);
	OutObstacle.Height = Top.Z - Feet.Z;
	OutObstacle.Distance = FMath::Max(0.f, Face.Distance - Radius);
	OutObstacle.Actor = Face.GetActor();
	ProbeBeyond(Feet, OutObstacle);
	ProbeStanding(OutObstacle);
	return true;
}

// --- Starting -----------------------------------------------------------------------------------

bool UParkourComponent::TryParkour(bool bAuto)
{
	const UCharacterMovementComponent* Movement = GetMovement();
	if (IsBusy() || !Movement || !(Movement->IsMovingOnGround() || Movement->IsFalling()))
	{
		return false;
	}
	FCastleParkourObstacle Obstacle;
	if (!DetectObstacle(bAuto ? AutoTriggerDistance : ManualTriggerDistance, Obstacle))
	{
		return false;
	}
	const ECastleParkourMove Move = ChooseMoveFor(Obstacle, bAuto);
	if (Move == ECastleParkourMove::None)
	{
		return false;
	}
	LastObstacle = Obstacle;
	UE_LOG(LogCastle, Log, TEXT("%s: %s %s: %.0f cm high, %.0f cm away, depth %.0f, clear beyond %d (drop %.0f), stand %d, on %s"),
		*GetNameSafe(GetOwner()), bAuto ? TEXT("auto") : TEXT("jump"), *UEnum::GetValueAsString(Move), Obstacle.Height,
		Obstacle.Distance, Obstacle.Depth, Obstacle.bClearBeyond ? 1 : 0, Obstacle.LandingDrop,
		Obstacle.bStandingSurface ? 1 : 0, *GetNameSafe(Obstacle.Actor));

	const bool bSampleMove = Move == ECastleParkourMove::Vault || Move == ECastleParkourMove::Mantle;
	if (bSampleMove && Movement->IsMovingOnGround() && TryStartSampleTraversal())
	{
		LastMove = Move;
		LastRoute = ECastleParkourRoute::SampleTraversal;
		OnParkourStarted.Broadcast(Move, LastRoute);
		return true;
	}
	return StartMove(Move, Obstacle);
}

bool UParkourComponent::TryStartSampleTraversal()
{
	ACharacter* Character = GetCharacter();
	UActorComponent* Logic = SampleTraversal.Get();
	if (!bUseSampleTraversal || !Character || !Logic)
	{
		return false;
	}
	if (!bLoggedSampleApi)
	{
		bLoggedSampleApi = true;
		FString Report;
		CastleGaspTraversal::HasTraversalApi(Character->GetClass(), Logic->GetClass(), Report);
		UE_LOG(LogCastle, Log, TEXT("%s: sample traversal API: %s"), *GetNameSafe(Character), *Report);
	}
	const CastleGaspTraversal::FResult Result = CastleGaspTraversal::TryTraversal(Character, Logic);
	UE_LOG(LogCastle, Log, TEXT("%s: sample TryTraversalAction called=%d check failed=%d montage failed=%d"),
		*GetNameSafe(Character), Result.bCalled ? 1 : 0, Result.bCheckFailed ? 1 : 0, Result.bMontageFailed ? 1 : 0);
	return Result.Started();
}

FVector UParkourComponent::HangLocationFor(const FCastleParkourObstacle& Obstacle) const
{
	const ACharacter* Character = GetCharacter();
	const float HalfHeight = Character ? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.f;
	return Obstacle.LedgePoint + Obstacle.WallNormal * HangBackFromEdge
		+ FVector(0.f, 0.f, HalfHeight - HangBelowLedge);
}

bool UParkourComponent::StartMove(ECastleParkourMove Move, const FCastleParkourObstacle& Obstacle)
{
	const ACharacter* Character = GetCharacter();
	if (!Character || !Obstacle.bFound || IsBusy())
	{
		return false;
	}
	const FVector Lift(0.f, 0.f, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + CastleParkour::FloorGap);
	LastObstacle = Obstacle;
	switch (Move)
	{
	case ECastleParkourMove::Vault:
		return Obstacle.bClearBeyond
			&& BeginMove(Move, Obstacle.LandingPoint + Lift, Obstacle.Height + 10.f, VaultSeconds, VaultClip);
	case ECastleParkourMove::Mantle:
		return Obstacle.bStandingSurface && BeginMove(Move, Obstacle.StandPoint + Lift, -1.f, MantleSeconds, MantleClip);
	case ECastleParkourMove::LedgeGrab:
	{
		HangObstacle = Obstacle;
		const bool bFalling = GetMovement() && GetMovement()->IsFalling();
		return BeginMove(Move, HangLocationFor(Obstacle), -1.f, bFalling ? CatchSeconds : GrabSeconds,
			bFalling ? CatchClip : GrabClip);
	}
	case ECastleParkourMove::DropToHang:
	{
		// Over the lip (a parapet or a rail) with a hop that clears it, then down to the hang.
		HangObstacle = Obstacle;
		DropHop = FMath::Max(0.f, Obstacle.Height) + CastleParkour::DropHopClearance;
		return BeginMove(Move, HangLocationFor(Obstacle), DropHop, DropToHangSeconds, CatchClip);
	}
	default:
		return false;
	}
}

bool UParkourComponent::BeginMove(ECastleParkourMove Move, const FVector& End, float Rise, float Seconds,
	const FCastleParkourClip& Clip)
{
	ACharacter* Character = GetCharacter();
	UCharacterMovementComponent* Movement = GetMovement();
	if (!Character || !Movement)
	{
		return false;
	}
	if (Move != ECastleParkourMove::Climb)
	{
		Character->SetActorRotation(FRotator(0.f, (-LastObstacle.WallNormal).Rotation().Yaw, 0.f));
	}
	MoveStart = Character->GetActorLocation();
	MoveEnd = End;
	MoveRise = Rise < 0.f ? End.Z - MoveStart.Z : Rise;
	MoveSeconds = Seconds;
	MoveElapsed = 0.f;
	ActiveMove = Move;
	LastMove = Move;
	LastRoute = ECastleParkourRoute::Procedural;
	bHanging = false;

	Movement->StopMovementImmediately();
	Movement->SetMovementMode(MOVE_Flying);
	PlayClip(Clip, Seconds);
	UE_LOG(LogCastle, Log, TEXT("%s: parkour %s from %s to %s over %.2f s (%s)"), *GetNameSafe(Character),
		*UEnum::GetValueAsString(Move), *MoveStart.ToCompactString(), *MoveEnd.ToCompactString(), Seconds,
		MoveClip ? *MoveClip->GetName() : TEXT("plain arc"));
	OnParkourStarted.Broadcast(Move, LastRoute);
	return true;
}

// --- Running ------------------------------------------------------------------------------------

FVector UParkourComponent::ClipRootOffset(float Time) const
{
	const ACharacter* Character = GetCharacter();
	const UAnimSequenceBase* Clip = MoveClip.Get();
	if (!Clip || !Character || !Character->GetMesh() || Time <= ClipStart)
	{
		return FVector::ZeroVector;
	}
	const FTransform Delta = Clip->ExtractRootMotionFromRange(ClipStart, Time, FAnimExtractContext());
	// Root motion is in the mesh's space; the mesh is turned relative to the actor (-90 yaw).
	return Character->GetMesh()->GetRelativeRotation().RotateVector(Delta.GetTranslation());
}

void UParkourComponent::ComputeMoveShape(float Alpha, float& OutForward, float& OutUp) const
{
	const bool bArc = ActiveMove == ECastleParkourMove::Vault;
	if (ActiveMove == ECastleParkourMove::DropToHang)
	{
		// Out over the lip in the first 70% with a hop that peaks halfway there; the clip only
		// poses the hands. The drop to the hang comes in with the left-over Z.
		OutForward = FMath::SmoothStep(0.f, 0.7f, Alpha);
		OutUp = FMath::Sin(PI * FMath::Min(Alpha / 0.7f, 1.f));
		return;
	}
	if (MoveClip && ClipForwardTotal > 1.f && ClipPeakUp > 1.f)
	{
		const FVector Offset = ClipRootOffset(FMath::Lerp(ClipStart, ClipEnd, Alpha));
		OutForward = FMath::Clamp(Offset.X / ClipForwardTotal, -0.5f, 1.5f);
		OutUp = Offset.Z / ClipPeakUp;
		return;
	}
	// No clip (a plain character, or a test): over in an arc, or up first and then in.
	OutForward = bArc ? Alpha : FMath::SmoothStep(0.35f, 1.f, Alpha);
	OutUp = bArc ? FMath::Sin(PI * Alpha) : FMath::SmoothStep(0.f, 0.6f, Alpha);
}

FVector UParkourComponent::ComputeMoveLocation(float Alpha) const
{
	const float A = FMath::Clamp(Alpha, 0.f, 1.f);
	float Forward = 0.f;
	float Up = 0.f;
	ComputeMoveShape(A, Forward, Up);
	float EndUp = 0.f;
	float EndForward = 1.f;
	ComputeMoveShape(1.f, EndForward, EndUp);

	// Scaled so the peak of the shape is MoveRise; whatever the shape leaves over at the end (a
	// landing lower or higher than the start) is blended in linearly, so the end is exact.
	const FVector Horizontal(MoveEnd.X - MoveStart.X, MoveEnd.Y - MoveStart.Y, 0.f);
	const float LeftOverZ = (MoveEnd.Z - MoveStart.Z) - EndUp * MoveRise;
	const float ForwardScale = FMath::IsNearlyZero(EndForward) ? 1.f : 1.f / EndForward;
	return MoveStart + Horizontal * (Forward * ForwardScale)
		+ FVector(0.f, 0.f, Up * MoveRise + LeftOverZ * A);
}

void UParkourComponent::AdvanceMove(float DeltaSeconds)
{
	ACharacter* Character = GetCharacter();
	if (!IsPerformingMove() || !Character)
	{
		return;
	}
	MoveElapsed += DeltaSeconds;
	const float Alpha = MoveSeconds > 0.f ? MoveElapsed / MoveSeconds : 1.f;
	Character->SetActorLocation(ComputeMoveLocation(Alpha), false, nullptr, ETeleportType::None);
	if (UCharacterMovementComponent* Movement = GetMovement())
	{
		Movement->Velocity = FVector::ZeroVector;
	}
	if (Alpha >= 1.f)
	{
		FinishMove();
	}
}

void UParkourComponent::FinishMove()
{
	const ECastleParkourMove Move = ActiveMove;
	ActiveMove = ECastleParkourMove::None;
	if (Move == ECastleParkourMove::LedgeGrab || Move == ECastleParkourMove::DropToHang)
	{
		EnterHang();
		return;
	}
	StopClip(0.2f);
	RestoreRootMotionMode();
	if (UCharacterMovementComponent* Movement = GetMovement())
	{
		// Walking finds the floor the move ended on, or falls if there is none.
		Movement->SetMovementMode(MOVE_Walking);
	}
	UE_LOG(LogCastle, Log, TEXT("%s: parkour %s done at %s"), *GetNameSafe(GetOwner()), *UEnum::GetValueAsString(Move),
		GetOwner() ? *GetOwner()->GetActorLocation().ToCompactString() : TEXT("?"));
	OnParkourFinished.Broadcast(Move);
}

void UParkourComponent::EnterHang()
{
	bHanging = true;
	if (UCharacterMovementComponent* Movement = GetMovement())
	{
		Movement->StopMovementImmediately();
	}
	const ACharacter* Character = GetCharacter();
	UAnimInstance* Anim = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
	if (Anim && ActiveMontage)
	{
		Anim->Montage_Pause(ActiveMontage);
	}
	UE_LOG(LogCastle, Log, TEXT("%s: hanging at %s from a ledge %.0f cm up (climb %d)"), *GetNameSafe(GetOwner()),
		GetOwner() ? *GetOwner()->GetActorLocation().ToCompactString() : TEXT("?"), HangObstacle.LedgePoint.Z,
		HangObstacle.bStandingSurface ? 1 : 0);
}

bool UParkourComponent::ClimbFromHang()
{
	const ACharacter* Character = GetCharacter();
	if (!bHanging || !Character || !HangObstacle.bStandingSurface)
	{
		return false;
	}
	bHanging = false;
	const FVector Lift(0.f, 0.f, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + CastleParkour::FloorGap);
	return BeginMove(ECastleParkourMove::Climb, HangObstacle.StandPoint + Lift, -1.f, ClimbSeconds, ClimbClip);
}

bool UParkourComponent::DropFromHang()
{
	ACharacter* Character = GetCharacter();
	if (!bHanging || !Character)
	{
		return false;
	}
	bHanging = false;
	StopClip(0.2f);
	RestoreRootMotionMode();
	Character->SetActorLocation(Character->GetActorLocation() + HangObstacle.WallNormal * 15.f);
	if (UCharacterMovementComponent* Movement = GetMovement())
	{
		Movement->SetMovementMode(MOVE_Falling);
	}
	RegrabCooldown = DropRegrabSeconds;
	DroppedLedgePoint = HangObstacle.LedgePoint;
	bIgnoreDroppedLedge = true;
	UE_LOG(LogCastle, Log, TEXT("%s: dropped from the hang on a ledge at %.0f cm, feet at %.0f"), *GetNameSafe(Character),
		HangObstacle.LedgePoint.Z, Character->GetActorLocation().Z - Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	OnParkourFinished.Broadcast(ECastleParkourMove::LedgeGrab);
	return true;
}

bool UParkourComponent::TryCatchLedge()
{
	const UCharacterMovementComponent* Movement = GetMovement();
	if (IsBusy() || RegrabCooldown > 0.f || !Movement || !Movement->IsFalling() || Movement->Velocity.Z > 0.f)
	{
		return false;
	}
	FCastleParkourObstacle Obstacle;
	if (!DetectObstacleUpTo(CatchDistance, CatchMaxHeight, Obstacle) || Obstacle.Height < CatchMinHeight
		|| Obstacle.Height > CatchMaxHeight)
	{
		return false;
	}
	if (bIgnoreDroppedLedge && FMath::Abs(Obstacle.LedgePoint.Z - DroppedLedgePoint.Z) < CastleParkour::SameLedgeTolerance
		&& FVector::Dist2D(Obstacle.LedgePoint, DroppedLedgePoint) < 200.f)
	{
		return false;
	}
	UE_LOG(LogCastle, Log, TEXT("%s: falling, caught a ledge %.0f cm above the feet at %s on %s"), *GetNameSafe(GetOwner()),
		Obstacle.Height, *Obstacle.LedgePoint.ToCompactString(), *GetNameSafe(Obstacle.Actor));
	bIgnoreDroppedLedge = false;
	return StartMove(ECastleParkourMove::LedgeGrab, Obstacle);
}

bool UParkourComponent::FindDropEdge(const FVector& Direction, FCastleParkourObstacle& OutEdge) const
{
	OutEdge = FCastleParkourObstacle();
	const ACharacter* Character = GetCharacter();
	const FVector D = Direction.GetSafeNormal2D();
	if (!Character || !GetWorld() || D.IsNearlyZero())
	{
		return false;
	}
	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const FVector Feet = Character->GetActorLocation() - FVector(0.f, 0.f, Capsule->GetScaledCapsuleHalfHeight());
	const float ProbeTop = MaxLipHeight + 20.f;
	const float MaxK = DropToHangReach + MaxLipDepth + CastleParkour::EdgeStep;

	// Anything taller than a lip in the way is a wall, not an edge (a line trace that starts
	// inside a wall would miss it, so check across first).
	FHitResult Wall;
	const FVector High = Feet + FVector(0.f, 0.f, ProbeTop);
	if (TraceLine(High, High + D * (Radius + MaxK), Wall))
	{
		return false;
	}

	// Walk out from the capsule surface: floor, then maybe a lip, then the drop.
	float FloorZ = Feet.Z;
	float LipTopZ = 0.f;
	float LipStartK = -1.f;
	float EdgeK = -1.f;
	for (float K = 0.f; K <= MaxK; K += CastleParkour::EdgeStep)
	{
		const FVector P = Feet + D * (Radius + K);
		FHitResult Down;
		const bool bHit = TraceLine(FVector(P.X, P.Y, Feet.Z + ProbeTop),
			FVector(P.X, P.Y, Feet.Z - CastleParkour::EdgeProbeDepth), Down);
		const float H = bHit ? Down.ImpactPoint.Z - Feet.Z : -CastleParkour::EdgeProbeDepth;
		if (LipStartK < 0.f)
		{
			if (FMath::Abs(H) <= CastleParkour::FloorTolerance)
			{
				FloorZ = Down.ImpactPoint.Z;
				continue;
			}
			if (K > DropToHangReach)
			{
				return false;
			}
			if (H > CastleParkour::FloorTolerance)
			{
				LipStartK = K;
				LipTopZ = Down.ImpactPoint.Z;
				continue;
			}
			if (H < -DropToHangMinDrop)
			{
				EdgeK = K;
				break;
			}
			return false;   // a step down, not an edge
		}
		if (H >= LipTopZ - Feet.Z - CastleParkour::FloorTolerance)
		{
			if (K - LipStartK > MaxLipDepth)
			{
				return false;   // a raised floor, not a lip
			}
			LipTopZ = FMath::Max(LipTopZ, static_cast<float>(Down.ImpactPoint.Z));
			continue;
		}
		if (H < -DropToHangMinDrop)
		{
			EdgeK = K;
			break;
		}
		return false;   // a low wall with floor behind it
	}
	if (EdgeK < 0.f)
	{
		return false;
	}
	const float TopZ = LipStartK >= 0.f ? LipTopZ : FloorZ;

	// The outer face, from outside looking back in, just under the top.
	FHitResult Face;
	const FVector Outside = Feet + D * (Radius + EdgeK + 30.f);
	const FVector From(Outside.X, Outside.Y, TopZ - 3.f);
	if (!TraceLine(From, From - D * (Radius + EdgeK + 30.f), Face) || FMath::Abs(Face.ImpactNormal.Z) > 0.5f)
	{
		return false;
	}
	const FVector N = Face.ImpactNormal.GetSafeNormal2D();
	if (FVector::DotProduct(N, D) < 0.7f)
	{
		return false;
	}

	// The drop beyond, measured where the hanging capsule would fall.
	FHitResult Beyond;
	const FVector Below = Face.ImpactPoint + N * (Radius + 15.f);
	const bool bBeyond = TraceLine(FVector(Below.X, Below.Y, TopZ - 5.f),
		FVector(Below.X, Below.Y, Feet.Z - CastleParkour::EdgeProbeDepth), Beyond);
	const float Drop = bBeyond ? Feet.Z - Beyond.ImpactPoint.Z : CastleParkour::EdgeProbeDepth;
	if (Drop <= DropToHangMinDrop)
	{
		return false;
	}

	OutEdge.bFound = true;
	OutEdge.WallPoint = Face.ImpactPoint;
	OutEdge.WallNormal = N;
	OutEdge.LedgePoint = FVector(Face.ImpactPoint.X, Face.ImpactPoint.Y, TopZ);
	OutEdge.Height = TopZ - Feet.Z;
	OutEdge.Distance = LipStartK >= 0.f ? LipStartK : EdgeK;
	OutEdge.Depth = LipStartK >= 0.f ? EdgeK - LipStartK : 0.f;
	OutEdge.LandingDrop = Drop;
	OutEdge.LandingPoint = bBeyond ? FVector(Beyond.ImpactPoint) : Below - FVector(0.f, 0.f, CastleParkour::EdgeProbeDepth);
	OutEdge.bStandingSurface = true;
	OutEdge.StandPoint = Feet;
	OutEdge.Actor = Face.GetActor();
	return CapsuleFits(HangLocationFor(OutEdge));
}

bool UParkourComponent::TryDropToHang(const FVector& Direction, const FString& Trigger)
{
	const UCharacterMovementComponent* Movement = GetMovement();
	if (IsBusy() || !Movement || !Movement->IsMovingOnGround())
	{
		return false;
	}
	FCastleParkourObstacle Edge;
	if (!FindDropEdge(Direction, Edge))
	{
		return false;
	}
	UE_LOG(LogCastle, Log, TEXT("%s: %s: drop to hang over a %.0f cm lip %.0f cm away, %.0f cm drop beyond, on %s"),
		*GetNameSafe(GetOwner()), *Trigger, Edge.Height, Edge.Distance, Edge.LandingDrop, *GetNameSafe(Edge.Actor));
	return StartMove(ECastleParkourMove::DropToHang, Edge);
}

void UParkourComponent::TryAutoParkour()
{
	const ACastleCharacter* Castle = Cast<ACastleCharacter>(GetOwner());
	const UCharacterMovementComponent* Movement = GetMovement();
	if (!Castle || !Movement || !Castle->IsSprinting() || Castle->IsSliding() || Castle->bIsCrouched
		|| !Movement->IsMovingOnGround())
	{
		return;
	}
	const FVector Velocity = Movement->Velocity.GetSafeNormal2D();
	if (Movement->Velocity.Size2D() < AutoMinSpeed
		|| FVector::DotProduct(Velocity, Castle->GetActorForwardVector().GetSafeNormal2D()) < 0.7f)
	{
		return;
	}
	TryParkour(true);
}

void UParkourComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	RegrabCooldown = FMath::Max(0.f, RegrabCooldown - DeltaTime);
	if (bIgnoreDroppedLedge && !bHanging && !IsPerformingMove())
	{
		const UCharacterMovementComponent* Movement = GetMovement();
		if (Movement && !Movement->IsFalling())
		{
			bIgnoreDroppedLedge = false;
		}
	}
	if (IsPerformingMove())
	{
		AdvanceMove(DeltaTime);
		return;
	}
	if (bHanging)
	{
		if (UCharacterMovementComponent* Movement = GetMovement())
		{
			Movement->Velocity = FVector::ZeroVector;
		}
		return;
	}
	if (!TryCatchLedge())
	{
		TryAutoParkour();
	}
}

// --- Clips --------------------------------------------------------------------------------------

bool UParkourComponent::PlayClip(const FCastleParkourClip& Clip, float Seconds)
{
	MoveClip = nullptr;
	ClipForwardTotal = ClipPeakUp = ClipEndUp = 0.f;
	const ACharacter* Character = GetCharacter();
	UAnimInstance* Anim = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
	if (!Anim || Clip.Sequence.IsNull() || Clip.EndTime <= Clip.StartTime)
	{
		return false;
	}
	// Loaded synchronously: the move starts this frame and must not depend on the clip being resident.
	UAnimSequenceBase* Sequence = Clip.Sequence.LoadSynchronous();
	if (!Sequence)
	{
		UE_LOG(LogCastle, Warning, TEXT("%s: parkour clip %s does not load; using the plain arc"),
			*GetNameSafe(GetOwner()), *Clip.Sequence.ToString());
		return false;
	}
	MoveClip = Sequence;
	ClipStart = Clip.StartTime;
	ClipEnd = FMath::Min(Clip.EndTime, Sequence->GetPlayLength());
	for (int32 Step = 1; Step <= 16; ++Step)
	{
		const FVector Offset = ClipRootOffset(FMath::Lerp(ClipStart, ClipEnd, Step / 16.f));
		ClipPeakUp = FMath::Max(ClipPeakUp, Offset.Z);
		ClipForwardTotal = Offset.X;
		ClipEndUp = Offset.Z;
	}
	if (!bRootMotionOverridden)
	{
		SavedRootMotionMode = Anim->RootMotionMode;
		bRootMotionOverridden = true;
	}
	// The capsule follows our warped copy of the root motion; the clip only poses the body.
	Anim->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
	const float Rate = (ClipEnd - ClipStart) / FMath::Max(Seconds, 0.05f);
	ActiveMontage = Anim->PlaySlotAnimationAsDynamicMontage(Sequence, ClipSlot, 0.08f, 0.2f, Rate, 1, -1.f, ClipStart);
	return ActiveMontage != nullptr;
}

void UParkourComponent::StopClip(float BlendOutSeconds)
{
	const ACharacter* Character = GetCharacter();
	UAnimInstance* Anim = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
	if (Anim && ActiveMontage)
	{
		Anim->Montage_Stop(BlendOutSeconds, ActiveMontage);
	}
	ActiveMontage = nullptr;
}

void UParkourComponent::RestoreRootMotionMode()
{
	if (!bRootMotionOverridden)
	{
		return;
	}
	bRootMotionOverridden = false;
	const ACharacter* Character = GetCharacter();
	if (UAnimInstance* Anim = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr)
	{
		Anim->SetRootMotionMode(SavedRootMotionMode);
	}
}

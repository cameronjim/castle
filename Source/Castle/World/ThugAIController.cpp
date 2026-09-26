// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/ThugAIController.h"

#include "Castle.h"
#include "Combat/HealthComponent.h"
#include "Combat/WeaponComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Sight.h"
#include "TimerManager.h"

AThugAIController::AThugAIController()
{
	PrimaryActorTick.bCanEverTick = false;

	ThugPerception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("ThugPerception"));

	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	SightConfig->SightRadius = SightRadius;
	SightConfig->LoseSightRadius = LoseSightRadius;
	SightConfig->PeripheralVisionAngleDegrees = SightHalfAngleDegrees;
	SightConfig->SetMaxAge(0.f);
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;

	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));
	HearingConfig->HearingRange = HearingRange;
	HearingConfig->SetMaxAge(0.f);
	HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
	HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
	HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;

	ThugPerception->ConfigureSense(*SightConfig);
	ThugPerception->ConfigureSense(*HearingConfig);
	ThugPerception->SetDominantSense(SightConfig->GetSenseImplementation());

	SetPerceptionComponent(*ThugPerception);
}

void AThugAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (ThugPerception)
	{
		ThugPerception->OnTargetPerceptionUpdated.AddDynamic(
			this, &AThugAIController::HandleTargetPerceptionUpdated);
	}

	LastStimulusLocation = InPawn ? InPawn->GetActorLocation() : FVector::ZeroVector;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ThinkTimerHandle, this, &AThugAIController::TickThink, ThinkIntervalSeconds, true);
	}
}

void AThugAIController::OnUnPossess()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ThinkTimerHandle);
	}

	if (ThugPerception)
	{
		ThugPerception->OnTargetPerceptionUpdated.RemoveDynamic(
			this, &AThugAIController::HandleTargetPerceptionUpdated);
	}

	Super::OnUnPossess();
}

void AThugAIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ThinkTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

AThugCharacter* AThugAIController::GetThug() const
{
	return Cast<AThugCharacter>(GetPawn());
}

EThugAlertState AThugAIController::GetAlertState() const
{
	const AThugCharacter* Thug = GetThug();
	return Thug ? Thug->GetAlertState() : EThugAlertState::Calm;
}

void AThugAIController::SetState(EThugAlertState NewState)
{
	AThugCharacter* Thug = GetThug();
	if (!Thug || Thug->GetAlertState() == NewState)
	{
		return;
	}

	Thug->SetAlertState(NewState);

	// Each state starts from a clean slate; a re-entry must not inherit the last one's clocks.
	InvestigateElapsed = 0.f;
	SeenSeconds = 0.f;
	UnseenSeconds = 0.f;
	bPatrolWaiting = false;
	PatrolWaitElapsed = 0.f;

	if (NewState != EThugAlertState::Alerted)
	{
		StopMovement();
	}
}

void AThugAIController::HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!Actor || !Actor->IsA<APawn>() || !Cast<APawn>(Actor)->IsPlayerControlled())
	{
		return;
	}

	const bool bIsSight = Stimulus.Type == UAISense::GetSenseID<UAISense_Sight>();
	if (Stimulus.WasSuccessfullySensed())
	{
		TargetActor = Actor;
	}

	ReportStimulus(
		bIsSight ? EStimulusKind::Sight : EStimulusKind::Hearing,
		Stimulus.StimulusLocation,
		Stimulus.WasSuccessfullySensed(),
		Stimulus.Strength);
}

void AThugAIController::ReportStimulus(EStimulusKind Kind, FVector Location, bool bSuccessful, float Loudness)
{
	AThugCharacter* Thug = GetThug();
	if (!Thug)
	{
		return;
	}

	if (Kind == EStimulusKind::Sight)
	{
		bSeesTarget = bSuccessful;
		if (!bSuccessful)
		{
			return;
		}

		LastStimulusLocation = Location;
		UnseenSeconds = 0.f;

		// A glimpse makes him suspicious; only a sustained look confirms it (see Think).
		if (Thug->GetAlertState() == EThugAlertState::Calm)
		{
			SetState(EThugAlertState::Suspicious);
		}
		return;
	}

	if (!bSuccessful)
	{
		return;
	}

	LastStimulusLocation = Location;

	// A gunshot is unambiguous; footsteps are not.
	if (Loudness >= GunshotLoudnessThreshold)
	{
		SetState(EThugAlertState::Alerted);
		UnseenSeconds = 0.f;
		return;
	}

	if (Thug->GetAlertState() == EThugAlertState::Calm)
	{
		SetState(EThugAlertState::Suspicious);
	}
}

void AThugAIController::TickThink()
{
	Think(ThinkIntervalSeconds);
}

void AThugAIController::Think(float DeltaSeconds)
{
	AThugCharacter* Thug = GetThug();
	if (!Thug)
	{
		return;
	}

	if (const UHealthComponent* Health = Thug->GetHealthComponent())
	{
		if (!Health->IsAlive())
		{
			return;
		}
	}

	if (bSeesTarget)
	{
		SeenSeconds += DeltaSeconds;
		UnseenSeconds = 0.f;
	}
	else
	{
		SeenSeconds = 0.f;
		UnseenSeconds += DeltaSeconds;
	}

	// A sustained sighting is the only thing that promotes Suspicious to Alerted.
	if (bSeesTarget && SeenSeconds >= SightConfirmSeconds)
	{
		SetState(EThugAlertState::Alerted);
	}

	switch (Thug->GetAlertState())
	{
	case EThugAlertState::Calm:
		TickCalm(DeltaSeconds);
		break;
	case EThugAlertState::Suspicious:
		TickSuspicious(DeltaSeconds);
		break;
	case EThugAlertState::Alerted:
		TickAlerted(DeltaSeconds);
		break;
	}
}

void AThugAIController::TickCalm(float DeltaSeconds)
{
	const AThugCharacter* Thug = GetThug();
	if (!Thug || Thug->PatrolPoints.Num() == 0)
	{
		return;
	}

	if (bPatrolWaiting)
	{
		PatrolWaitElapsed += DeltaSeconds;
		if (PatrolWaitElapsed < Thug->PatrolWaitSeconds)
		{
			return;
		}

		bPatrolWaiting = false;
		PatrolWaitElapsed = 0.f;
		PatrolIndex = (PatrolIndex + 1) % Thug->PatrolPoints.Num();
	}

	AdvancePatrol();
}

void AThugAIController::AdvancePatrol()
{
	AThugCharacter* Thug = GetThug();
	if (!Thug || !Thug->PatrolPoints.IsValidIndex(PatrolIndex))
	{
		return;
	}

	AActor* Point = Thug->PatrolPoints[PatrolIndex];
	if (!IsValid(Point))
	{
		PatrolIndex = (PatrolIndex + 1) % FMath::Max(Thug->PatrolPoints.Num(), 1);
		return;
	}

	const EPathFollowingStatus::Type Status = GetMoveStatus();
	if (Status == EPathFollowingStatus::Moving)
	{
		return;
	}

	const float DistanceSq = FVector::DistSquared2D(Thug->GetActorLocation(), Point->GetActorLocation());
	if (DistanceSq <= FMath::Square(120.f))
	{
		bPatrolWaiting = true;
		PatrolWaitElapsed = 0.f;
		return;
	}

	RequestMoveToActor(Point, /*AcceptanceRadius=*/60.f);
}

void AThugAIController::RequestMoveToActor(AActor* Goal, float AcceptanceRadius)
{
	ReportMoveResult(MoveToActor(Goal, AcceptanceRadius), GetNameSafe(Goal));
}

void AThugAIController::RequestMoveToLocation(const FVector& Goal, float AcceptanceRadius)
{
	ReportMoveResult(MoveToLocation(Goal, AcceptanceRadius), Goal.ToCompactString());
}

void AThugAIController::ReportMoveResult(EPathFollowingRequestResult::Type Result, const FString& GoalDescription)
{
	if (Result != EPathFollowingRequestResult::Failed || bLoggedMoveFailure)
	{
		return;
	}
	bLoggedMoveFailure = true;

	const UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(GetWorld());
	const FString Reason = !NavSystem
		? TEXT("there is no navigation system in this world")
		: (NavSystem->GetDefaultNavDataInstance() == nullptr
			? TEXT("no navigation data exists - the level needs a NavMeshBoundsVolume and "
				"RuntimeGeneration=Dynamic in DefaultEngine.ini")
			: TEXT("the goal is off the navmesh or unreachable"));

	UE_LOG(LogCastle, Warning, TEXT("%s: cannot move to %s: %s."),
		*GetName(), *GoalDescription, *Reason);
}

void AThugAIController::TickSuspicious(float DeltaSeconds)
{
	if (GetMoveStatus() == EPathFollowingStatus::Moving)
	{
		return;
	}

	const AThugCharacter* Thug = GetThug();
	const float DistanceSq = Thug
		? FVector::DistSquared2D(Thug->GetActorLocation(), LastStimulusLocation)
		: 0.f;

	if (DistanceSq > FMath::Square(120.f))
	{
		RequestMoveToLocation(LastStimulusLocation, /*AcceptanceRadius=*/60.f);
		return;
	}

	// Standing on the noise with nothing to show for it.
	InvestigateElapsed += DeltaSeconds;
	if (InvestigateElapsed >= InvestigateSeconds)
	{
		SetState(EThugAlertState::Calm);
	}
}

void AThugAIController::TickAlerted(float DeltaSeconds)
{
	AThugCharacter* Thug = GetThug();
	if (!Thug)
	{
		return;
	}

	if (UnseenSeconds >= LoseTargetSeconds)
	{
		SetState(EThugAlertState::Suspicious);
		return;
	}

	// Alerted by a noise he has not yet put a body to: stay alerted, but nothing to shoot at.
	if (!IsValid(TargetActor))
	{
		return;
	}

	const FVector ToTarget = TargetActor->GetActorLocation() - Thug->GetActorLocation();

	// The weapon traces along the control rotation, so the aim always points at him. The body
	// does not follow it any more (see AThugCharacter's movement setup): it faces where it is
	// walking, and only squares up once he has stopped.
	FRotator FacingRotation = ToTarget.Rotation();
	FacingRotation.Roll = 0.f;
	SetControlRotation(FacingRotation);

	TimeSinceLastShot += DeltaSeconds;

	if (ToTarget.Size2D() > EngageRange)
	{
		RequestMoveToActor(TargetActor, /*AcceptanceRadius=*/EngageRange * 0.8f);
		return;
	}

	StopMovement();
	FaceTarget(Thug, ToTarget);

	if (TimeSinceLastShot >= FireInterval)
	{
		TimeSinceLastShot = 0.f;
		FireAtTarget();
	}
}

void AThugAIController::FaceTarget(APawn* Thug, const FVector& ToTarget)
{
	// Standing still, bOrientRotationToMovement has no direction to work from, so the turn is
	// made by hand. Yaw only: a thug does not lean over to shoot down at you.
	const FVector Flat = ToTarget.GetSafeNormal2D();
	if (!Thug || Flat.IsNearlyZero())
	{
		return;
	}

	Thug->SetActorRotation(FRotator(0.f, Flat.Rotation().Yaw, 0.f));
}

void AThugAIController::FireAtTarget()
{
	AThugCharacter* Thug = GetThug();
	UWeaponComponent* Weapon = Thug ? Thug->GetWeaponComponent() : nullptr;
	if (!Weapon || !IsValid(TargetActor))
	{
		return;
	}

	if (Weapon->CurrentAmmo <= 0)
	{
		Weapon->Reload();
		return;
	}

	// Scatter the shot by rotating the aim inside a cone; the weapon traces the control rotation.
	const FVector AimDirection = (TargetActor->GetActorLocation() - Thug->GetPawnViewLocation()).GetSafeNormal();
	const FVector Scattered = FMath::VRandCone(AimDirection, FMath::DegreesToRadians(AimSpreadDegrees));

	FRotator AimRotation = Scattered.Rotation();
	AimRotation.Roll = 0.f;
	SetControlRotation(AimRotation);

	Weapon->Fire();
}

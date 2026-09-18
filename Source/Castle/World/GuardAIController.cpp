// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/GuardAIController.h"

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

AGuardAIController::AGuardAIController()
{
	PrimaryActorTick.bCanEverTick = false;

	GuardPerception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("GuardPerception"));

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

	GuardPerception->ConfigureSense(*SightConfig);
	GuardPerception->ConfigureSense(*HearingConfig);
	GuardPerception->SetDominantSense(SightConfig->GetSenseImplementation());

	SetPerceptionComponent(*GuardPerception);
}

void AGuardAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (GuardPerception)
	{
		GuardPerception->OnTargetPerceptionUpdated.AddDynamic(
			this, &AGuardAIController::HandleTargetPerceptionUpdated);
	}

	LastStimulusLocation = InPawn ? InPawn->GetActorLocation() : FVector::ZeroVector;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ThinkTimerHandle, this, &AGuardAIController::TickThink, ThinkIntervalSeconds, true);
	}
}

void AGuardAIController::OnUnPossess()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ThinkTimerHandle);
	}

	if (GuardPerception)
	{
		GuardPerception->OnTargetPerceptionUpdated.RemoveDynamic(
			this, &AGuardAIController::HandleTargetPerceptionUpdated);
	}

	Super::OnUnPossess();
}

void AGuardAIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ThinkTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

AGuardCharacter* AGuardAIController::GetGuard() const
{
	return Cast<AGuardCharacter>(GetPawn());
}

EGuardAlertState AGuardAIController::GetAlertState() const
{
	const AGuardCharacter* Guard = GetGuard();
	return Guard ? Guard->GetAlertState() : EGuardAlertState::Calm;
}

void AGuardAIController::SetState(EGuardAlertState NewState)
{
	AGuardCharacter* Guard = GetGuard();
	if (!Guard || Guard->GetAlertState() == NewState)
	{
		return;
	}

	Guard->SetAlertState(NewState);

	// Each state starts from a clean slate; a re-entry must not inherit the last one's clocks.
	InvestigateElapsed = 0.f;
	SeenSeconds = 0.f;
	UnseenSeconds = 0.f;
	bPatrolWaiting = false;
	PatrolWaitElapsed = 0.f;

	if (NewState != EGuardAlertState::Alerted)
	{
		StopMovement();
	}
}

void AGuardAIController::HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
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

void AGuardAIController::ReportStimulus(EStimulusKind Kind, FVector Location, bool bSuccessful, float Loudness)
{
	AGuardCharacter* Guard = GetGuard();
	if (!Guard)
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
		if (Guard->GetAlertState() == EGuardAlertState::Calm)
		{
			SetState(EGuardAlertState::Suspicious);
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
		SetState(EGuardAlertState::Alerted);
		UnseenSeconds = 0.f;
		return;
	}

	if (Guard->GetAlertState() == EGuardAlertState::Calm)
	{
		SetState(EGuardAlertState::Suspicious);
	}
}

void AGuardAIController::TickThink()
{
	Think(ThinkIntervalSeconds);
}

void AGuardAIController::Think(float DeltaSeconds)
{
	AGuardCharacter* Guard = GetGuard();
	if (!Guard)
	{
		return;
	}

	if (const UHealthComponent* Health = Guard->GetHealthComponent())
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
		SetState(EGuardAlertState::Alerted);
	}

	switch (Guard->GetAlertState())
	{
	case EGuardAlertState::Calm:
		TickCalm(DeltaSeconds);
		break;
	case EGuardAlertState::Suspicious:
		TickSuspicious(DeltaSeconds);
		break;
	case EGuardAlertState::Alerted:
		TickAlerted(DeltaSeconds);
		break;
	}
}

void AGuardAIController::TickCalm(float DeltaSeconds)
{
	const AGuardCharacter* Guard = GetGuard();
	if (!Guard || Guard->PatrolPoints.Num() == 0)
	{
		return;
	}

	if (bPatrolWaiting)
	{
		PatrolWaitElapsed += DeltaSeconds;
		if (PatrolWaitElapsed < Guard->PatrolWaitSeconds)
		{
			return;
		}

		bPatrolWaiting = false;
		PatrolWaitElapsed = 0.f;
		PatrolIndex = (PatrolIndex + 1) % Guard->PatrolPoints.Num();
	}

	AdvancePatrol();
}

void AGuardAIController::AdvancePatrol()
{
	AGuardCharacter* Guard = GetGuard();
	if (!Guard || !Guard->PatrolPoints.IsValidIndex(PatrolIndex))
	{
		return;
	}

	AActor* Point = Guard->PatrolPoints[PatrolIndex];
	if (!IsValid(Point))
	{
		PatrolIndex = (PatrolIndex + 1) % FMath::Max(Guard->PatrolPoints.Num(), 1);
		return;
	}

	const EPathFollowingStatus::Type Status = GetMoveStatus();
	if (Status == EPathFollowingStatus::Moving)
	{
		return;
	}

	const float DistanceSq = FVector::DistSquared2D(Guard->GetActorLocation(), Point->GetActorLocation());
	if (DistanceSq <= FMath::Square(120.f))
	{
		bPatrolWaiting = true;
		PatrolWaitElapsed = 0.f;
		return;
	}

	RequestMoveToActor(Point, /*AcceptanceRadius=*/60.f);
}

void AGuardAIController::RequestMoveToActor(AActor* Goal, float AcceptanceRadius)
{
	ReportMoveResult(MoveToActor(Goal, AcceptanceRadius), GetNameSafe(Goal));
}

void AGuardAIController::RequestMoveToLocation(const FVector& Goal, float AcceptanceRadius)
{
	ReportMoveResult(MoveToLocation(Goal, AcceptanceRadius), Goal.ToCompactString());
}

void AGuardAIController::ReportMoveResult(EPathFollowingRequestResult::Type Result, const FString& GoalDescription)
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

void AGuardAIController::TickSuspicious(float DeltaSeconds)
{
	if (GetMoveStatus() == EPathFollowingStatus::Moving)
	{
		return;
	}

	const AGuardCharacter* Guard = GetGuard();
	const float DistanceSq = Guard
		? FVector::DistSquared2D(Guard->GetActorLocation(), LastStimulusLocation)
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
		SetState(EGuardAlertState::Calm);
	}
}

void AGuardAIController::TickAlerted(float DeltaSeconds)
{
	AGuardCharacter* Guard = GetGuard();
	if (!Guard)
	{
		return;
	}

	if (UnseenSeconds >= LoseTargetSeconds)
	{
		SetState(EGuardAlertState::Suspicious);
		return;
	}

	// Alerted by a noise he has not yet put a body to: stay alerted, but nothing to shoot at.
	if (!IsValid(TargetActor))
	{
		return;
	}

	const FVector ToTarget = TargetActor->GetActorLocation() - Guard->GetActorLocation();

	// Face him whether shooting or closing; the weapon traces along the control rotation.
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

	if (TimeSinceLastShot >= FireInterval)
	{
		TimeSinceLastShot = 0.f;
		FireAtTarget();
	}
}

void AGuardAIController::FireAtTarget()
{
	AGuardCharacter* Guard = GetGuard();
	UWeaponComponent* Weapon = Guard ? Guard->GetWeaponComponent() : nullptr;
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
	const FVector AimDirection = (TargetActor->GetActorLocation() - Guard->GetPawnViewLocation()).GetSafeNormal();
	const FVector Scattered = FMath::VRandCone(AimDirection, FMath::DegreesToRadians(AimSpreadDegrees));

	FRotator AimRotation = Scattered.Rotation();
	AimRotation.Roll = 0.f;
	SetControlRotation(AimRotation);

	Weapon->Fire();
}

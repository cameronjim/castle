// Copyright Epic Games, Inc. All Rights Reserved.

#include "Mission/ObjectiveTriggerVolume.h"

#include "Castle.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Mission/MissionSubsystem.h"

AObjectiveTriggerVolume::AObjectiveTriggerVolume()
{
	PrimaryActorTick.bCanEverTick = false;
	bNetLoadOnClient = true;
}

void AObjectiveTriggerVolume::BeginPlay()
{
	Super::BeginPlay();

	OnActorBeginOverlap.AddDynamic(this, &AObjectiveTriggerVolume::HandleActorBeginOverlap);
}

void AObjectiveTriggerVolume::HandleActorBeginOverlap(AActor* /*OverlappedActor*/, AActor* OtherActor)
{
	if (!OtherActor || (bOnlyOnce && bHasTriggered))
	{
		return;
	}

	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (bPlayerOnly && (!Pawn || !Pawn->IsPlayerControlled()))
	{
		return;
	}

	UMissionSubsystem* MissionSubsystem = UMissionSubsystem::Get(this);
	if (!MissionSubsystem)
	{
		return;
	}

	if (MissionSubsystem->CompleteObjectiveByTag(ObjectiveTag))
	{
		bHasTriggered = true;
		OnObjectiveTriggered(OtherActor);

		UE_LOG(LogCastle, Verbose, TEXT("Objective '%s' completed by overlap with %s."),
			*ObjectiveTag.ToString(), *OtherActor->GetName());
	}
}

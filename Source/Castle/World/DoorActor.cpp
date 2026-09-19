// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/DoorActor.h"

#include "Castle.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Mission/MissionSubsystem.h"
#include "Player/CastleCharacter.h"

ADoorActor::ADoorActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// A bare scene component is the root, so the actor's origin is the middle of the threshold
	// and both meshes keep the relative heights authored here. See the DoorRoot comment.
	DoorRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DoorRoot"));
	SetRootComponent(DoorRoot);
	DoorRoot->SetMobility(EComponentMobility::Static);

	FrameMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FrameMesh"));
	FrameMesh->SetupAttachment(DoorRoot);
	FrameMesh->SetMobility(EComponentMobility::Static);
	FrameMesh->SetRelativeLocation(FVector(0.f, 0.f, FrameHeight * 0.5f));
	// Scenery only. A frame wide enough to be a surround is also wide enough to plug the
	// doorway, and a blocking one swallowed the interaction sweep and the player with it.
	FrameMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FrameMesh->SetCollisionResponseToAllChannels(ECR_Ignore);

	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(DoorRoot);
	// Movable and blocking: the leaf is what stops the player walking through, and the whole
	// slab from the floor to LeafHeight is what the interaction sweep finds.
	DoorMesh->SetMobility(EComponentMobility::Movable);
	DoorMesh->SetRelativeLocation(FVector(0.f, 0.f, LeafHeight * 0.5f));
	DoorMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	DoorMesh->SetCollisionResponseToAllChannels(ECR_Block);
}

FVector ADoorActor::GetLeafWorldCentre() const
{
	return DoorMesh ? DoorMesh->GetComponentLocation() : GetActorLocation();
}

void ADoorActor::BeginPlay()
{
	Super::BeginPlay();

	if (DoorMesh)
	{
		ClosedRelativeLocation = DoorMesh->GetRelativeLocation();
		ClosedRelativeRotation = DoorMesh->GetRelativeRotation();
	}
}

bool ADoorActor::CanInteract_Implementation(AActor* Interactor)  const
{
	// A locked door still answers, so the player is told what it wants; an open one is done.
	return !bOpen && Cast<ACastleCharacter>(Interactor) != nullptr;
}

FText ADoorActor::GetInteractPrompt_Implementation() const
{
	if (bOpen)
	{
		return FText::GetEmpty();
	}

	return bLocked
		? NSLOCTEXT("Castle", "DoorLocked", "Locked: keycard required")
		: NSLOCTEXT("Castle", "DoorOpen", "[E] Open");
}

void ADoorActor::Interact_Implementation(AActor* Interactor)
{
	TryOpen(Interactor);
}

bool ADoorActor::IsUnlockedFor(const AActor* Interactor) const
{
	if (!bLocked)
	{
		return true;
	}

	const ACastleCharacter* Character = Cast<ACastleCharacter>(Interactor);
	return Character && Character->HasKeycard(RequiredKeycardId);
}

bool ADoorActor::TryOpen(AActor* Interactor)
{
	if (!IsUnlockedFor(Interactor))
	{
		UE_LOG(LogCastle, Verbose, TEXT("%s: refused, '%s' keycard missing."),
			*GetName(), *RequiredKeycardId.ToString());
		OnOpenRefused(Interactor);
		return false;
	}

	return OpenNow(Interactor);
}

bool ADoorActor::OpenNow(AActor* Interactor)
{
	if (bOpen || bAnimating)
	{
		return false;
	}

	bOpen = true;
	bAnimating = true;
	SetActorTickEnabled(true);

	// Once open the leaf is scenery; stop it blocking the corridor it just cleared.
	if (DoorMesh)
	{
		DoorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (!CompletesObjectiveId.IsNone())
	{
		if (UMissionSubsystem* Missions = UMissionSubsystem::Get(this))
		{
			Missions->CompleteObjective(CompletesObjectiveId);
		}
	}

	UE_LOG(LogCastle, Log, TEXT("%s opened by %s."), *GetName(), *GetNameSafe(Interactor));
	OnDoorOpened.Broadcast(Interactor);
	return true;
}

void ADoorActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bAnimating)
	{
		SetActorTickEnabled(false);
		return;
	}

	OpenAlpha = FMath::Clamp(OpenAlpha + DeltaSeconds / FMath::Max(OpenSeconds, KINDA_SMALL_NUMBER), 0.f, 1.f);
	ApplyOpenAlpha();

	if (OpenAlpha >= 1.f)
	{
		bAnimating = false;
		SetActorTickEnabled(false);
	}
}

void ADoorActor::ApplyOpenAlpha()
{
	if (!DoorMesh)
	{
		return;
	}

	if (Motion == EDoorMotion::Swing)
	{
		FRotator Rotation = ClosedRelativeRotation;
		Rotation.Yaw += OpenYawDegrees * OpenAlpha;
		DoorMesh->SetRelativeRotation(Rotation);
		return;
	}

	DoorMesh->SetRelativeLocation(ClosedRelativeLocation + FVector(0.f, SlideDistance * OpenAlpha, 0.f));
}

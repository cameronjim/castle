// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/InteractionComponent.h"

#include "Castle.h"
#include "CastlePlayerController.h"
#include "CollisionQueryParams.h"
#include "Combat/TakedownComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "UI/CastleHudWidget.h"
#include "World/Interactable.h"

UInteractionComponent::UInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	// So the first frame already has a prompt rather than a blank one.
	RefreshFocus();
}

void UInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TimeSinceRefresh += DeltaTime;
	if (TimeSinceRefresh < RefreshSeconds)
	{
		return;
	}

	TimeSinceRefresh = 0.f;
	RefreshFocus();
}

void UInteractionComponent::GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	OutLocation = FVector::ZeroVector;
	OutRotation = FRotator::ZeroRotator;

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (const APawn* Pawn = Cast<APawn>(Owner))
	{
		if (AController* OwnerController = Pawn->GetController())
		{
			OwnerController->GetPlayerViewPoint(OutLocation, OutRotation);
			return;
		}
	}

	Owner->GetActorEyesViewPoint(OutLocation, OutRotation);
}

AActor* UInteractionComponent::TraceForInteractable() const
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner)
	{
		return nullptr;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	GetViewPoint(ViewLocation, ViewRotation);

	const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * InteractRange;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CastleInteractionFocus), /*bTraceComplex=*/false, Owner);
	QueryParams.AddIgnoredActor(Owner);

	// A sphere sweep rather than a line: doors and small pickups are easy to miss otherwise.
	TArray<FHitResult> Hits;
	World->SweepMultiByChannel(
		Hits, ViewLocation, TraceEnd, FQuat::Identity, ECC_Visibility,
		FCollisionShape::MakeSphere(TraceRadius), QueryParams);

	for (const FHitResult& Hit : Hits)
	{
		AActor* Candidate = Hit.GetActor();
		if (!IsValid(Candidate) || !Candidate->GetClass()->ImplementsInterface(UInteractable::StaticClass()))
		{
			continue;
		}

		if (!IInteractable::Execute_CanInteract(Candidate, Owner))
		{
			continue;
		}

		// Hits come back sorted along the sweep, so the first valid one is the nearest.
		return Candidate;
	}

	return nullptr;
}

void UInteractionComponent::RefreshFocus()
{
	SetFocusedActor(TraceForInteractable());
	UpdateHudPrompt();
}

void UInteractionComponent::SetFocusedActor(AActor* NewFocus)
{
	if (FocusedActor == NewFocus)
	{
		return;
	}

	FocusedActor = NewFocus;
	OnFocusedInteractableChanged.Broadcast(NewFocus);
}

FText UInteractionComponent::GetFocusPrompt() const
{
	if (!IsValid(FocusedActor))
	{
		return FText::GetEmpty();
	}

	return IInteractable::Execute_GetInteractPrompt(FocusedActor);
}

void UInteractionComponent::UpdateHudPrompt()
{
	if (!bDrivesHudPrompt)
	{
		return;
	}

	UCastleHudWidget* Hud = ACastlePlayerController::GetCastleHudFor(this);
	if (!Hud)
	{
		return;
	}

	FText Prompt = GetFocusPrompt();

	// Nothing to interact with: offer the takedown instead, so one line covers both verbs.
	if (Prompt.IsEmpty())
	{
		UTakedownComponent* Takedown = GetOwner() ? GetOwner()->FindComponentByClass<UTakedownComponent>() : nullptr;
		if (Takedown && Takedown->FindTakedownTarget())
		{
			Prompt = NSLOCTEXT("Castle", "TakedownPrompt", "[F] Take down");
		}
	}

	Hud->SetPrompt(Prompt);
}

bool UInteractionComponent::TryInteract()
{
	// The player may have turned since the last refresh tick.
	RefreshFocus();

	AActor* Target = FocusedActor;
	AActor* Owner = GetOwner();
	if (!IsValid(Target) || !Owner)
	{
		return false;
	}

	IInteractable::Execute_Interact(Target, Owner);
	UE_LOG(LogCastle, Verbose, TEXT("%s interacted with %s."), *GetNameSafe(Owner), *Target->GetName());

	// Interacting usually changes the prompt (a door that is now open, a pickup that is gone).
	RefreshFocus();
	return true;
}

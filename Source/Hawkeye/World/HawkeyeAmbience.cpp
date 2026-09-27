// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/HawkeyeAmbience.h"

#include "Hawkeye.h"
#include "Audio/HawkeyeAudioMath.h"
#include "Audio/HawkeyeAudioSubsystem.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

AHawkeyeAmbience::AHawkeyeAmbience()
{
	PrimaryActorTick.bCanEverTick = true;
	// The main menu pauses the district under it on a fresh boot; the city still hums behind it.
	PrimaryActorTick.bTickEvenWhenPaused = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AHawkeyeAmbience::BeginPlay()
{
	Super::BeginPlay();
	bBedsStarted = false;
}

void AHawkeyeAmbience::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UHawkeyeAudioSubsystem::StopLoop(Wind, TEXT("ambience wind"));
	UHawkeyeAudioSubsystem::StopLoop(Street, TEXT("ambience street"));
	Super::EndPlay(EndPlayReason);
}

void AHawkeyeAmbience::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bBedsStarted)
	{
		bBedsStarted = true;
		// Non-spatial beds: attached for their lifetime, but the sounds carry no attenuation.
		Wind = UHawkeyeAudioSubsystem::PlayAttached(WindSound, RootComponent, TEXT("ambience wind"));
		Street = UHawkeyeAudioSubsystem::PlayAttached(StreetSound, RootComponent, TEXT("ambience street"));
		SinceUpdate = UpdateSeconds;
	}
	SinceUpdate += DeltaSeconds;
	if (SinceUpdate >= UpdateSeconds)
	{
		UpdateMix();
	}
}

void AHawkeyeAmbience::UpdateMix()
{
	SinceUpdate = 0.f;
	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn)
	{
		return;
	}
	ListenerHeight = Pawn->GetActorLocation().Z - GetActorLocation().Z;
	float WindVolume = 1.f;
	float StreetVolume = 1.f;
	HawkeyeAudioMath::ComputeAmbienceMix(ListenerHeight, WindVolume, StreetVolume);
	if (Wind)
	{
		Wind->AdjustVolume(FadeSeconds, WindVolume);
	}
	if (Street)
	{
		Street->AdjustVolume(FadeSeconds, StreetVolume);
	}
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Audio/HawkeyeAudioSubsystem.h"

#include "Hawkeye.h"
#include "HawkeyePlayerController.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

UHawkeyeAudioSubsystem* UHawkeyeAudioSubsystem::Find(const UObject* WorldContext)
{
	UWorld* World = (GEngine && WorldContext)
		? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	return World ? World->GetSubsystem<UHawkeyeAudioSubsystem>() : nullptr;
}

USoundBase* UHawkeyeAudioSubsystem::Resolve(const TSoftObjectPtr<USoundBase>& Sound)
{
	if (Sound.IsNull())
	{
		return nullptr;
	}
	USoundBase* Loaded = Sound.Get();
	if (!Loaded)
	{
		Loaded = Sound.LoadSynchronous();
		if (!Loaded)
		{
			UE_LOG(LogHawkeye, Warning, TEXT("%s: sound %s is missing; run Tools\\create-content.ps1."), *GetName(),
				*Sound.ToString());
			return nullptr;
		}
	}
	Resident.AddUnique(Loaded);
	return Loaded;
}

void UHawkeyeAudioSubsystem::NoteTrigger(
	const TCHAR* Event, const USoundBase* Sound, const UObject* Context, const FVector& Location)
{
	++TriggerCount;
	UE_LOG(LogHawkeye, Verbose, TEXT("Sound: %s %s at %s (%s, #%d)."), Event ? Event : TEXT("?"), *GetNameSafe(Sound),
		*Location.ToCompactString(), *GetNameSafe(Context), TriggerCount);
}

UAudioComponent* UHawkeyeAudioSubsystem::PlayAt(const UObject* WorldContext, const TSoftObjectPtr<USoundBase>& Sound,
	const FVector& Location, const TCHAR* Event, float VolumeMultiplier)
{
	UHawkeyeAudioSubsystem* Audio = Sound.IsNull() ? nullptr : Find(WorldContext);
	USoundBase* Resolved = Audio ? Audio->Resolve(Sound) : nullptr;
	if (!Resolved)
	{
		return nullptr;
	}
	Audio->NoteTrigger(Event, Resolved, WorldContext, Location);
	return UGameplayStatics::SpawnSoundAtLocation(WorldContext, Resolved, Location, FRotator::ZeroRotator,
		VolumeMultiplier);
}

UAudioComponent* UHawkeyeAudioSubsystem::PlayAttached(const TSoftObjectPtr<USoundBase>& Sound,
	USceneComponent* AttachTo, const TCHAR* Event, float VolumeMultiplier)
{
	UHawkeyeAudioSubsystem* Audio = (Sound.IsNull() || !AttachTo) ? nullptr : Find(AttachTo);
	USoundBase* Resolved = Audio ? Audio->Resolve(Sound) : nullptr;
	if (!Resolved)
	{
		return nullptr;
	}
	Audio->NoteTrigger(Event, Resolved, AttachTo->GetOwner(), AttachTo->GetComponentLocation());
	return UGameplayStatics::SpawnSoundAttached(Resolved, AttachTo, NAME_None, FVector::ZeroVector,
		FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, /*bStopWhenAttachedToDestroyed=*/true,
		VolumeMultiplier);
}

UAudioComponent* UHawkeyeAudioSubsystem::Play2D(const UObject* WorldContext, const TSoftObjectPtr<USoundBase>& Sound,
	const TCHAR* Event, float VolumeMultiplier)
{
	UHawkeyeAudioSubsystem* Audio = Sound.IsNull() ? nullptr : Find(WorldContext);
	USoundBase* Resolved = Audio ? Audio->Resolve(Sound) : nullptr;
	if (!Resolved)
	{
		return nullptr;
	}
	Audio->NoteTrigger(Event, Resolved, WorldContext, FVector::ZeroVector);
	return UGameplayStatics::SpawnSound2D(WorldContext, Resolved, VolumeMultiplier);
}

void UHawkeyeAudioSubsystem::PlayUI(const UObject* WorldContext, EHawkeyeUISound Sound)
{
	UWorld* World = (GEngine && WorldContext)
		? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	if (AHawkeyePlayerController* PC = World ? Cast<AHawkeyePlayerController>(World->GetFirstPlayerController()) : nullptr)
	{
		PC->PlayUISound(Sound);
	}
}

void UHawkeyeAudioSubsystem::StopLoop(TObjectPtr<UAudioComponent>& Loop, const TCHAR* Event)
{
	if (IsValid(Loop))
	{
		UE_LOG(LogHawkeye, Verbose, TEXT("Sound: %s stopped %s (%s)."), Event ? Event : TEXT("?"),
			*GetNameSafe(Loop->Sound), *GetNameSafe(Loop->GetOwner()));
		Loop->Stop();
	}
	Loop = nullptr;
}

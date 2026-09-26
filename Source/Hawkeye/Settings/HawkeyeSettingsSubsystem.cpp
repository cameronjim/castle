// Copyright Epic Games, Inc. All Rights Reserved.

#include "Settings/HawkeyeSettingsSubsystem.h"

#include "Hawkeye.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Settings/HawkeyeSettingsSave.h"

const TCHAR* UHawkeyeSettingsSubsystem::DefaultSlotName = TEXT("HawkeyeSettings");

UHawkeyeSettingsSubsystem* UHawkeyeSettingsSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UHawkeyeSettingsSubsystem>() : nullptr;
}

void UHawkeyeSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Load();
}

float UHawkeyeSettingsSubsystem::ClampLookSensitivity(float Value)
{
	return FMath::Clamp(Value, MinLookSensitivity, MaxLookSensitivity);
}

float UHawkeyeSettingsSubsystem::ClampStickSensitivity(float Value)
{
	return FMath::Clamp(Value, MinStickSensitivity, MaxStickSensitivity);
}

FString UHawkeyeSettingsSubsystem::GetSlotName() const
{
	return SlotNameOverride.IsEmpty() ? FString(DefaultSlotName) : SlotNameOverride;
}

void UHawkeyeSettingsSubsystem::SetLookSensitivity(float NewSensitivity)
{
	const float Clamped = ClampLookSensitivity(NewSensitivity);

	// A slider fires on every pixel of drag; a value that changes nothing must not write the
	// file or wake every listener.
	if (FMath::IsNearlyEqual(Clamped, Settings.LookSensitivity, UE_KINDA_SMALL_NUMBER))
	{
		return;
	}

	Settings.LookSensitivity = Clamped;
	Save();
	OnSettingsChanged.Broadcast(Settings);
}

void UHawkeyeSettingsSubsystem::SetStickSensitivity(float NewSensitivity)
{
	const float Clamped = ClampStickSensitivity(NewSensitivity);

	if (FMath::IsNearlyEqual(Clamped, Settings.StickSensitivity, UE_KINDA_SMALL_NUMBER))
	{
		return;
	}

	Settings.StickSensitivity = Clamped;
	Save();
	OnSettingsChanged.Broadcast(Settings);
}

void UHawkeyeSettingsSubsystem::Load()
{
	const FString Slot = GetSlotName();

	if (!UGameplayStatics::DoesSaveGameExist(Slot, 0))
	{
		Settings = FHawkeyeSettings();
		return;
	}

	const UHawkeyeSettingsSave* Loaded = Cast<UHawkeyeSettingsSave>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
	if (!Loaded)
	{
		UE_LOG(LogHawkeye, Warning, TEXT("Settings slot %s could not be read; using defaults."), *Slot);
		Settings = FHawkeyeSettings();
		return;
	}

	if (Loaded->Settings.Version != FHawkeyeSettings::CurrentVersion)
	{
		UE_LOG(LogHawkeye, Warning,
			TEXT("Settings slot %s is version %d, this build writes %d; using defaults."),
			*Slot, Loaded->Settings.Version, FHawkeyeSettings::CurrentVersion);
		Settings = FHawkeyeSettings();
		return;
	}

	Settings = Loaded->Settings;
	Settings.LookSensitivity = ClampLookSensitivity(Settings.LookSensitivity);
	Settings.StickSensitivity = ClampStickSensitivity(Settings.StickSensitivity);
}

bool UHawkeyeSettingsSubsystem::Save() const
{
	UHawkeyeSettingsSave* SaveObject =
		Cast<UHawkeyeSettingsSave>(UGameplayStatics::CreateSaveGameObject(UHawkeyeSettingsSave::StaticClass()));
	if (!SaveObject)
	{
		UE_LOG(LogHawkeye, Error, TEXT("Could not create the settings save object."));
		return false;
	}

	SaveObject->Settings = Settings;
	SaveObject->Settings.Version = FHawkeyeSettings::CurrentVersion;

	const FString Slot = GetSlotName();
	if (!UGameplayStatics::SaveGameToSlot(SaveObject, Slot, 0))
	{
		UE_LOG(LogHawkeye, Error, TEXT("Could not write the settings slot %s."), *Slot);
		return false;
	}

	return true;
}

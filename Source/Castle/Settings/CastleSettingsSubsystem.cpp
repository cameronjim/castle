// Copyright Epic Games, Inc. All Rights Reserved.

#include "Settings/CastleSettingsSubsystem.h"

#include "Castle.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Settings/CastleSettingsSave.h"

const TCHAR* UCastleSettingsSubsystem::DefaultSlotName = TEXT("CastleSettings");

UCastleSettingsSubsystem* UCastleSettingsSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UCastleSettingsSubsystem>() : nullptr;
}

void UCastleSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Load();
}

float UCastleSettingsSubsystem::ClampLookSensitivity(float Value)
{
	return FMath::Clamp(Value, MinLookSensitivity, MaxLookSensitivity);
}

FString UCastleSettingsSubsystem::GetSlotName() const
{
	return SlotNameOverride.IsEmpty() ? FString(DefaultSlotName) : SlotNameOverride;
}

void UCastleSettingsSubsystem::SetLookSensitivity(float NewSensitivity)
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

void UCastleSettingsSubsystem::Load()
{
	const FString Slot = GetSlotName();

	if (!UGameplayStatics::DoesSaveGameExist(Slot, 0))
	{
		Settings = FCastleSettings();
		return;
	}

	const UCastleSettingsSave* Loaded = Cast<UCastleSettingsSave>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
	if (!Loaded)
	{
		UE_LOG(LogCastle, Warning, TEXT("Settings slot %s could not be read; using defaults."), *Slot);
		Settings = FCastleSettings();
		return;
	}

	if (Loaded->Settings.Version != FCastleSettings::CurrentVersion)
	{
		UE_LOG(LogCastle, Warning,
			TEXT("Settings slot %s is version %d, this build writes %d; using defaults."),
			*Slot, Loaded->Settings.Version, FCastleSettings::CurrentVersion);
		Settings = FCastleSettings();
		return;
	}

	Settings = Loaded->Settings;
	Settings.LookSensitivity = ClampLookSensitivity(Settings.LookSensitivity);
}

bool UCastleSettingsSubsystem::Save() const
{
	UCastleSettingsSave* SaveObject =
		Cast<UCastleSettingsSave>(UGameplayStatics::CreateSaveGameObject(UCastleSettingsSave::StaticClass()));
	if (!SaveObject)
	{
		UE_LOG(LogCastle, Error, TEXT("Could not create the settings save object."));
		return false;
	}

	SaveObject->Settings = Settings;
	SaveObject->Settings.Version = FCastleSettings::CurrentVersion;

	const FString Slot = GetSlotName();
	if (!UGameplayStatics::SaveGameToSlot(SaveObject, Slot, 0))
	{
		UE_LOG(LogCastle, Error, TEXT("Could not write the settings slot %s."), *Slot);
		return false;
	}

	return true;
}

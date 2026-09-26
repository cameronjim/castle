// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Settings/HawkeyeSettings.h"
#include "HawkeyeSettingsSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHawkeyeSettingsChangedSignature, FHawkeyeSettings, Settings);

/**
 * Owns the player's options and persists them. A game instance subsystem because settings
 * outlive a level: the pause menu, the pawn and anything else read the same live values
 * across travel.
 *
 * Deliberately free of UI. UHawkeyeSettingsWidget drives it; AHawkeyeCharacter listens to
 * OnSettingsChanged so a slider takes effect while the player is still holding it.
 */
UCLASS()
class HAWKEYE_API UHawkeyeSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** The subsystem for the world this object lives in, or nullptr outside a game instance. */
	static UHawkeyeSettingsSubsystem* Get(const UObject* WorldContextObject);

	//~ Begin USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	//~ End USubsystem interface

	/** Fires after every accepted change, with the whole struct. */
	UPROPERTY(BlueprintAssignable, Category = "Settings")
	FOnHawkeyeSettingsChangedSignature OnSettingsChanged;

	/** Slowest and fastest mouse the screen will offer. */
	static constexpr float MinLookSensitivity = 0.02f;
	static constexpr float MaxLookSensitivity = 1.0f;

	/** Slowest and fastest gamepad stick look the screen will offer. */
	static constexpr float MinStickSensitivity = 0.2f;
	static constexpr float MaxStickSensitivity = 3.0f;

	/** The slot settings live in, unless SlotNameOverride is set. */
	static const TCHAR* DefaultSlotName;

	UFUNCTION(BlueprintPure, Category = "Settings")
	float GetLookSensitivity() const { return Settings.LookSensitivity; }

	/** Clamps, stores, saves and broadcasts. A value that changes nothing does nothing. */
	UFUNCTION(BlueprintCallable, Category = "Settings")
	void SetLookSensitivity(float NewSensitivity);

	UFUNCTION(BlueprintPure, Category = "Settings")
	float GetStickSensitivity() const { return Settings.StickSensitivity; }

	/** Clamps, stores, saves and broadcasts. A value that changes nothing does nothing. */
	UFUNCTION(BlueprintCallable, Category = "Settings")
	void SetStickSensitivity(float NewSensitivity);

	UFUNCTION(BlueprintPure, Category = "Settings")
	FHawkeyeSettings GetSettings() const { return Settings; }

	/** Reads the slot. A missing file or a version mismatch leaves defaults in place. */
	UFUNCTION(BlueprintCallable, Category = "Settings")
	void Load();

	/** Writes the slot. Returns false when the platform refused the write. */
	UFUNCTION(BlueprintCallable, Category = "Settings")
	bool Save() const;

	UFUNCTION(BlueprintPure, Category = "Settings")
	static float ClampLookSensitivity(float Value);

	UFUNCTION(BlueprintPure, Category = "Settings")
	static float ClampStickSensitivity(float Value);

	/**
	 * Set by tests so the real player's settings are never touched. Empty in game.
	 */
	UPROPERTY(Transient)
	FString SlotNameOverride;

	UFUNCTION(BlueprintPure, Category = "Settings")
	FString GetSlotName() const;

private:
	UPROPERTY(Transient)
	FHawkeyeSettings Settings;
};

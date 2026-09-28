// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Settings/HawkeyeSettings.h"
#include "Subsystems/WorldSubsystem.h"
#include "TimeOfDaySubsystem.generated.h"

class AActor;
class APostProcessVolume;
class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class ULightComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMeshComponent;
class USkyLightComponent;

/**
 * What one time of day asks of the district, relative to the night the generator builds
 * (Tools/Editor/generate_city.py, ensure_lighting_core). One row per EHawkeyeTimeOfDay in
 * UTimeOfDaySubsystem::GetPreset. Night's row is bUseAuthored: the level's own values, untouched.
 *
 * A real cycle later would add rows (dusk, dawn) and blend two of them by the clock; the sun's angle
 * could then come from the engine's SunPosition plugin (USunPositionFunctionLibrary::GetSunPosition at
 * 40.73 N, 73.98 W). Two fixed states do not need it.
 */
USTRUCT(BlueprintType)
struct HAWKEYE_API FTimeOfDayPreset
{
	GENERATED_BODY()

	/** Night: write back exactly what the level had. Every other field is ignored. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TimeOfDay")
	bool bUseAuthored = false;

	/** The directional light's rotation (it shines along its forward vector). Pitch -35 is 35 degrees up. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TimeOfDay")
	FRotator SunRotation = FRotator::ZeroRotator;

	/** Directional light illuminance, lux. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TimeOfDay")
	float SunLux = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TimeOfDay")
	FLinearColor SunColor = FLinearColor::White;

	/** The sun disc the sky atmosphere draws, as a tint. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TimeOfDay")
	FLinearColor SunDiskColorScale = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TimeOfDay")
	float SkyLightIntensity = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TimeOfDay")
	FLinearColor SkyLightColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TimeOfDay")
	float FogDensity = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TimeOfDay")
	FLinearColor FogInscattering = FLinearColor::Black;

	/** cm from the camera before the fog starts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TimeOfDay")
	float FogStartDistance = 0.f;

	/**
	 * PP_Global's exposure bias. Exposure is pinned (auto exposure is off project-wide and PP_Global's
	 * min and max brightness are both 1.0 EV100), so this is the only exposure knob: the night's is 2.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TimeOfDay")
	float ExposureBias = 0.f;

	/** PP_Global's shadow and highlight gains: the night's purple and cream, or neutral. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TimeOfDay")
	FVector4 GainShadows = FVector4(1.f, 1.f, 1.f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TimeOfDay")
	FVector4 GainHighlights = FVector4(1.f, 1.f, 1.f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TimeOfDay")
	float Vignette = 0.f;

	/** City_NightSky, the star sphere. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TimeOfDay")
	bool bStars = false;

	/** Street lamps (tag CityLamp): their lights' intensity and their heads' glow, times this. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TimeOfDay")
	float LampScale = 1.f;

	/** The facades' lit windows (M_Facade's WindowGlow), times this. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TimeOfDay")
	float WindowGlowScale = 1.f;
};

/**
 * Lights the district for the time of day in force (UHawkeyeSettingsSubsystem::GetTimeOfDay): at world
 * begin play, and again whenever the setting or the hawkeye.TimeOfDay console override changes, without
 * a reload. Night is the level exactly as generated; Day is GetPreset(Day) applied on top of it.
 *
 * Finds the scene's pieces by the generator's labels (Moon, SkyLight, HeightFog, PP_Global) in the
 * editor, by class in a cooked build (labels are editor-only), the stars by tag CityNightSky, lamps by
 * tag CityLamp and facades by their material's WindowGlow parameter. Anything missing is skipped.
 * Actors tagged Interior are never touched, and a world without outdoor weather (an interior, a
 * flashback's playable scene) is left alone at begin play; only Apply called directly lights those.
 *
 * Before the first change it records what the level had, so going back to Night restores it exactly.
 */
UCLASS()
class HAWKEYE_API UTimeOfDaySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UTimeOfDaySubsystem* Get(const UObject* WorldContextObject);

	//~ Begin USubsystem / UWorldSubsystem interface
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	//~ End USubsystem / UWorldSubsystem interface

	/** Lights the world for State now. Records the authored night first if it has not yet. */
	UFUNCTION(BlueprintCallable, Category = "TimeOfDay")
	void Apply(EHawkeyeTimeOfDay State);

	/** Applies the time of day in force, if it is not the one already applied. */
	UFUNCTION(BlueprintCallable, Category = "TimeOfDay")
	void Refresh();

	/** What Apply last put in place (Night before the first call). */
	UFUNCTION(BlueprintPure, Category = "TimeOfDay")
	EHawkeyeTimeOfDay GetApplied() const { return Applied; }

	/** How many lamp lights, lamp heads and facade materials the last Apply changed (tests, the log). */
	int32 GetLampLightCount() const { return LampLights.Num(); }
	int32 GetLampGlowCount() const { return LampGlows.Num(); }
	int32 GetWindowMaterialCount() const { return WindowMaterials.Num(); }

	/** The table: what State asks for. Night's row says "the level's own values". */
	UFUNCTION(BlueprintPure, Category = "TimeOfDay")
	static FTimeOfDayPreset GetPreset(EHawkeyeTimeOfDay State);

	/** The console override, else the command line's -TimeOfDay=, else Stored. */
	static EHawkeyeTimeOfDay ResolveTimeOfDay(EHawkeyeTimeOfDay Stored);

	/** "Night" or "Day" (case does not matter) into OutState. False for anything else. */
	static bool ParseTimeOfDay(const FString& Text, EHawkeyeTimeOfDay& OutState);

	/** The player-facing name: "Night", "Day". */
	UFUNCTION(BlueprintPure, Category = "TimeOfDay")
	static FText GetTimeOfDayName(EHawkeyeTimeOfDay State);

	/**
	 * hawkeye.TimeOfDay's override (not saved), or unset to fall back to the command line and the
	 * setting. Relights every game world. The Settings row clears it when the player picks a value.
	 */
	static void SetConsoleOverride(TOptional<EHawkeyeTimeOfDay> Override);
	static TOptional<EHawkeyeTimeOfDay> GetConsoleOverride();

	/** Tests only: replaces what -TimeOfDay= parsed to, until the returned previous value is put back. */
	static TOptional<EHawkeyeTimeOfDay> SetCommandLineOverrideForTest(TOptional<EHawkeyeTimeOfDay> Override);

	/** What -TimeOfDay= in CommandLine names, if anything. Pure; the cached command line uses it. */
	static TOptional<EHawkeyeTimeOfDay> ParseCommandLine(const TCHAR* CommandLine);

private:
	/** Every piece the time of day moves, as the level had it. */
	struct FAuthored
	{
		FRotator SunRotation = FRotator::ZeroRotator;
		float SunLux = 0.f;
		FColor SunColor = FColor::White;
		FLinearColor SunDiskColorScale = FLinearColor::White;
		bool bSunUseTemperature = false;
		float SkyLightIntensity = 1.f;
		FColor SkyLightColor = FColor::White;
		float FogDensity = 0.f;
		FLinearColor FogInscattering = FLinearColor::Black;
		float FogStartDistance = 0.f;
		bool bOverrideExposureBias = false;
		float ExposureBias = 0.f;
		bool bOverrideGainShadows = false;
		FVector4 GainShadows = FVector4(1.f, 1.f, 1.f, 1.f);
		bool bOverrideGainHighlights = false;
		FVector4 GainHighlights = FVector4(1.f, 1.f, 1.f, 1.f);
		bool bOverrideVignette = false;
		float Vignette = 0.f;
		bool bStarsHidden = false;
	};

	/** A lamp head's glowing material slot and its glow as authored. */
	struct FLampGlow
	{
		TWeakObjectPtr<UMaterialInstanceDynamic> Material;
		float Intensity = 0.f;
	};

	/** One facade material: the MID that dims it, and every component slot it was swapped into. */
	struct FWindowMaterial
	{
		TWeakObjectPtr<UMaterialInterface> Authored;
		float Glow = 0.f;
		TArray<TPair<TWeakObjectPtr<UMeshComponent>, int32>> Slots;
	};

	/** Finds the pieces and records them as authored. Once per world. */
	void Capture();
	void CaptureLamps();
	void CaptureWindows();

	/** Writes the night back exactly. */
	void RestoreAuthored();

	void ApplyPreset(const FTimeOfDayPreset& Preset);

	UFUNCTION()
	void HandleSettingsChanged(FHawkeyeSettings NewSettings);

	/** The actor labelled Label (editor), else the first of Class not tagged Interior. */
	AActor* FindScenePiece(const TCHAR* Label, UClass* Class) const;

	bool bCaptured = false;
	EHawkeyeTimeOfDay Applied = EHawkeyeTimeOfDay::Night;
	FAuthored Authored;

	TWeakObjectPtr<UDirectionalLightComponent> Sun;
	TWeakObjectPtr<USkyLightComponent> SkyLight;
	TWeakObjectPtr<UExponentialHeightFogComponent> Fog;
	TWeakObjectPtr<APostProcessVolume> PostProcess;
	TWeakObjectPtr<AActor> Stars;

	TArray<TPair<TWeakObjectPtr<ULightComponent>, float>> LampLights;
	TArray<FLampGlow> LampGlows;
	TArray<FWindowMaterial> WindowMaterials;

	/** The dimmed facade materials, one per authored one; kept so they are not collected while in use. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> WindowDimmers;

	bool bListening = false;
};

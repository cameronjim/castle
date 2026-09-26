// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/ArrowEffects/ArrowEffect.h"
#include "EmpPulse.generated.h"

class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;

/**
 * The EMP arrow's pulse. Everything electric within Radius of the impact dies: street lamps
 * (actors tagged CityLamp) go dark for LampOffSeconds through UArrowEffectsSubsystem, and every
 * gunner's pistol jams for JamSeconds (AThugAIController::Jam). TODO(stage3): security cameras,
 * once there are any, go off the same way.
 *
 * The look is a blue ring of glowing segments (M_ArrowGlow) racing out to Radius over
 * RingSeconds and fading, with a blue flash at the centre. Placeholder, like the smoke: the
 * engine has no ready-made pulse Niagara system.
 */
UCLASS(Blueprintable, BlueprintType)
class HAWKEYE_API AEmpPulse : public AArrowEffect
{
	GENERATED_BODY()

public:
	AEmpPulse();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "EMP", meta = (ClampMin = "0.0"))
	float Radius = 600.f;

	/** How long lamps in the radius stay dark, s. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "EMP", meta = (ClampMin = "0.0"))
	float LampOffSeconds = 20.f;

	/** How long a gunner in the radius cannot fire, s. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "EMP", meta = (ClampMin = "0.0"))
	float JamSeconds = 6.f;

	/** How long the ring takes to reach Radius and fade, s. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "EMP", meta = (ClampMin = "0.05"))
	float RingSeconds = 0.9f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "EMP")
	FLinearColor PulseColor = FLinearColor(0.25f, 0.6f, 1.f);

	/** Lamp actors put out by this pulse. */
	UFUNCTION(BlueprintPure, Category = "EMP")
	int32 GetLampsAffected() const { return LampsAffected; }

	/** Gunners jammed by this pulse. */
	UFUNCTION(BlueprintPure, Category = "EMP")
	int32 GetGunnersJammed() const { return GunnersJammed; }

	virtual void Activate() override;
	virtual void AdvanceEffect(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	/** Jams every living gunner within Radius. Returns how many. */
	int32 JamGunners();

	/** Sizes and fades the ring and the flash for the current moment. */
	void UpdateRing();

	static constexpr int32 SegmentCount = 32;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "EMP")
	TArray<TObjectPtr<UStaticMeshComponent>> Segments;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "EMP")
	TObjectPtr<UPointLightComponent> Flash;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> SegmentMaterials;

	int32 LampsAffected = 0;
	int32 GunnersJammed = 0;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/ArrowEffects/ArrowEffect.h"
#include "ExplosiveBlast.generated.h"

class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;

/**
 * The explosive arrow's blast. Everything with health within Radius of the impact takes the
 * arrow's damage (80) at the centre falling linearly to 0 at the edge, Kate included if she is in
 * it. Surviving thugs are knocked down for KnockdownSeconds and thrown; the player's camera
 * shakes within ShakeRadius. No cover check: the blast goes round corners. TODO(stage3): occlusion.
 *
 * The look is an orange flash light and an additive fireball sphere (M_ArrowGlow) that swells
 * and fades over FlashSeconds.
 */
UCLASS(Blueprintable, BlueprintType)
class HAWKEYE_API AExplosiveBlast : public AArrowEffect
{
	GENERATED_BODY()

public:
	AExplosiveBlast();

	/** Where the damage reaches 0, cm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Explosive", meta = (ClampMin = "1.0"))
	float Radius = 400.f;

	/** How long a surviving thug is down, s. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Explosive", meta = (ClampMin = "0.0"))
	float KnockdownSeconds = 2.f;

	/** How hard a thug at the centre is thrown, cm/s; less further out. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Explosive", meta = (ClampMin = "0.0"))
	float ThrowSpeed = 900.f;

	/** The player's camera shakes when she is this close, cm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Explosive", meta = (ClampMin = "0.0"))
	float ShakeRadius = 2500.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Explosive", meta = (ClampMin = "0.0"))
	float ShakeSeconds = 0.5f;

	/** Lens throw at the start of the shake for a blast at her feet, cm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Explosive", meta = (ClampMin = "0.0"))
	float ShakeAmplitude = 18.f;

	/** How long the fireball and the flash last, s. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Explosive", meta = (ClampMin = "0.05"))
	float FlashSeconds = 0.6f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Explosive")
	FLinearColor FireColor = FLinearColor(1.f, 0.45f, 0.08f);

	/**
	 * Damage at Distance from a blast of MaxDamage that reaches 0 at InRadius: linear, never
	 * negative, MaxDamage at the centre. Pure.
	 */
	UFUNCTION(BlueprintPure, Category = "Explosive")
	static float ComputeFalloffDamage(float MaxDamage, float Distance, float InRadius);

	/** Actors this blast damaged. */
	UFUNCTION(BlueprintPure, Category = "Explosive")
	int32 GetVictimCount() const { return VictimCount; }

	virtual void Activate() override;
	virtual void AdvanceEffect(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	/** Damages, knocks down and shakes. */
	void ApplyBlast();

	void UpdateFireball();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Explosive")
	TObjectPtr<UStaticMeshComponent> Fireball;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Explosive")
	TObjectPtr<UStaticMeshComponent> FireballCore;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Explosive")
	TObjectPtr<UPointLightComponent> Flash;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FireballMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CoreMaterial;

	int32 VictimCount = 0;
};

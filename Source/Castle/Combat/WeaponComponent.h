// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WeaponComponent.generated.h"

class UDamageType;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAmmoChangedSignature, int32, CurrentAmmo, int32, ReserveAmmo);

/**
 * Hitscan weapon attached to a pawn. Traces from the owner's view point, so it works for the
 * first-person camera on ACastleCharacter without any extra wiring.
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = (Castle), meta = (BlueprintSpawnableComponent))
class CASTLE_API UWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWeaponComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Ammo", meta = (ClampMin = "1"))
	int32 MagazineSize = 30;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|Ammo")
	int32 CurrentAmmo = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Ammo", meta = (ClampMin = "0"))
	int32 ReserveAmmo = 120;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (ClampMin = "0.0"))
	float Damage = 20.f;

	/** Hitscan range in centimetres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (ClampMin = "0.0"))
	float Range = 10000.f;

	/** Rounds per minute. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (ClampMin = "1.0"))
	float FireRate = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (ClampMin = "0.0"))
	float ReloadSeconds = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	TSubclassOf<UDamageType> DamageTypeClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (ClampMin = "0.0"))
	float TraceChannelDebugDuration = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Debug")
	bool bDrawDebug = false;

	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FOnAmmoChangedSignature OnAmmoChanged;

	/** Fires one round if allowed. Returns true when a shot went out. */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	bool Fire();

	/** Starts the reload timer. Returns false if already full, already reloading or out of reserve. */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	bool Reload();

	/** Fire rate, ammo and reload state all satisfied. */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool CanFire() const;

	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsReloading() const { return bIsReloading; }

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void AddAmmo(int32 Rounds);

	/** Muzzle flash, tracer, sound, recoil. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Weapon")
	void OnWeaponFired(const FHitResult& Hit, bool bHitSomething);

	UFUNCTION(BlueprintImplementableEvent, Category = "Weapon")
	void OnReloadStarted(float Duration);

	UFUNCTION(BlueprintImplementableEvent, Category = "Weapon")
	void OnReloadFinished();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void FinishReload();

	/** View point the shot originates from (player camera when the owner is player controlled). */
	void GetFireViewPoint(FVector& OutLocation, FRotator& OutRotation) const;

	float GetSecondsBetweenShots() const { return 60.f / FMath::Max(FireRate, 1.f); }

private:
	bool bIsReloading = false;
	float LastFireTimeSeconds = TNumericLimits<float>::Lowest();
	FTimerHandle ReloadTimerHandle;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Math/RandomStream.h"
#include "WeaponComponent.generated.h"

class UDamageType;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAmmoChangedSignature, int32, CurrentAmmo, int32, ReserveAmmo);

/** Fired instead of a shot when the trigger is pulled on an empty magazine. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEmptyClickSignature);

/**
 * Hitscan weapon attached to a pawn. Traces from the owner's view point, so it works for the
 * first-person camera on ACastleCharacter without any extra wiring.
 *
 * Default stats are the starter pistol: 34 damage, x3 on a headshot, so a 100 HP guard dies to
 * three body shots or one headshot.
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = (Castle), meta = (BlueprintSpawnableComponent))
class CASTLE_API UWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWeaponComponent();

	/**
	 * False while the owner is empty-handed: Fire and Reload do nothing and the HUD shows no ammo.
	 * The player's component starts false and APickupActor turns it on; guards start armed.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	bool bHasWeapon = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Ammo", meta = (ClampMin = "1"))
	int32 MagazineSize = 12;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Ammo")
	int32 CurrentAmmo = 12;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Ammo", meta = (ClampMin = "0"))
	int32 ReserveAmmo = 24;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (ClampMin = "0.0"))
	float Damage = 34.f;

	/** Damage multiplier applied when the hit bone is in HeadBoneNames. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (ClampMin = "1.0"))
	float HeadshotMultiplier = 3.f;

	/** Bones that count as a head. Matches the default UE5 skeleton naming. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	TSet<FName> HeadBoneNames;

	/** Hitscan range in centimetres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (ClampMin = "0.0"))
	float Range = 10000.f;

	/** Rounds per minute. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (ClampMin = "1.0"))
	float FireRate = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (ClampMin = "0.0"))
	float ReloadSeconds = 2.f;

	/** Cone half-angle applied to the shot direction while firing from the hip. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Spread", meta = (ClampMin = "0.0"))
	float HipSpreadDegrees = 2.5f;

	/** Cone half-angle applied while aiming down sights. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Spread", meta = (ClampMin = "0.0"))
	float AimSpreadDegrees = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	TSubclassOf<UDamageType> DamageTypeClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (ClampMin = "0.0"))
	float TraceChannelDebugDuration = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Debug")
	bool bDrawDebug = false;

	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FOnAmmoChangedSignature OnAmmoChanged;

	/** Fired when Fire() is called with an empty magazine; play the dry-fire click from this. */
	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FOnEmptyClickSignature OnEmptyClick;

	/** Arms the owner with Magazine rounds loaded and Reserve spare, and fires OnAmmoChanged. */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void GiveWeapon(int32 Magazine, int32 Reserve);

	/** Disarms the owner. Ammo is kept so a later GiveWeapon can restore it. */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void RemoveWeapon();

	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool HasWeapon() const { return bHasWeapon; }

	/** Fires one round if allowed. Returns true when a shot went out. */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	bool Fire();

	/** Starts the reload timer. Returns false if already full, already reloading or out of reserve. */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	bool Reload();

	/**
	 * Moves min(MagazineSize - CurrentAmmo, ReserveAmmo) rounds into the magazine right now and
	 * ends the reload. Called by the reload timer, by an animation notify, and by tests.
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void CompleteReloadNow();

	/** Aborts an in-progress reload without moving any ammo (sprinting does this). */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void CancelReload();

	/**
	 * Tightens the shot cone while the owner is aiming down sights. Does nothing while
	 * bHasWeapon is false, so aiming empty-handed never leaves the component in an aimed state.
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Spread")
	void SetAiming(bool bInAiming);

	UFUNCTION(BlueprintPure, Category = "Weapon|Spread")
	bool IsAiming() const { return bIsAiming; }

	/** AimSpreadDegrees while aiming, HipSpreadDegrees otherwise. */
	UFUNCTION(BlueprintPure, Category = "Weapon|Spread")
	float GetCurrentSpreadDegrees() const { return bIsAiming ? AimSpreadDegrees : HipSpreadDegrees; }

	/**
	 * Direction rotated by a random offset inside a cone of SpreadDegrees half-angle. Pure and
	 * stream-driven so the spread maths is testable without firing a shot.
	 */
	UFUNCTION(BlueprintPure, Category = "Weapon|Spread")
	static FVector ApplyConeSpread(const FVector& Direction, float SpreadDegrees, const FRandomStream& Stream);

	/** Replaces the spread stream so a test gets the same cone offsets every run. */
	void SetTestRandomStream(const FRandomStream& InStream);

	/** Damage this weapon deals to a hit on BoneName, including the headshot multiplier. */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	float ComputeDamageForHit(FName BoneName) const;

	/** Fire rate, ammo and reload state all satisfied. */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool CanFire() const;

	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsReloading() const { return bIsReloading; }

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void AddAmmo(int32 Rounds);

	/**
	 * Overrides the clock used for the fire-rate check. Automation tests use this because a
	 * component created with NewObject has no world to read GetTimeSeconds() from.
	 */
	void SetTestTimeSeconds(double InSeconds);

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

	/** Current time for the fire-rate check: the test clock if one is set, else the world clock. */
	virtual double GetNowSeconds() const;

	/** View point the shot originates from (player camera when the owner is player controlled). */
	void GetFireViewPoint(FVector& OutLocation, FRotator& OutRotation) const;

	/** Double precision so a test clock advanced by exactly one interval is not rejected. */
	double GetSecondsBetweenShots() const { return 60.0 / FMath::Max(static_cast<double>(FireRate), 1.0); }

	/** Runs the hitscan trace and applies damage. Skipped when the component has no world. */
	void TraceAndApplyDamage();

private:
	bool bIsReloading = false;

	/** Runtime only: ACastleCharacter drives this from the Aim input action. */
	UPROPERTY(Transient)
	bool bIsAiming = false;

	/** Seeded from the owner's name at BeginPlay so two guards do not fire identical patterns. */
	UPROPERTY(Transient)
	FRandomStream SpreadStream;

	double LastFireTimeSeconds = TNumericLimits<double>::Lowest();
	FTimerHandle ReloadTimerHandle;

	bool bUseTestTime = false;
	double TestTimeOverride = 0.0;
};

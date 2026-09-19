// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

class UHealthComponent;
class UDamageType;
class AController;

/** NewHealth/Delta are absolute values; Delta is negative for damage. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnHealthChangedSignature, UHealthComponent*, HealthComponent, float, NewHealth, float, Delta, AActor*, DamageInstigator);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDeathSignature, UHealthComponent*, HealthComponent, AActor*, Killer);

/** Fired by a melee hit above StaggerThreshold. Bullets never stagger. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStaggeredSignature, UHealthComponent*, HealthComponent, AActor*, DamageInstigator);

/**
 * Health, damage and death for any actor. Automatically forwards the owner's
 * OnTakeAnyDamage (so UGameplayStatics::ApplyDamage/ApplyPointDamage just work).
 *
 * Rules (see claude-docs/gameplay-semantics.md):
 * CurrentHealth is clamped to [0, MaxHealth]; damage <= 0 does nothing; invulnerability
 * ignores damage entirely; OnHealthChanged fires on every change including heals;
 * OnDeath fires exactly once; healing a dead component does nothing until Revive.
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = (Castle), meta = (BlueprintSpawnableComponent))
class CASTLE_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHealthComponent();

	/** Upper bound for CurrentHealth. Change it through SetMaxHealth at runtime. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.f;

	/** Current hit points. Always within [0, MaxHealth]. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health")
	float CurrentHealth = 100.f;

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnHealthChangedSignature OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnDeathSignature OnDeath;

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnStaggeredSignature OnStaggered;

	/**
	 * Melee damage above this fires OnStaggered. A punch is 15 damage, so the default lets
	 * every punch stagger and leaves room for a weaker one later.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health", meta = (ClampMin = "0.0"))
	float StaggerThreshold = 10.f;

	/** Applies DamageAmount. Returns the damage actually taken (0 when invulnerable or dead). */
	UFUNCTION(BlueprintCallable, Category = "Health")
	float ApplyDamage(float DamageAmount, AActor* DamageInstigator = nullptr);

	/**
	 * Melee damage: the same as ApplyDamage, plus OnStaggered when bStagger is true, the
	 * damage landed and it was above StaggerThreshold. Ignored damage never staggers.
	 */
	UFUNCTION(BlueprintCallable, Category = "Health")
	float ApplyMeleeDamage(float DamageAmount, AActor* DamageInstigator = nullptr, bool bStagger = true);

	/** Restores health up to MaxHealth. Returns the amount actually healed. No-op while dead. */
	UFUNCTION(BlueprintCallable, Category = "Health")
	float Heal(float HealAmount, AActor* Healer = nullptr);

	/**
	 * Brings a dead component back with NewHealth hit points and fires OnHealthChanged.
	 * Values outside (0, MaxHealth] are clamped; a revive at 0 health is not allowed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Health")
	void Revive(float NewHealth);

	/** Changes MaxHealth. When bResetCurrent is true CurrentHealth is set to the new maximum. */
	UFUNCTION(BlueprintCallable, Category = "Health")
	void SetMaxHealth(float NewMaxHealth, bool bResetCurrent = true);

	/** While invulnerable, damage is ignored entirely: no OnHealthChanged, no OnDeath. */
	UFUNCTION(BlueprintCallable, Category = "Health")
	void SetInvulnerable(bool bNewInvulnerable) { bInvulnerable = bNewInvulnerable; }

	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsInvulnerable() const { return bInvulnerable; }

	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsDead() const { return bIsDead; }

	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsAlive() const { return !bIsDead; }

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetHealthPercent() const { return MaxHealth > 0.f ? CurrentHealth / MaxHealth : 0.f; }

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser);

	/** While true all damage is ignored (used by boss phase transitions). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health")
	bool bInvulnerable = false;

	/** Set the first time health reaches 0; cleared only by Revive. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health")
	bool bIsDead = false;
};

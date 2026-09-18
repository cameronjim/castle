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

/**
 * Health, damage and death for any actor. Automatically forwards the owner's
 * OnTakeAnyDamage (so UGameplayStatics::ApplyDamage/ApplyPointDamage just work).
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = (Castle), meta = (BlueprintSpawnableComponent))
class CASTLE_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHealthComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.f;

	UPROPERTY(BlueprintReadOnly, Category = "Health")
	float CurrentHealth = 100.f;

	/** While true all damage is ignored (used by boss phase transitions). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	bool bInvulnerable = false;

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnHealthChangedSignature OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnDeathSignature OnDeath;

	/** Applies DamageAmount. Returns the damage actually taken (0 when invulnerable or dead). */
	UFUNCTION(BlueprintCallable, Category = "Health")
	float ApplyDamage(float DamageAmount, AActor* DamageInstigator = nullptr);

	/** Restores health up to MaxHealth. Returns the amount actually healed. */
	UFUNCTION(BlueprintCallable, Category = "Health")
	float Heal(float HealAmount, AActor* Healer = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Health")
	void SetInvulnerable(bool bNewInvulnerable) { bInvulnerable = bNewInvulnerable; }

	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsDead() const { return CurrentHealth <= 0.f; }

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetHealthPercent() const { return MaxHealth > 0.f ? CurrentHealth / MaxHealth : 0.f; }

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser);

	bool bDeathBroadcast = false;
};

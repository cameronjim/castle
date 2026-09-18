// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/HealthComponent.h"

#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"

UHealthComponent::UHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	CurrentHealth = MaxHealth;

	if (AActor* Owner = GetOwner())
	{
		Owner->OnTakeAnyDamage.AddDynamic(this, &UHealthComponent::HandleTakeAnyDamage);
	}
}

void UHealthComponent::HandleTakeAnyDamage(AActor* /*DamagedActor*/, float Damage, const UDamageType* /*DamageType*/, AController* InstigatedBy, AActor* DamageCauser)
{
	AActor* DamageInstigator = InstigatedBy ? Cast<AActor>(InstigatedBy->GetPawn()) : DamageCauser;
	ApplyDamage(Damage, DamageInstigator ? DamageInstigator : DamageCauser);
}

float UHealthComponent::ApplyDamage(float DamageAmount, AActor* DamageInstigator)
{
	if (DamageAmount <= 0.f || bInvulnerable || IsDead())
	{
		return 0.f;
	}

	const float OldHealth = CurrentHealth;
	CurrentHealth = FMath::Clamp(CurrentHealth - DamageAmount, 0.f, MaxHealth);

	const float ActualDelta = CurrentHealth - OldHealth;
	if (FMath::IsNearlyZero(ActualDelta))
	{
		return 0.f;
	}

	OnHealthChanged.Broadcast(this, CurrentHealth, ActualDelta, DamageInstigator);

	if (IsDead() && !bDeathBroadcast)
	{
		bDeathBroadcast = true;
		OnDeath.Broadcast(this, DamageInstigator);
	}

	return -ActualDelta;
}

float UHealthComponent::Heal(float HealAmount, AActor* Healer)
{
	if (HealAmount <= 0.f || IsDead())
	{
		return 0.f;
	}

	const float OldHealth = CurrentHealth;
	CurrentHealth = FMath::Clamp(CurrentHealth + HealAmount, 0.f, MaxHealth);

	const float ActualDelta = CurrentHealth - OldHealth;
	if (FMath::IsNearlyZero(ActualDelta))
	{
		return 0.f;
	}

	OnHealthChanged.Broadcast(this, CurrentHealth, ActualDelta, Healer);
	return ActualDelta;
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/WeaponComponent.h"

#include "Castle.h"
#include "CollisionQueryParams.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

UWeaponComponent::UWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWeaponComponent::BeginPlay()
{
	Super::BeginPlay();

	CurrentAmmo = FMath::Clamp(CurrentAmmo, 0, MagazineSize);
	OnAmmoChanged.Broadcast(CurrentAmmo, ReserveAmmo);
}

void UWeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReloadTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

bool UWeaponComponent::CanFire() const
{
	const UWorld* World = GetWorld();
	if (!World || bIsReloading || CurrentAmmo <= 0)
	{
		return false;
	}

	return World->GetTimeSeconds() - LastFireTimeSeconds >= GetSecondsBetweenShots();
}

void UWeaponComponent::GetFireViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	OutLocation = FVector::ZeroVector;
	OutRotation = FRotator::ZeroRotator;

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (const APawn* Pawn = Cast<APawn>(Owner))
	{
		if (AController* OwnerController = Pawn->GetController())
		{
			// Matches the first-person camera for player-controlled pawns.
			OwnerController->GetPlayerViewPoint(OutLocation, OutRotation);
			return;
		}
	}

	Owner->GetActorEyesViewPoint(OutLocation, OutRotation);
}

bool UWeaponComponent::Fire()
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner || !CanFire())
	{
		return false;
	}

	LastFireTimeSeconds = World->GetTimeSeconds();
	--CurrentAmmo;
	OnAmmoChanged.Broadcast(CurrentAmmo, ReserveAmmo);

	FVector ViewLocation;
	FRotator ViewRotation;
	GetFireViewPoint(ViewLocation, ViewRotation);

	const FVector ShotDirection = ViewRotation.Vector();
	const FVector TraceEnd = ViewLocation + ShotDirection * Range;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CastleWeaponFire), /*bTraceComplex=*/true, Owner);
	QueryParams.AddIgnoredActor(Owner);
	QueryParams.bReturnPhysicalMaterial = true;

	FHitResult Hit;
	const bool bHitSomething = World->LineTraceSingleByChannel(Hit, ViewLocation, TraceEnd, ECC_Visibility, QueryParams);

	if (bHitSomething && Hit.GetActor())
	{
		AController* InstigatorController = nullptr;
		if (const APawn* Pawn = Cast<APawn>(Owner))
		{
			InstigatorController = Pawn->GetController();
		}

		UGameplayStatics::ApplyPointDamage(
			Hit.GetActor(), Damage, ShotDirection, Hit, InstigatorController, Owner, DamageTypeClass);
	}

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug)
	{
		DrawDebugLine(World, ViewLocation, bHitSomething ? Hit.ImpactPoint : TraceEnd,
			FColor::Red, false, TraceChannelDebugDuration, 0, 1.f);
	}
#endif

	OnWeaponFired(Hit, bHitSomething);
	return true;
}

bool UWeaponComponent::Reload()
{
	UWorld* World = GetWorld();
	if (!World || bIsReloading || ReserveAmmo <= 0 || CurrentAmmo >= MagazineSize)
	{
		return false;
	}

	bIsReloading = true;
	OnReloadStarted(ReloadSeconds);

	if (ReloadSeconds > 0.f)
	{
		World->GetTimerManager().SetTimer(
			ReloadTimerHandle, this, &UWeaponComponent::FinishReload, ReloadSeconds, false);
	}
	else
	{
		FinishReload();
	}

	return true;
}

void UWeaponComponent::FinishReload()
{
	bIsReloading = false;

	const int32 Needed = MagazineSize - CurrentAmmo;
	const int32 Loaded = FMath::Min(Needed, ReserveAmmo);

	CurrentAmmo += Loaded;
	ReserveAmmo -= Loaded;

	OnAmmoChanged.Broadcast(CurrentAmmo, ReserveAmmo);
	OnReloadFinished();
}

void UWeaponComponent::AddAmmo(int32 Rounds)
{
	if (Rounds <= 0)
	{
		return;
	}

	ReserveAmmo += Rounds;
	OnAmmoChanged.Broadcast(CurrentAmmo, ReserveAmmo);
}

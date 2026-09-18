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

	HeadBoneNames.Add(FName(TEXT("head")));
	HeadBoneNames.Add(FName(TEXT("neck_01")));
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

double UWeaponComponent::GetNowSeconds() const
{
	if (bUseTestTime)
	{
		return TestTimeOverride;
	}

	const UWorld* World = GetWorld();
	return World ? static_cast<double>(World->GetTimeSeconds()) : 0.0;
}

void UWeaponComponent::SetTestTimeSeconds(double InSeconds)
{
	bUseTestTime = true;
	TestTimeOverride = InSeconds;
}

void UWeaponComponent::GiveWeapon(int32 Magazine, int32 Reserve)
{
	bHasWeapon = true;
	CurrentAmmo = FMath::Clamp(Magazine, 0, MagazineSize);
	ReserveAmmo = FMath::Max(Reserve, 0);
	OnAmmoChanged.Broadcast(CurrentAmmo, ReserveAmmo);
}

void UWeaponComponent::RemoveWeapon()
{
	CancelReload();
	bHasWeapon = false;
	OnAmmoChanged.Broadcast(CurrentAmmo, ReserveAmmo);
}

bool UWeaponComponent::CanFire() const
{
	if (!bHasWeapon || bIsReloading || CurrentAmmo <= 0)
	{
		return false;
	}

	return GetNowSeconds() - LastFireTimeSeconds >= GetSecondsBetweenShots();
}

float UWeaponComponent::ComputeDamageForHit(FName BoneName) const
{
	return HeadBoneNames.Contains(BoneName) ? Damage * HeadshotMultiplier : Damage;
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
	if (!bHasWeapon)
	{
		// Empty-handed: not even a dry-fire click.
		return false;
	}

	if (bIsReloading)
	{
		return false;
	}

	if (CurrentAmmo <= 0)
	{
		OnEmptyClick.Broadcast();
		return false;
	}

	if (GetNowSeconds() - LastFireTimeSeconds < GetSecondsBetweenShots())
	{
		return false;
	}

	LastFireTimeSeconds = GetNowSeconds();
	--CurrentAmmo;
	OnAmmoChanged.Broadcast(CurrentAmmo, ReserveAmmo);

	TraceAndApplyDamage();
	return true;
}

void UWeaponComponent::TraceAndApplyDamage()
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner)
	{
		// Ammo accounting still happened; there is simply nothing to trace against.
		return;
	}

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
			Hit.GetActor(), ComputeDamageForHit(Hit.BoneName), ShotDirection, Hit,
			InstigatorController, Owner, DamageTypeClass);
	}

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug)
	{
		DrawDebugLine(World, ViewLocation, bHitSomething ? Hit.ImpactPoint : TraceEnd,
			FColor::Red, false, TraceChannelDebugDuration, 0, 1.f);
	}
#endif

	OnWeaponFired(Hit, bHitSomething);
}

bool UWeaponComponent::Reload()
{
	if (!bHasWeapon || bIsReloading || ReserveAmmo <= 0 || CurrentAmmo >= MagazineSize)
	{
		return false;
	}

	bIsReloading = true;
	OnReloadStarted(ReloadSeconds);

	if (ReloadSeconds <= 0.f)
	{
		CompleteReloadNow();
		return true;
	}

	// Without a world there is no timer manager; the caller (an animation notify, or a test)
	// finishes the reload with CompleteReloadNow().
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ReloadTimerHandle, this, &UWeaponComponent::CompleteReloadNow, ReloadSeconds, false);
	}

	return true;
}

void UWeaponComponent::CompleteReloadNow()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReloadTimerHandle);
	}

	bIsReloading = false;

	const int32 Needed = MagazineSize - CurrentAmmo;
	const int32 Loaded = FMath::Min(Needed, ReserveAmmo);
	if (Loaded <= 0)
	{
		OnReloadFinished();
		return;
	}

	CurrentAmmo += Loaded;
	ReserveAmmo -= Loaded;

	OnAmmoChanged.Broadcast(CurrentAmmo, ReserveAmmo);
	OnReloadFinished();
}

void UWeaponComponent::CancelReload()
{
	if (!bIsReloading)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReloadTimerHandle);
	}

	bIsReloading = false;
	UE_LOG(LogCastle, Verbose, TEXT("%s: reload cancelled."), *GetNameSafe(GetOwner()));
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

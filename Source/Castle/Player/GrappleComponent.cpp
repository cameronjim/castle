// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/GrappleComponent.h"

#include "Camera/CameraComponent.h"
#include "Castle.h"
#include "Combat/ArrowDefinition.h"
#include "Combat/BowComponent.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/CastleCharacter.h"
#include "Player/InventoryComponent.h"
#include "World/GrappleAnchor.h"
#include "World/GrappleArrowProjectile.h"

namespace CastleGrapple
{
	/** Line-of-sight traces per refresh at most; the candidates are tried smallest angle first. */
	static constexpr int32 MaxSightTraces = 8;

	/** How far below an anchor's landing point its building is looked for. */
	static constexpr float SupportProbeDepth = 150.f;
}

UGrappleComponent::UGrappleComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	ArrowClass = AGrappleArrowProjectile::StaticClass();
}

void UGrappleComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bZipping)
	{
		AdvanceZip(DeltaTime);
	}

	RefreshAccumulator += DeltaTime;
	if (RefreshAccumulator >= RefreshSeconds)
	{
		RefreshAccumulator = 0.f;
		RefreshTarget();
		RecoverNearbyArrows();
	}
}

ACharacter* UGrappleComponent::GetCharacter() const
{
	return Cast<ACharacter>(GetOwner());
}

UInventoryComponent* UGrappleComponent::GetInventory() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->FindComponentByClass<UInventoryComponent>() : nullptr;
}

int32 UGrappleComponent::FindGrappleSlot() const
{
	const UInventoryComponent* Inventory = GetInventory();
	return Inventory ? Inventory->FindArrowSlotByEffect(EArrowHitEffect::Grapple) : INDEX_NONE;
}

int32 UGrappleComponent::GetGrappleArrows() const
{
	const UInventoryComponent* Inventory = GetInventory();
	const int32 Slot = FindGrappleSlot();
	return (Inventory && Slot != INDEX_NONE) ? Inventory->GetArrowCount(Slot) : 0;
}

void UGrappleComponent::SetGrappleArrows(int32 Count)
{
	UInventoryComponent* Inventory = GetInventory();
	const int32 Slot = FindGrappleSlot();
	if (Inventory && Slot != INDEX_NONE)
	{
		Inventory->SetArrowCount(Slot, Count);
	}
}

// --- Targeting ----------------------------------------------------------------------------------

FIntPoint UGrappleComponent::CellOf(const FVector& Location) const
{
	return FIntPoint(FMath::FloorToInt(Location.X / GridCellSize), FMath::FloorToInt(Location.Y / GridCellSize));
}

void UGrappleComponent::BuildGrid() const
{
	AnchorGrid.Reset();
	bGridBuilt = true;
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	int32 Count = 0;
	for (TActorIterator<AGrappleAnchor> It(World); It; ++It)
	{
		AnchorGrid.FindOrAdd(CellOf(It->GetActorLocation())).Add(*It);
		++Count;
	}
	UE_LOG(LogCastle, Log, TEXT("%s: %d grapple anchors in %d grid cells of %.0f cm"),
		*GetNameSafe(GetOwner()), Count, AnchorGrid.Num(), GridCellSize);
}

void UGrappleComponent::RebuildAnchorGrid()
{
	BuildGrid();
}

void UGrappleComponent::GatherNearbyAnchors(const FVector& Location, TArray<AGrappleAnchor*>& OutAnchors) const
{
	if (!bGridBuilt)
	{
		BuildGrid();
	}

	const FIntPoint Centre = CellOf(Location);
	const int32 Reach = FMath::CeilToInt(Range / GridCellSize);
	for (int32 DX = -Reach; DX <= Reach; ++DX)
	{
		for (int32 DY = -Reach; DY <= Reach; ++DY)
		{
			const TArray<TWeakObjectPtr<AGrappleAnchor>>* Cell = AnchorGrid.Find(Centre + FIntPoint(DX, DY));
			if (!Cell)
			{
				continue;
			}
			for (const TWeakObjectPtr<AGrappleAnchor>& Anchor : *Cell)
			{
				if (AGrappleAnchor* Live = Anchor.Get())
				{
					OutAnchors.Add(Live);
				}
			}
		}
	}
}

bool UGrappleComponent::IsAnchorValid(const AGrappleAnchor* Anchor, const FVector& ViewLocation,
	const FVector& ViewForward, float& OutAngleDegrees) const
{
	if (!IsValid(Anchor) || !Anchor->bEnabled || (bZipping && Anchor == ZipAnchor.Get()))
	{
		return false;
	}

	const AActor* Owner = GetOwner();
	const FVector Marker = Anchor->GetMarkerLocation();
	const float Distance = Owner ? FVector::Dist(Owner->GetActorLocation(), Marker) : 0.f;
	if (!Owner || Distance > Range || Distance < MinRange)
	{
		return false;
	}

	const FVector ToAnchor = (Marker - ViewLocation).GetSafeNormal();
	const float Cosine = FMath::Clamp(FVector::DotProduct(ViewForward.GetSafeNormal(), ToAnchor), -1.f, 1.f);
	OutAngleDegrees = FMath::RadiansToDegrees(FMath::Acos(Cosine));
	return OutAngleDegrees <= ConeDegrees;
}

bool UGrappleComponent::HasLineOfSight(const AGrappleAnchor* Anchor, const FVector& ViewLocation) const
{
	const UWorld* World = GetWorld();
	if (!World || !Anchor)
	{
		return false;
	}

	const FVector Marker = Anchor->GetMarkerLocation();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GrappleSight), false, GetOwner());
	Params.AddIgnoredActor(Anchor);
	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, ViewLocation, Marker, ECC_Visibility, Params))
	{
		return true;
	}
	// The anchor sits on a parapet; a hit on the stone right under it still means it is in view.
	return Hit.Distance >= FVector::Dist(ViewLocation, Marker) - SightTolerance;
}

AGrappleAnchor* UGrappleComponent::SelectBestAnchor(const FVector& ViewLocation, const FVector& ViewForward) const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	TArray<AGrappleAnchor*> Nearby;
	GatherNearbyAnchors(Owner->GetActorLocation(), Nearby);

	TArray<TPair<float, AGrappleAnchor*>> Candidates;
	for (AGrappleAnchor* Anchor : Nearby)
	{
		float Angle = 0.f;
		if (IsAnchorValid(Anchor, ViewLocation, ViewForward, Angle))
		{
			Candidates.Emplace(Angle, Anchor);
		}
	}
	Candidates.Sort([](const TPair<float, AGrappleAnchor*>& A, const TPair<float, AGrappleAnchor*>& B)
	{
		return A.Key < B.Key;
	});

	const int32 Traces = FMath::Min(Candidates.Num(), CastleGrapple::MaxSightTraces);
	for (int32 Index = 0; Index < Traces; ++Index)
	{
		if (HasLineOfSight(Candidates[Index].Value, ViewLocation))
		{
			return Candidates[Index].Value;
		}
	}
	return nullptr;
}

void UGrappleComponent::UpdateTarget(const FVector& ViewLocation, const FVector& ViewForward)
{
	TargetAnchor = SelectBestAnchor(ViewLocation, ViewForward);
}

void UGrappleComponent::GetViewPoint(FVector& OutLocation, FVector& OutForward) const
{
	const ACastleCharacter* Castle = Cast<ACastleCharacter>(GetOwner());
	if (const UCameraComponent* Camera = Castle ? Castle->GetFollowCamera() : nullptr)
	{
		OutLocation = Camera->GetComponentLocation();
		OutForward = Camera->GetForwardVector();
		return;
	}

	FRotator EyesRotation = FRotator::ZeroRotator;
	OutLocation = FVector::ZeroVector;
	if (const AActor* Owner = GetOwner())
	{
		Owner->GetActorEyesViewPoint(OutLocation, EyesRotation);
	}
	OutForward = EyesRotation.Vector();
}

void UGrappleComponent::RefreshTarget()
{
	FVector ViewLocation, ViewForward;
	GetViewPoint(ViewLocation, ViewForward);
	UpdateTarget(ViewLocation, ViewForward);
}

// --- Firing -------------------------------------------------------------------------------------

bool UGrappleComponent::TryFire()
{
	if (IsArrowInFlight() || !CanChain())
	{
		return false;
	}

	AGrappleAnchor* Anchor = TargetAnchor.Get();
	if (!IsValid(Anchor) || !Anchor->bEnabled)
	{
		return false;
	}

	// Whatever slot is nocked, Q (and a release with the grapple slot active) spends from the
	// grapple slot of the quiver.
	UInventoryComponent* Inventory = GetInventory();
	const int32 Slot = FindGrappleSlot();
	UArrowDefinition* Definition = (Inventory && Slot != INDEX_NONE) ? Inventory->GetArrowSlot(Slot).Arrow.Get() : nullptr;
	if (!Definition || !Inventory->ConsumeArrow(Slot))
	{
		UE_LOG(LogCastle, Log, TEXT("%s: no grapple arrows left"), *GetNameSafe(GetOwner()));
		return false;
	}
	++UseCount;

	const AActor* Owner = GetOwner();
	AGrappleArrowProjectile* Arrow = SpawnGrappleArrow(Definition, Anchor);

	UE_LOG(LogCastle, Log, TEXT("%s: grapple arrow at %s, %.0f cm away, %d left"), *GetNameSafe(Owner),
		*GetNameSafe(Anchor), Owner ? FVector::Dist(Owner->GetActorLocation(), Anchor->GetMarkerLocation()) : 0.f,
		GetGrappleArrows());

	if (!Arrow)
	{
		// No world to fly through: the arrow is there at once.
		Anchor->AddStuckArrow(nullptr);
		HandleArrowArrived(nullptr, Anchor);
		return true;
	}
	InFlightArrow = Arrow;
	Arrow->Launch(Anchor, this);
	return true;
}

AGrappleArrowProjectile* UGrappleComponent::SpawnGrappleArrow(UArrowDefinition* Definition, const AGrappleAnchor* Anchor) const
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!World || !Owner || !Anchor)
	{
		return nullptr;
	}

	// Through the bow when there is one, so it leaves the hand; the definition's class wins when
	// it is a grapple projectile (BP_Arrow_Grapple), ArrowClass otherwise.
	AGrappleArrowProjectile* Arrow = nullptr;
	if (const UBowComponent* Bow = Owner->FindComponentByClass<UBowComponent>())
	{
		const FVector Start = Bow->GetArrowSpawnLocation();
		const FVector Direction = (Anchor->GetMarkerLocation() - Start).GetSafeNormal();
		AArrowProjectile* Spawned = Bow->SpawnArrowProjectile(Definition, ArrowClass, Direction);
		Arrow = Cast<AGrappleArrowProjectile>(Spawned);
		if (Spawned && !Arrow)
		{
			UE_LOG(LogCastle, Warning, TEXT("%s: %s's projectile %s is not a grapple arrow; using %s."),
				*GetNameSafe(Owner), *GetNameSafe(Definition), *GetNameSafe(Spawned->GetClass()), *GetNameSafe(ArrowClass));
			Spawned->Destroy();
		}
	}
	if (!Arrow && ArrowClass)
	{
		const FVector Start = Owner->GetActorTransform().TransformPosition(ArrowLaunchOffset);
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.Owner = Owner;
		Arrow = World->SpawnActor<AGrappleArrowProjectile>(ArrowClass, Start, FRotator::ZeroRotator, SpawnParams);
	}
	if (Arrow)
	{
		Arrow->InitArrow(Definition, nullptr, 0.f, Owner, nullptr);
	}
	return Arrow;
}

void UGrappleComponent::HandleArrowArrived(AGrappleArrowProjectile* Arrow, AGrappleAnchor* Anchor)
{
	if (InFlightArrow.Get() == Arrow)
	{
		InFlightArrow.Reset();
	}
	StartZip(Anchor);
}

int32 UGrappleComponent::RecoverNearbyArrows()
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return 0;
	}

	TArray<AGrappleAnchor*> Nearby;
	GatherNearbyAnchors(Owner->GetActorLocation(), Nearby);
	int32 Recovered = 0;
	for (AGrappleAnchor* Anchor : Nearby)
	{
		if (Anchor->GetStuckArrowCount() > 0
			&& FVector::Dist(Owner->GetActorLocation(), Anchor->GetActorLocation()) <= RecoverRadius)
		{
			Recovered += Anchor->RecoverStuckArrows();
		}
	}
	if (Recovered > 0)
	{
		UInventoryComponent* Inventory = GetInventory();
		const int32 Slot = FindGrappleSlot();
		if (Inventory && Slot != INDEX_NONE)
		{
			Inventory->AddArrows(Inventory->GetArrowSlot(Slot).Arrow, Recovered);
		}
		UE_LOG(LogCastle, Log, TEXT("%s: recovered %d grapple arrow(s), %d now"), *GetNameSafe(Owner), Recovered,
			GetGrappleArrows());
	}
	return Recovered;
}

// --- Zip ----------------------------------------------------------------------------------------

FVector UGrappleComponent::ComputeZipEnd(const AGrappleAnchor* Anchor) const
{
	if (!Anchor)
	{
		return FVector::ZeroVector;
	}
	const ACharacter* Character = GetCharacter();
	const float HalfHeight = Character && Character->GetCapsuleComponent()
		? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.f;
	// Feet on the landing point, with the same 2 cm of air a teleport leaves so the floor is found.
	return Anchor->GetLandingLocation() + FVector(0.f, 0.f, HalfHeight + 2.f);
}

void UGrappleComponent::FindAnchorSupports(const AGrappleAnchor* Anchor, TArray<AActor*>& OutSupports) const
{
	const UWorld* World = GetWorld();
	if (!World || !Anchor)
	{
		return;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GrappleSupport), true, GetOwner());
	Params.AddIgnoredActor(Anchor);

	const FVector Landing = Anchor->GetLandingLocation();
	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, Landing + FVector(0.f, 0.f, 50.f),
			Landing - FVector(0.f, 0.f, CastleGrapple::SupportProbeDepth), ECC_WorldStatic, Params)
		&& Hit.GetActor())
	{
		OutSupports.AddUnique(Hit.GetActor());
	}

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Anchor->GetActorLocation(), FQuat::Identity,
		FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionShape::MakeSphere(SupportRadius), Params);
	for (const FOverlapResult& Overlap : Overlaps)
	{
		if (AActor* Actor = Overlap.GetActor())
		{
			OutSupports.AddUnique(Actor);
		}
	}
}

void UGrappleComponent::SetSupportsIgnored(bool bIgnore)
{
	const ACharacter* Character = GetCharacter();
	UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
	if (!Capsule)
	{
		return;
	}
	for (const TWeakObjectPtr<AActor>& Support : ZipIgnoredSupports)
	{
		if (AActor* Actor = Support.Get())
		{
			Capsule->IgnoreActorWhenMoving(Actor, bIgnore);
		}
	}
}

bool UGrappleComponent::StartZip(AGrappleAnchor* Anchor)
{
	ACharacter* Character = GetCharacter();
	UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!IsValid(Anchor) || !Movement)
	{
		return false;
	}

	if (bZipping)
	{
		// A chain: the old building stops being ignored, the new one starts.
		SetSupportsIgnored(false);
	}
	else
	{
		PreZipGravityScale = Movement->GravityScale;
	}

	if (ACastleCharacter* Castle = Cast<ACastleCharacter>(Character))
	{
		Castle->StopAim();
	}

	ZipAnchor = Anchor;
	ZipStart = Character->GetActorLocation();
	ZipEnd = ComputeZipEnd(Anchor);
	ZipLength = FVector::Dist(ZipStart, ZipEnd);
	ZipTravelled = 0.f;
	ZipProgress = 0.f;
	bZipping = true;

	// The line ends on the anchor's roof and clips its parapet on the way in; that building is
	// expected. Anything else in the way cancels the zip.
	TArray<AActor*> Supports;
	FindAnchorSupports(Anchor, Supports);
	ZipIgnoredSupports.Reset();
	for (AActor* Support : Supports)
	{
		ZipIgnoredSupports.Add(Support);
	}
	SetSupportsIgnored(true);

	Movement->GravityScale = 0.f;
	Movement->Velocity = FVector::ZeroVector;
	Movement->SetMovementMode(MOVE_Flying);

	const FVector Direction = (ZipEnd - ZipStart).GetSafeNormal2D();
	if (!Direction.IsNearlyZero())
	{
		Character->SetActorRotation(FRotator(0.f, Direction.Rotation().Yaw, 0.f));
	}

	UE_LOG(LogCastle, Log, TEXT("%s: zip to %s, %.0f cm at %.0f cm/s (%.2f s), ignoring %s%s"),
		*GetNameSafe(Character), *GetNameSafe(Anchor), ZipLength, ZipSpeed, ZipLength / ZipSpeed,
		*GetNameSafe(Supports.Num() > 0 ? Supports[0] : nullptr),
		Supports.Num() > 1 ? *FString::Printf(TEXT(" and %d more"), Supports.Num() - 1) : TEXT(""));
	return true;
}

void UGrappleComponent::AdvanceZip(float DeltaSeconds)
{
	ACharacter* Character = GetCharacter();
	if (!bZipping || !Character)
	{
		return;
	}

	ZipTravelled = FMath::Min(ZipTravelled + ZipSpeed * DeltaSeconds, ZipLength);
	const FVector Target = ZipLength > 0.f
		? ZipStart + (ZipEnd - ZipStart) / ZipLength * ZipTravelled
		: ZipEnd;

	FHitResult Hit;
	Character->SetActorLocation(Target, /*bSweep=*/true, &Hit);
	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		// The zip moves the capsule itself; any velocity left in the movement component would add to it.
		Movement->Velocity = FVector::ZeroVector;
	}
	if (Hit.bBlockingHit)
	{
		UE_LOG(LogCastle, Log, TEXT("%s: zip to %s blocked by %s at %s (%.0f%% along); dropping"),
			*GetNameSafe(Character), *GetNameSafe(ZipAnchor.Get()), *GetNameSafe(Hit.GetActor()),
			*Hit.Location.ToCompactString(), ZipProgress * 100.f);
		CancelZip();
		return;
	}

	ZipProgress = ZipLength > 0.f ? ZipTravelled / ZipLength : 1.f;
	if (ZipTravelled >= ZipLength)
	{
		FinishZip();
	}
}

void UGrappleComponent::EndZipMovement()
{
	ACharacter* Character = GetCharacter();
	if (!Character)
	{
		return;
	}
	SetSupportsIgnored(false);
	ZipIgnoredSupports.Reset();
	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		Movement->GravityScale = PreZipGravityScale;
		Movement->Velocity = FVector::ZeroVector;
	}
}

void UGrappleComponent::FinishZip()
{
	ACharacter* Character = GetCharacter();
	AGrappleAnchor* Anchor = ZipAnchor.Get();
	bZipping = false;
	ZipProgress = 0.f;
	EndZipMovement();
	ZipAnchor.Reset();

	if (Character && Character->GetCharacterMovement())
	{
		Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}
	if (ACastleCharacter* Castle = Cast<ACastleCharacter>(Character))
	{
		Castle->NotifyGrappleLanded();
	}
	UE_LOG(LogCastle, Log, TEXT("%s: grapple landed at %s on %s"), *GetNameSafe(Character),
		Character ? *Character->GetActorLocation().ToCompactString() : TEXT("?"), *GetNameSafe(Anchor));
	OnGrappleLanded.Broadcast(Anchor);
}

void UGrappleComponent::CancelZip()
{
	if (!bZipping)
	{
		return;
	}

	AGrappleAnchor* Anchor = ZipAnchor.Get();
	bZipping = false;
	ZipProgress = 0.f;
	EndZipMovement();
	ZipAnchor.Reset();

	ACharacter* Character = GetCharacter();
	if (Character && Character->GetCharacterMovement())
	{
		Character->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	}
	OnGrappleCancelled.Broadcast(Anchor);
}

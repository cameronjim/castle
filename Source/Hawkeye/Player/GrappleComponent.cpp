// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/GrappleComponent.h"

#include "Camera/CameraComponent.h"
#include "Hawkeye.h"
#include "Combat/ArrowDefinition.h"
#include "Combat/BowComponent.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/HawkeyeCharacter.h"
#include "Player/InventoryComponent.h"
#include "World/GrappleAnchor.h"
#include "World/GrappleArrowProjectile.h"

namespace HawkeyeGrapple
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
	UE_LOG(LogHawkeye, Log, TEXT("%s: %d grapple anchors in %d grid cells of %.0f cm"),
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

	const int32 Traces = FMath::Min(Candidates.Num(), HawkeyeGrapple::MaxSightTraces);
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
	const AHawkeyeCharacter* Hawkeye = Cast<AHawkeyeCharacter>(GetOwner());
	if (const UCameraComponent* Camera = Hawkeye ? Hawkeye->GetFollowCamera() : nullptr)
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
		UE_LOG(LogHawkeye, Log, TEXT("%s: no grapple arrows left"), *GetNameSafe(GetOwner()));
		return false;
	}
	++UseCount;

	const AActor* Owner = GetOwner();
	AGrappleArrowProjectile* Arrow = SpawnGrappleArrow(Definition, Anchor);

	UE_LOG(LogHawkeye, Log, TEXT("%s: grapple arrow at %s, %.0f cm away, %d left"), *GetNameSafe(Owner),
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
	if (bZipping)
	{
		// A chain: the arrow has to beat the rest of the line, or she lands first and it is a
		// fresh zip off the roof rather than a redirect in the air.
		Arrow->Launch(Anchor, this, ChainArrowSpeed);
	}
	else
	{
		Arrow->Launch(Anchor, this);
	}
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
			UE_LOG(LogHawkeye, Warning, TEXT("%s: %s's projectile %s is not a grapple arrow; using %s."),
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
		UE_LOG(LogHawkeye, Log, TEXT("%s: recovered %d grapple arrow(s), %d now"), *GetNameSafe(Owner), Recovered,
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
			Landing - FVector(0.f, 0.f, HawkeyeGrapple::SupportProbeDepth), ECC_WorldStatic, Params)
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

void UGrappleComponent::FindStartSupports(const FVector& Start, TArray<AActor*>& OutSupports) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GrappleStartSupport), true, GetOwner());
	const ACharacter* Character = GetCharacter();
	const float HalfHeight = Character && Character->GetCapsuleComponent()
		? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f;
	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, Start, Start - FVector(0.f, 0.f, HalfHeight + 50.f), ECC_WorldStatic, Params)
		&& Hit.GetActor())
	{
		OutSupports.AddUnique(Hit.GetActor());
	}
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Start, FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic),
		FCollisionShape::MakeSphere(ZipStartIgnoreRadius), Params);
	for (const FOverlapResult& Overlap : Overlaps)
	{
		if (AActor* Actor = Overlap.GetActor())
		{
			OutSupports.AddUnique(Actor);
		}
	}
}

float UGrappleComponent::GetStartReleaseDistance() const
{
	const ACharacter* Character = GetCharacter();
	const float Radius = Character && Character->GetCapsuleComponent() ? Character->GetCapsuleComponent()->GetScaledCapsuleRadius() : 34.f;
	return ZipStartIgnoreRadius + Radius;
}

FVector UGrappleComponent::ComputeZipLaunch(const FVector& Start, bool bFromGround) const
{
	return bFromGround ? Start + FVector(0.f, 0.f, ZipLaunchHeight) : Start;
}

bool UGrappleComponent::IsZipClear(const FVector& From, const AGrappleAnchor* Anchor, bool bFromGround, AActor** OutBlocker) const
{
	const UWorld* World = GetWorld();
	const ACharacter* Character = GetCharacter();
	const UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
	if (OutBlocker)
	{
		*OutBlocker = nullptr;
	}
	if (!World || !Capsule || !Anchor)
	{
		return false;
	}
	const FVector Launch = ComputeZipLaunch(From, bFromGround);
	const FVector End = ComputeZipEnd(Anchor);
	TArray<AActor*> AnchorSupports;
	FindAnchorSupports(Anchor, AnchorSupports);
	TArray<AActor*> StartSupports;
	FindStartSupports(From, StartSupports);

	// Where the start supports count again: the first point on the line that far from From.
	const float Release = GetStartReleaseDistance();
	const FVector Line = End - Launch;
	const float Length = Line.Size();
	FVector ReleasePoint = End;
	if (Length > KINDA_SMALL_NUMBER)
	{
		const FVector Dir = Line / Length;
		const FVector Offset = Launch - From;
		const float B = FVector::DotProduct(Offset, Dir);
		const float C = Offset.SizeSquared() - Release * Release;
		const float Disc = B * B - C;
		const float S = Disc >= 0.f ? -B + FMath::Sqrt(Disc) : 0.f;
		ReleasePoint = Launch + Dir * FMath::Clamp(S, 0.f, Length);
	}

	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight());
	FCollisionResponseParams Responses(Capsule->GetCollisionResponseToChannels());
	auto Sweep = [&](const FVector& A, const FVector& B, bool bIgnoreStart) -> bool
	{
		if (A.Equals(B, 0.1f))
		{
			return true;
		}
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GrappleZipClear), false, Character);
		Params.AddIgnoredActor(Anchor);
		Params.AddIgnoredActors(AnchorSupports);
		if (bIgnoreStart)
		{
			Params.AddIgnoredActors(StartSupports);
		}
		FHitResult Hit;
		if (World->SweepSingleByChannel(Hit, A, B, FQuat::Identity, Capsule->GetCollisionObjectType(), Shape, Params, Responses))
		{
			if (OutBlocker)
			{
				*OutBlocker = Hit.GetActor();
			}
			return false;
		}
		return true;
	};
	return Sweep(Launch, ReleasePoint, true) && Sweep(ReleasePoint, End, false);
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
	for (const TWeakObjectPtr<AActor>& Support : ZipStartSupports)
	{
		if (AActor* Actor = Support.Get())
		{
			// Released start supports are already back; letting go of everything includes them.
			if (!bIgnore || bStartSupportsIgnored)
			{
				Capsule->IgnoreActorWhenMoving(Actor, bIgnore);
			}
		}
	}
}

void UGrappleComponent::ReleaseStartSupports()
{
	const ACharacter* Character = GetCharacter();
	UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
	if (!Capsule || !bStartSupportsIgnored)
	{
		return;
	}
	bStartSupportsIgnored = false;
	for (const TWeakObjectPtr<AActor>& Support : ZipStartSupports)
	{
		AActor* Actor = Support.Get();
		if (Actor && !ZipIgnoredSupports.Contains(Support))
		{
			Capsule->IgnoreActorWhenMoving(Actor, false);
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

	// From the ground there is a hop to the launch point; a chain or a mid-air zip goes straight.
	const bool bFromGround = !bZipping && Movement->IsMovingOnGround();
	const bool bRedirect = bZipping && !bHopping;
	// A chain in the air is often right at the old anchor's parapet: that building counts as
	// where she starts from, ignored until she is clear of it like her own roof on a fresh zip.
	TArray<TWeakObjectPtr<AActor>> OldAnchorSupports;
	if (bRedirect)
	{
		OldAnchorSupports = ZipIgnoredSupports;
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

	if (AHawkeyeCharacter* Hawkeye = Cast<AHawkeyeCharacter>(Character))
	{
		Hawkeye->StopAim();
	}

	ZipAnchor = Anchor;
	ZipStart = Character->GetActorLocation();
	ZipLaunch = ComputeZipLaunch(ZipStart, bFromGround);
	ZipEnd = ComputeZipEnd(Anchor);
	ZipLength = FVector::Dist(ZipLaunch, ZipEnd);
	ZipTravelled = 0.f;
	ZipProgress = 0.f;
	bZipping = true;
	bHopping = bFromGround && ZipHopSeconds > 0.f;
	HopElapsed = 0.f;

	// A chain in the air keeps moving the way it was going and turns onto the new line over
	// RedirectBlendSeconds; anything else starts on its line.
	const FVector NewDirection = (ZipEnd - ZipLaunch).GetSafeNormal();
	bRedirecting = bRedirect && RedirectBlendSeconds > 0.f && !ZipDirection.IsNearlyZero();
	RedirectElapsed = 0.f;
	RedirectFromDirection = bRedirecting ? ZipDirection : NewDirection;
	if (!bRedirecting)
	{
		ZipDirection = NewDirection;
	}
	if (bRedirect)
	{
		++RedirectCount;
	}

	// The line ends on the anchor's roof and clips its parapet on the way in; that building is
	// expected, and so is the roof and parapet she leaves from until she is clear of them.
	// Anything else in the way cancels the zip.
	TArray<AActor*> Supports;
	FindAnchorSupports(Anchor, Supports);
	ZipIgnoredSupports.Reset();
	for (AActor* Support : Supports)
	{
		ZipIgnoredSupports.Add(Support);
	}
	TArray<AActor*> StartSupports;
	FindStartSupports(ZipStart, StartSupports);
	ZipStartSupports.Reset();
	for (AActor* Support : StartSupports)
	{
		ZipStartSupports.Add(Support);
	}
	for (const TWeakObjectPtr<AActor>& Support : OldAnchorSupports)
	{
		if (Support.IsValid())
		{
			ZipStartSupports.AddUnique(Support);
		}
	}
	bStartSupportsIgnored = true;
	SetSupportsIgnored(true);

	Movement->GravityScale = 0.f;
	Movement->Velocity = FVector::ZeroVector;
	Movement->SetMovementMode(MOVE_Flying);

	const FVector Direction = (ZipEnd - ZipStart).GetSafeNormal2D();
	if (!Direction.IsNearlyZero() && !bRedirecting)
	{
		Character->SetActorRotation(FRotator(0.f, Direction.Rotation().Yaw, 0.f));
	}

	UE_LOG(LogHawkeye, Log, TEXT("%s: zip to %s, %.0f cm at %.0f cm/s (%.2f s), %s, line %+.0f cm, ignoring %s%s and %d start support(s)"),
		*GetNameSafe(Character), *GetNameSafe(Anchor), ZipLength, ZipSpeed, ZipLength / ZipSpeed,
		bRedirecting ? *FString::Printf(TEXT("mid-air redirect over %.2f s"), RedirectBlendSeconds)
			: (bHopping ? *FString::Printf(TEXT("hop %.0f cm in %.2f s"), ZipLaunchHeight, ZipHopSeconds) : TEXT("no hop")),
		ZipEnd.Z - ZipLaunch.Z, *GetNameSafe(Supports.Num() > 0 ? Supports[0] : nullptr),
		Supports.Num() > 1 ? *FString::Printf(TEXT(" and %d more"), Supports.Num() - 1) : TEXT(""), StartSupports.Num());
	return true;
}

void UGrappleComponent::AdvanceZip(float DeltaSeconds)
{
	ACharacter* Character = GetCharacter();
	if (!bZipping || !Character)
	{
		return;
	}

	if (bHopping)
	{
		// Up to the launch point, easing out, through whatever she stood on or beside.
		HopElapsed += DeltaSeconds;
		const float Alpha = FMath::Clamp(HopElapsed / ZipHopSeconds, 0.f, 1.f);
		Character->SetActorLocation(FMath::Lerp(ZipStart, ZipLaunch, FMath::Sin(Alpha * HALF_PI)), false, nullptr,
			ETeleportType::None);
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			Movement->Velocity = FVector::ZeroVector;
		}
		bHopping = Alpha < 1.f;
		return;
	}

	// Head for the landing point from wherever she is. On a straight line that is the line itself;
	// after a chain the direction swings from the old line onto the new over RedirectBlendSeconds.
	const FVector Current = Character->GetActorLocation();
	const FVector ToEnd = ZipEnd - Current;
	const float Remaining = ToEnd.Size();
	const float Step = ZipSpeed * DeltaSeconds;
	FVector Direction = Remaining > KINDA_SMALL_NUMBER ? ToEnd / Remaining : ZipDirection;
	if (bRedirecting)
	{
		RedirectElapsed += DeltaSeconds;
		const float Alpha = FMath::Clamp(RedirectElapsed / RedirectBlendSeconds, 0.f, 1.f);
		const FVector Blended = RedirectFromDirection * (1.f - Alpha) + Direction * Alpha;
		Direction = Blended.IsNearlyZero() ? Direction : Blended.GetSafeNormal();
		bRedirecting = Alpha < 1.f;
		const FVector Flat = Direction.GetSafeNormal2D();
		if (!Flat.IsNearlyZero())
		{
			Character->SetActorRotation(FRotator(0.f, Flat.Rotation().Yaw, 0.f));
		}
	}
	const bool bArrives = Remaining <= Step;
	const FVector Target = bArrives ? ZipEnd : Current + Direction * Step;
	ZipDirection = Direction;
	ZipTravelled = FMath::Clamp(ZipLength - FVector::Dist(Target, ZipEnd), ZipTravelled, ZipLength);
	if (bArrives)
	{
		ZipTravelled = ZipLength;
	}

	FHitResult Hit;
	Character->SetActorLocation(Target, /*bSweep=*/true, &Hit);
	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		// The zip moves the capsule itself; any velocity left in the movement component would add to it.
		Movement->Velocity = FVector::ZeroVector;
	}
	if (Hit.bBlockingHit)
	{
		UE_LOG(LogHawkeye, Log, TEXT("%s: zip to %s blocked by %s at %s (%.0f%% along); dropping"),
			*GetNameSafe(Character), *GetNameSafe(ZipAnchor.Get()), *GetNameSafe(Hit.GetActor()),
			*Hit.Location.ToCompactString(), ZipProgress * 100.f);
		CancelZip();
		return;
	}

	if (bStartSupportsIgnored && FVector::Dist(Character->GetActorLocation(), ZipStart) > GetStartReleaseDistance())
	{
		ReleaseStartSupports();
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
	ZipStartSupports.Reset();
	bStartSupportsIgnored = false;
	bHopping = false;
	bRedirecting = false;
	ZipDirection = FVector::ZeroVector;
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
	if (AHawkeyeCharacter* Hawkeye = Cast<AHawkeyeCharacter>(Character))
	{
		Hawkeye->NotifyGrappleLanded();
	}
	UE_LOG(LogHawkeye, Log, TEXT("%s: grapple landed at %s on %s"), *GetNameSafe(Character),
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
	ACharacter* Character = GetCharacter();
	UE_LOG(LogHawkeye, Log, TEXT("%s: let go of the zip to %s at %.0f%% (%s); falling"), *GetNameSafe(Character),
		*GetNameSafe(Anchor), ZipProgress * 100.f, Character ? *Character->GetActorLocation().ToCompactString() : TEXT("?"));
	bZipping = false;
	ZipProgress = 0.f;
	EndZipMovement();
	ZipAnchor.Reset();

	if (Character && Character->GetCharacterMovement())
	{
		Character->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	}
	OnGrappleCancelled.Broadcast(Anchor);
}

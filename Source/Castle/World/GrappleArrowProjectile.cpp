// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/GrappleArrowProjectile.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Player/GrappleComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "World/GrappleAnchor.h"

AGrappleArrowProjectile::AGrappleArrowProjectile()
{
	PrimaryActorTick.bCanEverTick = true;

	// A 70 cm shaft 2 cm thick along +X: the engine cylinder stands along Z, so it is pitched over.
	Shaft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Shaft"));
	RootComponent = Shaft;
	Shaft->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Shaft->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cylinder.Succeeded())
	{
		Shaft->SetStaticMesh(Cylinder.Object);
	}
	Shaft->SetWorldScale3D(FVector(0.02f, 0.02f, 0.7f));
}

void AGrappleArrowProjectile::Launch(AGrappleAnchor* Anchor, UGrappleComponent* InGrapple)
{
	TargetAnchor = Anchor;
	Grapple = InGrapple;
	bArrived = false;
	if (Anchor)
	{
		const FVector Direction = (Anchor->GetMarkerLocation() - GetActorLocation()).GetSafeNormal();
		SetActorRotation(FRotationMatrix::MakeFromZ(Direction).Rotator());
	}
}

void AGrappleArrowProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Advance(DeltaSeconds);
}

void AGrappleArrowProjectile::Advance(float DeltaSeconds)
{
	if (bArrived)
	{
		return;
	}

	const AGrappleAnchor* Anchor = TargetAnchor.Get();
	if (!IsValid(Anchor))
	{
		// The anchor went away mid-flight; the arrow is lost.
		Destroy();
		return;
	}

	const FVector Target = Anchor->GetMarkerLocation();
	const FVector ToTarget = Target - GetActorLocation();
	const float Step = Speed * DeltaSeconds;
	if (ToTarget.Size() <= Step)
	{
		SetActorLocation(Target);
		Arrive();
		return;
	}
	SetActorLocation(GetActorLocation() + ToTarget.GetSafeNormal() * Step);
}

void AGrappleArrowProjectile::Arrive()
{
	bArrived = true;
	SetActorTickEnabled(false);

	AGrappleAnchor* Anchor = TargetAnchor.Get();
	if (Anchor)
	{
		Anchor->AddStuckArrow(this);
	}
	if (UGrappleComponent* Component = Grapple.Get())
	{
		Component->HandleArrowArrived(this, Anchor);
	}
}

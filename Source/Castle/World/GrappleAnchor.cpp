// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/GrappleAnchor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "World/GrappleArrowProjectile.h"

AGrappleAnchor::AGrappleAnchor()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	RootComponent = Root;

	// A 40 cm cube sitting on the origin. The engine cube is the one mesh the C++ can rely on, so
	// an anchor spawned without the Blueprint (tests, a hand-placed actor) is still visible.
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetMobility(EComponentMobility::Static);
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, 20.f));
	Mesh->SetRelativeScale3D(FVector(0.4f));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		Mesh->SetStaticMesh(Cube.Object);
	}

	LandingPoint = CreateDefaultSubobject<USceneComponent>(TEXT("LandingPoint"));
	LandingPoint->SetupAttachment(Root);
	LandingPoint->SetMobility(EComponentMobility::Static);
	LandingPoint->SetRelativeLocation(FVector(60.f, 0.f, 0.f));
}

FVector AGrappleAnchor::GetLandingLocation() const
{
	return LandingPoint ? LandingPoint->GetComponentLocation() : GetActorLocation();
}

FVector AGrappleAnchor::GetMarkerLocation() const
{
	return GetActorLocation() + GetActorUpVector() * MarkerHeight;
}

void AGrappleAnchor::AddStuckArrow(AGrappleArrowProjectile* Arrow)
{
	++StuckArrowCount;
	if (Arrow)
	{
		StuckArrows.Add(Arrow);
	}
}

int32 AGrappleAnchor::RecoverStuckArrows()
{
	const int32 Recovered = StuckArrowCount;
	for (AGrappleArrowProjectile* Arrow : StuckArrows)
	{
		if (IsValid(Arrow))
		{
			Arrow->Destroy();
		}
	}
	StuckArrows.Reset();
	StuckArrowCount = 0;
	return Recovered;
}

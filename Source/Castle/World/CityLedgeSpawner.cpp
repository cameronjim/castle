// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/CityLedgeSpawner.h"

#include "Castle.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "HAL/PlatformTime.h"
#include "Player/GrappleComponent.h"
#include "World/CityLedgeData.h"
#include "World/GrappleAnchor.h"

const FName ACityLedgeSpawner::LedgeTag(TEXT("CityLedge"));
const FName ACityLedgeSpawner::AnchorTag(TEXT("CityAnchor"));

namespace CastleCitySpawn
{
	static const FName CityTag(TEXT("City"));
	static const TCHAR* LedgeLabelPrefix = TEXT("City_Ledge_");
	static const TCHAR* AnchorLabelPrefix = TEXT("City_Anchor_");
}

ACityLedgeSpawner::ACityLedgeSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Static);
}

void ACityLedgeSpawner::BeginPlay()
{
	Super::BeginPlay();
	if (SpawnedLedges.Num() == 0 && SpawnedAnchors.Num() == 0)
	{
		SpawnAll();
	}
}

void ACityLedgeSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// On a level change or quit the world takes everything down anyway.
	if (EndPlayReason == EEndPlayReason::Destroyed)
	{
		DestroySpawned();
	}
	Super::EndPlay(EndPlayReason);
}

void ACityLedgeSpawner::Destroyed()
{
	DestroySpawned();
	Super::Destroyed();
}

void ACityLedgeSpawner::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
#if WITH_EDITOR
	const UWorld* World = GetWorld();
	if (!World || World->IsGameWorld() || HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		return;
	}
	if (bSpawnInEditor)
	{
		if (SpawnedLedges.Num() == 0 && SpawnedAnchors.Num() == 0)
		{
			SpawnAll();
		}
	}
	else
	{
		DestroySpawned();
	}
#endif
}

void ACityLedgeSpawner::MakeTraceOnly(AActor* Ledge)
{
	TInlineComponentArray<UPrimitiveComponent*> Primitives(Ledge);
	for (UPrimitiveComponent* Primitive : Primitives)
	{
		if (UTextRenderComponent* Text = Cast<UTextRenderComponent>(Primitive))
		{
			Text->SetVisibility(false);
			Text->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			continue;
		}
		if (!Primitive->IsA<UStaticMeshComponent>())
		{
			continue;
		}
		Primitive->SetVisibility(false);
		Primitive->SetCastShadow(false);
		Primitive->SetCollisionObjectType(ECC_WorldStatic);
		Primitive->SetCollisionResponseToAllChannels(ECR_Ignore);
		Primitive->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
		Primitive->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
}

int32 ACityLedgeSpawner::SpawnAll()
{
	UWorld* World = GetWorld();
	if (!World || !Data)
	{
		UE_LOG(LogCastle, Warning, TEXT("%s: no world or no ledge data; nothing spawned."), *GetName());
		return 0;
	}
	const bool bEditorWorld = !World->IsGameWorld();
	int32 Spawned = 0;

	auto MakeParams = [this, bEditorWorld]()
	{
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.bAllowDuringConstructionScript = true;
		Params.ObjectFlags |= RF_Transient;
#if WITH_EDITOR
		Params.bHideFromSceneOutliner = !bEditorWorld;
		Params.bCreateActorPackage = false;
#endif
		return Params;
	};

	// --- Ledges ---------------------------------------------------------------------------------
	double Start = FPlatformTime::Seconds();
	if (LedgeClass && SpawnedLedges.Num() == 0)
	{
		SpawnedLedges.Reserve(Data->Ledges.Num());
		for (const FCityLedgeRecord& Record : Data->Ledges)
		{
			FActorSpawnParameters Params = MakeParams();
			const FName OsmTag(*(TEXT("osm:") + Record.OsmId));
			Params.CustomPreSpawnInitialization = [OsmTag](AActor* Actor)
			{
				Actor->Tags = { CastleCitySpawn::CityTag, LedgeTag, OsmTag };
			};
#if WITH_EDITOR
			const FString Label = FString::Printf(TEXT("%s%s_%d"), CastleCitySpawn::LedgeLabelPrefix, *Record.OsmId, Record.EdgeIndex);
			if (bEditorWorld)
			{
				Params.InitialActorLabel = Label;
			}
#endif
			if (AActor* Ledge = World->SpawnActor(LedgeClass, &Record.Transform, Params))
			{
				MakeTraceOnly(Ledge);
				SpawnedLedges.Add(Ledge);
				++Spawned;
			}
		}
	}
	LastLedgeSpawnSeconds = static_cast<float>(FPlatformTime::Seconds() - Start);

	// --- Anchors --------------------------------------------------------------------------------
	Start = FPlatformTime::Seconds();
	if (AnchorClass && SpawnedAnchors.Num() == 0)
	{
		SpawnedAnchors.Reserve(Data->Anchors.Num());
		for (const FCityAnchorRecord& Record : Data->Anchors)
		{
			FActorSpawnParameters Params = MakeParams();
			const FName OsmTag(*(TEXT("osm:") + Record.OsmId));
			const FVector LandingOffset = Record.LandingOffset;
			// Before registration: the anchor's components are Static, and a registered Static
			// component refuses to move in a game world.
			Params.CustomPreSpawnInitialization = [OsmTag, LandingOffset](AActor* Actor)
			{
				Actor->Tags = { CastleCitySpawn::CityTag, AnchorTag, OsmTag };
				if (const AGrappleAnchor* Anchor = Cast<AGrappleAnchor>(Actor))
				{
					if (USceneComponent* Landing = Anchor->GetLandingPoint())
					{
						Landing->SetRelativeLocation(LandingOffset);
					}
				}
			};
#if WITH_EDITOR
			const FString Label = FString::Printf(TEXT("%s%d"), CastleCitySpawn::AnchorLabelPrefix, Record.Index);
			if (bEditorWorld)
			{
				Params.InitialActorLabel = Label;
			}
#endif
			if (AGrappleAnchor* Anchor = World->SpawnActor<AGrappleAnchor>(AnchorClass, Record.Transform, Params))
			{
				SpawnedAnchors.Add(Anchor);
				++Spawned;
			}
		}
	}
	LastAnchorSpawnSeconds = static_cast<float>(FPlatformTime::Seconds() - Start);

	// Grapple components bucket anchors lazily; one that already looked needs to look again.
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		if (UGrappleComponent* Grapple = It->FindComponentByClass<UGrappleComponent>())
		{
			Grapple->RebuildAnchorGrid();
		}
	}

	UE_LOG(LogCastle, Log, TEXT("%s: spawned %d ledges in %.0f ms and %d anchors in %.0f ms from %s"),
		*GetName(), SpawnedLedges.Num(), LastLedgeSpawnSeconds * 1000.f, SpawnedAnchors.Num(),
		LastAnchorSpawnSeconds * 1000.f, *GetNameSafe(Data));
	return Spawned;
}

void ACityLedgeSpawner::DestroySpawned()
{
	for (AActor* Ledge : SpawnedLedges)
	{
		if (IsValid(Ledge))
		{
			Ledge->Destroy();
		}
	}
	for (AGrappleAnchor* Anchor : SpawnedAnchors)
	{
		if (IsValid(Anchor))
		{
			Anchor->Destroy();
		}
	}
	SpawnedLedges.Reset();
	SpawnedAnchors.Reset();
}

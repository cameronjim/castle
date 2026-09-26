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
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
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

	static FActorSpawnParameters MakeParams(AActor* Owner, bool bEditorWorld)
	{
		FActorSpawnParameters Params;
		Params.Owner = Owner;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.bAllowDuringConstructionScript = true;
		Params.ObjectFlags |= RF_Transient;
#if WITH_EDITOR
		Params.bHideFromSceneOutliner = !bEditorWorld;
		Params.bCreateActorPackage = false;
#endif
		return Params;
	}
}

ACityLedgeSpawner::ACityLedgeSpawner()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Static);
}

bool ACityLedgeSpawner::IsSpawnComplete() const
{
	return Data && SpawnedLedges.Num() >= Data->Ledges.Num() && SpawnedAnchors.Num() >= Data->Anchors.Num();
}

void ACityLedgeSpawner::BeginPlay()
{
	Super::BeginPlay();
	if (!Data || !GetWorld())
	{
		UE_LOG(LogCastle, Warning, TEXT("%s: no ledge data; nothing spawned."), *GetName());
		return;
	}

	SpawnAnchors();

	const double Start = FPlatformTime::Seconds();
	QueueLedges(FindFocus());
	SpawnQueuedLedges(ImmediateRadius, TNumericLimits<double>::Max());
	LoadLedgeSeconds = static_cast<float>(FPlatformTime::Seconds() - Start);
	TotalLedgeSeconds = LoadLedgeSeconds;

	UE_LOG(LogCastle, Log, TEXT("%s: at load %d anchors in %.0f ms, %d of %d ledges within %.0f m in %.0f ms; the rest at %.1f ms a frame"),
		*GetName(), SpawnedAnchors.Num(), AnchorSeconds * 1000.f, SpawnedLedges.Num(), Data->Ledges.Num(),
		ImmediateRadius / 100.f, LoadLedgeSeconds * 1000.f, FrameBudgetMs);
	SetActorTickEnabled(!IsSpawnComplete());
}

void ACityLedgeSpawner::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Data || !bLedgesQueued)
	{
		SetActorTickEnabled(false);
		return;
	}
	const double Start = FPlatformTime::Seconds();
	SpawnQueuedLedges(TNumericLimits<float>::Max(), Start + FrameBudgetMs / 1000.0);
	TotalLedgeSeconds += static_cast<float>(FPlatformTime::Seconds() - Start);
	++BackgroundFrames;
	if (LedgeQueueNext >= LedgeQueue.Num())
	{
		SetActorTickEnabled(false);
		UE_LOG(LogCastle, Log, TEXT("%s: all %d ledges out; %.0f ms at load, %.0f ms in total over %d more frames"),
			*GetName(), SpawnedLedges.Num(), LoadLedgeSeconds * 1000.f, TotalLedgeSeconds * 1000.f, BackgroundFrames);
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
		if (!IsSpawnComplete())
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

FVector ACityLedgeSpawner::FindFocus() const
{
	UWorld* World = GetWorld();
	if (const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr)
	{
		if (const APawn* Pawn = PC->GetPawn())
		{
			return Pawn->GetActorLocation();
		}
	}
	if (World)
	{
		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			return It->GetActorLocation();
		}
	}
	return GetActorLocation();
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

AActor* ACityLedgeSpawner::SpawnLedge(const FCityLedgeRecord& Record)
{
	UWorld* World = GetWorld();
	if (!World || !LedgeClass)
	{
		return nullptr;
	}
	const bool bEditorWorld = !World->IsGameWorld();
	FActorSpawnParameters Params = CastleCitySpawn::MakeParams(this, bEditorWorld);
	const FName OsmTag(*(TEXT("osm:") + Record.OsmId));
	Params.CustomPreSpawnInitialization = [OsmTag](AActor* Actor)
	{
		Actor->Tags = { CastleCitySpawn::CityTag, LedgeTag, OsmTag };
	};
#if WITH_EDITOR
	const FString Label = bEditorWorld
		? FString::Printf(TEXT("%s%s_%d"), CastleCitySpawn::LedgeLabelPrefix, *Record.OsmId, Record.EdgeIndex) : FString();
	if (bEditorWorld)
	{
		Params.InitialActorLabel = Label;
	}
#endif
	// Deferred, then finished with a non-default transform: a plain SpawnActor of a Blueprint whose
	// root comes from its construction script takes the template's scale, not ours.
	Params.bDeferConstruction = true;
	AActor* Ledge = World->SpawnActor(LedgeClass, &Record.Transform, Params);
	if (Ledge)
	{
		Ledge->FinishSpawning(Record.Transform, /*bIsDefaultTransform=*/false);
		MakeTraceOnly(Ledge);
		SpawnedLedges.Add(Ledge);
	}
	return Ledge;
}

void ACityLedgeSpawner::QueueLedges(const FVector& Focus)
{
	LedgeQueue.Reset();
	LedgeQueueDistance.Reset();
	LedgeQueueNext = 0;
	bLedgesQueued = true;
	if (!Data)
	{
		return;
	}
	TArray<TPair<float, int32>> Order;
	Order.Reserve(Data->Ledges.Num());
	for (int32 Index = 0; Index < Data->Ledges.Num(); ++Index)
	{
		Order.Emplace(FVector::Dist2D(Data->Ledges[Index].Transform.GetLocation(), Focus), Index);
	}
	Order.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; });
	for (const TPair<float, int32>& Entry : Order)
	{
		LedgeQueueDistance.Add(Entry.Key);
		LedgeQueue.Add(Entry.Value);
	}
}

void ACityLedgeSpawner::SpawnQueuedLedges(float MaxDistance, double Deadline)
{
	if (!Data)
	{
		return;
	}
	while (LedgeQueueNext < LedgeQueue.Num())
	{
		if (LedgeQueueDistance[LedgeQueueNext] > MaxDistance || FPlatformTime::Seconds() >= Deadline)
		{
			return;
		}
		SpawnLedge(Data->Ledges[LedgeQueue[LedgeQueueNext]]);
		++LedgeQueueNext;
	}
}

void ACityLedgeSpawner::SpawnAnchors()
{
	UWorld* World = GetWorld();
	if (!World || !Data || !AnchorClass || SpawnedAnchors.Num() > 0)
	{
		return;
	}
	const bool bEditorWorld = !World->IsGameWorld();
	const double Start = FPlatformTime::Seconds();
	SpawnedAnchors.Reserve(Data->Anchors.Num());
	for (const FCityAnchorRecord& Record : Data->Anchors)
	{
		FActorSpawnParameters Params = CastleCitySpawn::MakeParams(this, bEditorWorld);
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
		const FString Label = bEditorWorld ? FString::Printf(TEXT("%s%d"), CastleCitySpawn::AnchorLabelPrefix, Record.Index) : FString();
		if (bEditorWorld)
		{
			Params.InitialActorLabel = Label;
		}
#endif
		if (AGrappleAnchor* Anchor = World->SpawnActor<AGrappleAnchor>(AnchorClass, Record.Transform, Params))
		{
			SpawnedAnchors.Add(Anchor);
		}
	}
	AnchorSeconds = static_cast<float>(FPlatformTime::Seconds() - Start);

	// Grapple components bucket anchors lazily; one that already looked needs to look again.
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		if (UGrappleComponent* Grapple = It->FindComponentByClass<UGrappleComponent>())
		{
			Grapple->RebuildAnchorGrid();
		}
	}
}

int32 ACityLedgeSpawner::SpawnAll()
{
	if (!Data || !GetWorld())
	{
		UE_LOG(LogCastle, Warning, TEXT("%s: no world or no ledge data; nothing spawned."), *GetName());
		return 0;
	}
	const int32 Before = SpawnedLedges.Num() + SpawnedAnchors.Num();
	SpawnAnchors();

	const double Start = FPlatformTime::Seconds();
	if (!bLedgesQueued)
	{
		QueueLedges(FindFocus());
	}
	SpawnQueuedLedges(TNumericLimits<float>::Max(), TNumericLimits<double>::Max());
	LoadLedgeSeconds = static_cast<float>(FPlatformTime::Seconds() - Start);
	TotalLedgeSeconds = LoadLedgeSeconds;
	SetActorTickEnabled(false);

	UE_LOG(LogCastle, Log, TEXT("%s: SpawnAll: %d ledges in %.0f ms and %d anchors in %.0f ms from %s"),
		*GetName(), SpawnedLedges.Num(), LoadLedgeSeconds * 1000.f, SpawnedAnchors.Num(), AnchorSeconds * 1000.f,
		*GetNameSafe(Data));
	return SpawnedLedges.Num() + SpawnedAnchors.Num() - Before;
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
	LedgeQueue.Reset();
	LedgeQueueDistance.Reset();
	LedgeQueueNext = 0;
	bLedgesQueued = false;
}

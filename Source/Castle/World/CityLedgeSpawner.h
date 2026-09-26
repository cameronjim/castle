// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CityLedgeSpawner.generated.h"

class AGrappleAnchor;
class UCityLedgeData;
struct FCityLedgeRecord;

/**
 * Spawns the district's roof-edge ledges and grapple anchors from a UCityLedgeData at load, so the
 * map does not carry five thousand saved actors (claude-docs/gameplay-semantics.md, traversal).
 *
 * BeginPlay spawns every anchor (and rebuilds every grapple component's anchor grid) and every
 * ledge within ImmediateRadius of the player, then the rest of the ledges nearest first, at most
 * FrameBudgetMs of each frame, logging both times. Each ledge is a Blueprint whose construction
 * script costs about 0.3 ms, so all 3,700 at once would hitch the load by well over a second.
 * SpawnAll() does everything at once; verify_city.py calls it. In the editor the actors appear
 * only while bSpawnInEditor is on; editor-spawned actors are transient and never saved.
 *
 * Every ledge is LedgeClass (BP_TraversableBlock) with its meshes hidden and blocking only the
 * Traversable channel (ECC_GameTraceChannel1), tagged City, CityLedge and osm:<id>. Every anchor
 * is AnchorClass (BP_GrappleAnchor) tagged City, CityAnchor and osm:<id>. In an editor world they
 * also get their generator labels (City_Ledge_<osm id>_<edge>, City_Anchor_<n>).
 */
UCLASS(Blueprintable, BlueprintType)
class CASTLE_API ACityLedgeSpawner : public AActor
{
	GENERATED_BODY()

public:
	ACityLedgeSpawner();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "City")
	TObjectPtr<UCityLedgeData> Data;

	/** What each ledge record spawns: the sample's LevelBlock_Traversable child. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "City")
	TSubclassOf<AActor> LedgeClass;

	/** What each anchor record spawns. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "City")
	TSubclassOf<AGrappleAnchor> AnchorClass;

	/** Show the spawned actors in the editor viewport (transient, never saved). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "City")
	bool bSpawnInEditor = false;

	/** At BeginPlay, ledges this close to the player (2D) spawn before the first frame, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "City", meta = (ClampMin = "0.0"))
	float ImmediateRadius = 10000.f;

	/** After BeginPlay, how much of each frame the remaining ledges may take, ms. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "City", meta = (ClampMin = "0.5"))
	float FrameBudgetMs = 4.f;

	/** Spawns every ledge and anchor that is not already out, at once. Returns how many it spawned. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "City")
	int32 SpawnAll();

	/** Destroys everything this spawner spawned. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "City")
	void DestroySpawned();

	/** Every ledge and anchor is out. */
	UFUNCTION(BlueprintPure, Category = "City")
	bool IsSpawnComplete() const;

	UFUNCTION(BlueprintPure, Category = "City")
	int32 GetSpawnedLedgeCount() const { return SpawnedLedges.Num(); }

	UFUNCTION(BlueprintPure, Category = "City")
	int32 GetSpawnedAnchorCount() const { return SpawnedAnchors.Num(); }

	/** The spawned ledges. Transient, so editor actor listings skip them; verify_city reads these. */
	UFUNCTION(BlueprintPure, Category = "City")
	TArray<AActor*> GetSpawnedLedges() const { return TArray<AActor*>(SpawnedLedges); }

	UFUNCTION(BlueprintPure, Category = "City")
	TArray<AGrappleAnchor*> GetSpawnedAnchors() const { return TArray<AGrappleAnchor*>(SpawnedAnchors); }

	/** Wall time spent spawning ledges before the first frame (all of them for SpawnAll), s. */
	UFUNCTION(BlueprintPure, Category = "City")
	float GetLoadLedgeSpawnSeconds() const { return LoadLedgeSeconds; }

	/** Wall time spent spawning ledges in total, load plus the per-frame remainder, s. */
	UFUNCTION(BlueprintPure, Category = "City")
	float GetTotalLedgeSpawnSeconds() const { return TotalLedgeSeconds; }

	/** Wall time spent spawning anchors, s. */
	UFUNCTION(BlueprintPure, Category = "City")
	float GetAnchorSpawnSeconds() const { return AnchorSeconds; }

	static const FName LedgeTag;
	static const FName AnchorTag;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Destroyed() override;

	/** Spawns every anchor not yet out and rebuilds the grapple grids. */
	void SpawnAnchors();

	/** Orders the ledge records nearest Focus first and resets the queue. */
	void QueueLedges(const FVector& Focus);

	/** Spawns queued ledges until the queue is empty, one is further than MaxDistance, or Deadline. */
	void SpawnQueuedLedges(float MaxDistance, double Deadline);

	AActor* SpawnLedge(const FCityLedgeRecord& Record);

	/** Hidden, query only, blocking nothing but the Traversable channel; the height labels hidden. */
	static void MakeTraceOnly(AActor* Ledge);

	/** The player pawn, else a PlayerStart, else this actor. */
	FVector FindFocus() const;

	UPROPERTY(Transient, VisibleInstanceOnly, Category = "City")
	TArray<TObjectPtr<AActor>> SpawnedLedges;

	UPROPERTY(Transient, VisibleInstanceOnly, Category = "City")
	TArray<TObjectPtr<AGrappleAnchor>> SpawnedAnchors;

	/** Indices into Data->Ledges, nearest first, and how far down the list spawning has got. */
	TArray<int32> LedgeQueue;
	TArray<float> LedgeQueueDistance;
	int32 LedgeQueueNext = 0;
	bool bLedgesQueued = false;

	float LoadLedgeSeconds = 0.f;
	float TotalLedgeSeconds = 0.f;
	float AnchorSeconds = 0.f;
	int32 BackgroundFrames = 0;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CityLedgeSpawner.generated.h"

class AGrappleAnchor;
class UCityLedgeData;

/**
 * Spawns the district's roof-edge ledges and grapple anchors from a UCityLedgeData at load, so the
 * map does not carry five thousand saved actors (claude-docs/gameplay-semantics.md, traversal).
 *
 * BeginPlay spawns everything (and rebuilds every grapple component's anchor grid), logging the
 * time it took. In the editor the actors appear only while bSpawnInEditor is on, for looking at
 * them; editor-spawned actors are transient and never saved. verify_city.py calls SpawnAll().
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

	/** Spawns every ledge and anchor that is not already out. Returns how many actors it spawned. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "City")
	int32 SpawnAll();

	/** Destroys everything this spawner spawned. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "City")
	void DestroySpawned();

	UFUNCTION(BlueprintPure, Category = "City")
	int32 GetSpawnedLedgeCount() const { return SpawnedLedges.Num(); }

	UFUNCTION(BlueprintPure, Category = "City")
	int32 GetSpawnedAnchorCount() const { return SpawnedAnchors.Num(); }

	/** Wall time of the last SpawnAll's ledges, s. */
	UFUNCTION(BlueprintPure, Category = "City")
	float GetLastLedgeSpawnSeconds() const { return LastLedgeSpawnSeconds; }

	/** Wall time of the last SpawnAll's anchors, s. */
	UFUNCTION(BlueprintPure, Category = "City")
	float GetLastAnchorSpawnSeconds() const { return LastAnchorSpawnSeconds; }

	static const FName LedgeTag;
	static const FName AnchorTag;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Destroyed() override;

	/** Hidden, query only, blocking nothing but the Traversable channel; the height labels hidden. */
	static void MakeTraceOnly(AActor* Ledge);

	UPROPERTY(Transient, VisibleInstanceOnly, Category = "City")
	TArray<TObjectPtr<AActor>> SpawnedLedges;

	UPROPERTY(Transient, VisibleInstanceOnly, Category = "City")
	TArray<TObjectPtr<AGrappleAnchor>> SpawnedAnchors;

	float LastLedgeSpawnSeconds = 0.f;
	float LastAnchorSpawnSeconds = 0.f;
};

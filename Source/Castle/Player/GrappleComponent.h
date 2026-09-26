// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GrappleComponent.generated.h"

class ACharacter;
class AGrappleAnchor;
class AGrappleArrowProjectile;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGrappleLandedSignature, AGrappleAnchor*, Anchor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGrappleCancelledSignature, AGrappleAnchor*, Anchor);

/**
 * The grapple arrow (claude-docs/gameplay-semantics.md, PLANNED: traversal).
 *
 * Every RefreshSeconds it picks the best anchor: enabled, within Range of the character, within
 * ConeDegrees of the camera forward, in line of sight of the camera; the smallest angle wins.
 * TryFire spends a grapple arrow and shoots it at that anchor; when it arrives the character
 * zips along a straight line to the anchor's landing point at ZipSpeed, in Flying mode with
 * gravity and movement input off (the camera still turns). Firing again mid-zip is allowed once
 * ZipProgress reaches ChainMinProgress and redirects the zip to the new anchor. A zip blocked by
 * anything but the anchor's own building stops and drops the character. Arrows stay in the
 * anchor and come back when the character is within RecoverRadius of it.
 *
 * Anchors are bucketed into a GridCellSize grid the first time they are needed, so the query
 * only looks at the cells around the character however many anchors the district has.
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = (Castle), meta = (BlueprintSpawnableComponent))
class CASTLE_API UGrappleComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGrappleComponent();

	/** The anchor a press would fire at, or null when none is valid. */
	UFUNCTION(BlueprintPure, Category = "Grapple")
	AGrappleAnchor* GetTargetAnchor() const { return TargetAnchor.Get(); }

	/**
	 * Fires a grapple arrow at the target anchor. Refused (false) with no target, no arrows, an
	 * arrow already in flight, or mid-zip before ChainMinProgress.
	 */
	UFUNCTION(BlueprintCallable, Category = "Grapple")
	bool TryFire();

	/** Starts the zip to Anchor from where the character is now. The arrow's arrival calls this. */
	UFUNCTION(BlueprintCallable, Category = "Grapple")
	bool StartZip(AGrappleAnchor* Anchor);

	/** Moves the zip DeltaSeconds on. Tick calls this; a test with no ticking calls it directly. */
	void AdvanceZip(float DeltaSeconds);

	/** Stops the zip where it is and lets the character fall. */
	UFUNCTION(BlueprintCallable, Category = "Grapple")
	void CancelZip();

	/** The best anchor seen from ViewLocation looking along ViewForward. Does not change the target. */
	AGrappleAnchor* SelectBestAnchor(const FVector& ViewLocation, const FVector& ViewForward) const;

	/** Re-picks the target from this view. RefreshTarget passes the camera's. */
	void UpdateTarget(const FVector& ViewLocation, const FVector& ViewForward);

	/** Re-picks the target from the owner's camera. Runs every RefreshSeconds. */
	UFUNCTION(BlueprintCallable, Category = "Grapple")
	void RefreshTarget();

	/** Re-buckets every anchor in the world. Call after spawning anchors at runtime. */
	UFUNCTION(BlueprintCallable, Category = "Grapple")
	void RebuildAnchorGrid();

	/** Pulls back the arrows stuck in any anchor within RecoverRadius. Returns how many. */
	UFUNCTION(BlueprintCallable, Category = "Grapple")
	int32 RecoverNearbyArrows();

	/** Arrow arrived at Anchor: start (or redirect) the zip. Called by AGrappleArrowProjectile. */
	void HandleArrowArrived(AGrappleArrowProjectile* Arrow, AGrappleAnchor* Anchor);

	UFUNCTION(BlueprintPure, Category = "Grapple")
	bool IsZipping() const { return bZipping; }

	/** 0 at the start of the line, 1 at the landing point. 0 while not zipping. */
	UFUNCTION(BlueprintPure, Category = "Grapple")
	float GetZipProgress() const { return ZipProgress; }

	/** False only while a zip is running and it is still too early to chain off it. */
	UFUNCTION(BlueprintPure, Category = "Grapple")
	bool CanChain() const { return !bZipping || ZipProgress >= ChainMinProgress; }

	UFUNCTION(BlueprintPure, Category = "Grapple")
	AGrappleAnchor* GetZipAnchor() const { return ZipAnchor.Get(); }

	UFUNCTION(BlueprintPure, Category = "Grapple")
	bool IsArrowInFlight() const { return InFlightArrow.IsValid(); }

	UFUNCTION(BlueprintPure, Category = "Grapple")
	int32 GetGrappleArrows() const { return GrappleArrows; }

	UFUNCTION(BlueprintCallable, Category = "Grapple")
	void SetGrappleArrows(int32 Count) { GrappleArrows = FMath::Max(0, Count); }

	/** Arrows fired this session. The HUD shows the key hint for the first few. */
	UFUNCTION(BlueprintPure, Category = "Grapple")
	int32 GetUseCount() const { return UseCount; }

	/** Length of the current zip's line, cm. 0 while not zipping. */
	UFUNCTION(BlueprintPure, Category = "Grapple")
	float GetZipLength() const { return ZipLength; }

	/** Where the character's capsule centre ends a zip to Anchor. */
	FVector ComputeZipEnd(const AGrappleAnchor* Anchor) const;

	UPROPERTY(BlueprintAssignable, Category = "Grapple")
	FOnGrappleLandedSignature OnGrappleLanded;

	UPROPERTY(BlueprintAssignable, Category = "Grapple")
	FOnGrappleCancelledSignature OnGrappleCancelled;

	/** Farthest an anchor can be from the character, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple", meta = (ClampMin = "0.0"))
	float Range = 2500.f;

	/** Largest angle between the camera forward and the anchor, degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple", meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float ConeDegrees = 30.f;

	/** Zip speed along the line, cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple", meta = (ClampMin = "1.0"))
	float ZipSpeed = 1800.f;

	/** How far along a zip a second arrow may be fired to chain, 0..1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ChainMinProgress = 0.7f;

	/** Seconds between target refreshes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple", meta = (ClampMin = "0.01"))
	float RefreshSeconds = 0.1f;

	/** A line-of-sight hit this close to the anchor still counts as seeing it (the parapet it sits on). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple", meta = (ClampMin = "0.0"))
	float SightTolerance = 60.f;

	/** Within this of an anchor, its stuck arrows go back in the quiver, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple", meta = (ClampMin = "0.0"))
	float RecoverRadius = 200.f;

	/** Side of a spatial grid cell, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple", meta = (ClampMin = "100.0"))
	float GridCellSize = 2500.f;

	/** Where on the character the arrow leaves from, relative to the actor origin. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple")
	FVector ArrowLaunchOffset = FVector(40.f, 0.f, 50.f);

	/** What TryFire spawns. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple")
	TSubclassOf<AGrappleArrowProjectile> ArrowClass;

	/**
	 * Grapple arrows carried. TODO(stage3): read from the quiver once arrows are
	 * UArrowDefinitions in the inventory; until then a large stand-in count.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple", meta = (ClampMin = "0"))
	int32 GrappleArrows = 99;

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** True when Anchor passes range, cone and sight; OutAngleDegrees is its angle from ViewForward. */
	bool IsAnchorValid(const AGrappleAnchor* Anchor, const FVector& ViewLocation, const FVector& ViewForward,
		float& OutAngleDegrees) const;

	/** The camera sees Anchor's marker, give or take SightTolerance. */
	bool HasLineOfSight(const AGrappleAnchor* Anchor, const FVector& ViewLocation) const;

	/** The owner's camera position and forward; the actor's eyes when it has no camera. */
	void GetViewPoint(FVector& OutLocation, FVector& OutForward) const;

	/** Anchors in the grid cells within Range of Location. */
	void GatherNearbyAnchors(const FVector& Location, TArray<AGrappleAnchor*>& OutAnchors) const;

	FIntPoint CellOf(const FVector& Location) const;

	/** Arrived: Walking, landing dip, OnGrappleLanded. */
	void FinishZip();

	/** Puts gravity and the building collision back. Shared by landing and cancelling. */
	void EndZipMovement();

	/** The building the anchor stands on: found under its landing point, ignored by the zip sweep. */
	AActor* FindAnchorSupport(const AGrappleAnchor* Anchor) const;

	ACharacter* GetCharacter() const;

	TWeakObjectPtr<AGrappleAnchor> TargetAnchor;
	TWeakObjectPtr<AGrappleAnchor> ZipAnchor;
	TWeakObjectPtr<AActor> ZipIgnoredSupport;
	TWeakObjectPtr<AGrappleArrowProjectile> InFlightArrow;

	/** Built on the first query (anchors are placed with the level) and by RebuildAnchorGrid. */
	mutable TMap<FIntPoint, TArray<TWeakObjectPtr<AGrappleAnchor>>> AnchorGrid;
	mutable bool bGridBuilt = false;

	/** Buckets every anchor in the world by cell. Const so the lazy build can run from a query. */
	void BuildGrid() const;

	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Grapple")
	bool bZipping = false;

	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Grapple")
	float ZipProgress = 0.f;

	FVector ZipStart = FVector::ZeroVector;
	FVector ZipEnd = FVector::ZeroVector;
	float ZipLength = 0.f;
	float ZipTravelled = 0.f;
	float PreZipGravityScale = 1.f;
	float RefreshAccumulator = 0.f;
	int32 UseCount = 0;
};

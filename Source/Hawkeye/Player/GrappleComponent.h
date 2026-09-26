// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GrappleComponent.generated.h"

class ACharacter;
class AGrappleAnchor;
class AGrappleArrowProjectile;
class UArrowDefinition;
class UInventoryComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGrappleLandedSignature, AGrappleAnchor*, Anchor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGrappleCancelledSignature, AGrappleAnchor*, Anchor);

/**
 * The grapple arrow (claude-docs/gameplay-semantics.md, PLANNED: traversal).
 *
 * Every RefreshSeconds it picks the best anchor: enabled, between MinRange and Range of the character, within
 * ConeDegrees of the camera forward, in line of sight of the camera; the smallest angle wins.
 * TryFire spends a grapple arrow from the quiver (the slot whose arrow has OnHitEffect Grapple)
 * and shoots it through the bow at that anchor, whatever quiver slot is active (Q, or a release
 * with the grapple slot nocked); when it arrives the character
 * zips along a straight line to the anchor's landing point at ZipSpeed, in Flying mode with
 * gravity and movement input off (the camera still turns). From the ground the line starts at a
 * launch point ZipLaunchHeight above where she stood, reached by a ZipHopSeconds hop, so a level
 * or downward line clears her own parapet. Firing again mid-zip is allowed once
 * ZipProgress reaches ChainMinProgress; the chain arrow travels at ChainArrowSpeed (0: it is there
 * at once) so the zip is redirected to the new anchor in the air, the direction of travel turning
 * onto the new line over RedirectBlendSeconds rather than snapping. A zip blocked by
 * anything but the anchor's own building (the one under its landing point, and any static actor
 * within SupportRadius of the anchor) or, until she is ZipStartIgnoreRadius clear of the start,
 * the geometry round where she stood (her own roof and parapet; after a mid-air chain, also the
 * building the old line was landing on), stops and drops the character.
 * Jump or crouch mid-zip lets go (CancelZip). Arrows stay in the
 * anchor and go back in the quiver when the character is within RecoverRadius of it.
 *
 * Anchors are bucketed into a GridCellSize grid the first time they are needed, so the query
 * only looks at the cells around the character however many anchors the district has.
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = (Hawkeye), meta = (BlueprintSpawnableComponent))
class HAWKEYE_API UGrappleComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGrappleComponent();

	/** The anchor a press would fire at, or null when none is valid. */
	UFUNCTION(BlueprintPure, Category = "Grapple")
	AGrappleAnchor* GetTargetAnchor() const { return TargetAnchor.Get(); }

	/**
	 * Fires a grapple arrow at the target anchor, from the quiver's grapple slot, out of the bow hand.
	 * Refused (false, nothing spent) with no target, no grapple arrows, an arrow already in flight,
	 * or mid-zip before ChainMinProgress.
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

	/** Grapple arrows in the owner's quiver; 0 when it carries no grapple slot. */
	UFUNCTION(BlueprintPure, Category = "Grapple")
	int32 GetGrappleArrows() const;

	/** Sets the quiver's grapple count (clamped to its Cap). Does nothing without a grapple slot. */
	UFUNCTION(BlueprintCallable, Category = "Grapple")
	void SetGrappleArrows(int32 Count);

	/** Arrows fired this session. The HUD shows the key hint for the first few. */
	UFUNCTION(BlueprintPure, Category = "Grapple")
	int32 GetUseCount() const { return UseCount; }

	/** Length of the current zip's line, cm. 0 while not zipping. */
	UFUNCTION(BlueprintPure, Category = "Grapple")
	float GetZipLength() const { return ZipLength; }

	/** Where the character's capsule centre ends a zip to Anchor. */
	FVector ComputeZipEnd(const AGrappleAnchor* Anchor) const;

	/** Where the straight line of a zip starts from a capsule centre at Start: ZipLaunchHeight up from the ground, Start itself mid-air. */
	UFUNCTION(BlueprintPure, Category = "Grapple")
	FVector ComputeZipLaunch(const FVector& Start, bool bFromGround) const;

	/**
	 * Offline, the same rules as a real zip: would a zip to Anchor from a capsule centre at From
	 * (on the ground when bFromGround) get there? Sweeps the capsule from the launch point,
	 * ignoring the start supports until ZipStartIgnoreRadius (plus the capsule radius) from From
	 * and the anchor supports all the way. OutBlocker names what stops it.
	 */
	bool IsZipClear(const FVector& From, const AGrappleAnchor* Anchor, bool bFromGround, AActor** OutBlocker = nullptr) const;

	/** In the hop before the straight line. */
	UFUNCTION(BlueprintPure, Category = "Grapple")
	bool IsHopping() const { return bZipping && bHopping; }

	/** Where the current zip's straight line starts. */
	UFUNCTION(BlueprintPure, Category = "Grapple")
	FVector GetZipLaunch() const { return ZipLaunch; }

	/** Where the character stood (or hung in the air) when the current zip started. */
	UFUNCTION(BlueprintPure, Category = "Grapple")
	FVector GetZipStart() const { return ZipStart; }

	/** The geometry round the start is still being ignored. */
	UFUNCTION(BlueprintPure, Category = "Grapple")
	bool IsIgnoringStartSupports() const { return bZipping && bStartSupportsIgnored; }

	/** A chain is still turning the direction of travel onto the new line. */
	UFUNCTION(BlueprintPure, Category = "Grapple")
	bool IsRedirecting() const { return bZipping && bRedirecting; }

	/** Unit direction the zip moved in on its last step; zero before the first. */
	UFUNCTION(BlueprintPure, Category = "Grapple")
	FVector GetZipDirection() const { return ZipDirection; }

	/** Mid-air redirects (chains that took before landing) this session. */
	UFUNCTION(BlueprintPure, Category = "Grapple")
	int32 GetRedirectCount() const { return RedirectCount; }

	UPROPERTY(BlueprintAssignable, Category = "Grapple")
	FOnGrappleLandedSignature OnGrappleLanded;

	UPROPERTY(BlueprintAssignable, Category = "Grapple")
	FOnGrappleCancelledSignature OnGrappleCancelled;

	/** Farthest an anchor can be from the character, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple", meta = (ClampMin = "0.0"))
	float Range = 2500.f;

	/**
	 * Anchors closer than this are never targeted: the one just landed beside would otherwise keep
	 * the marker lit and a press would zip less than a metre. cm.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple", meta = (ClampMin = "0.0"))
	float MinRange = 300.f;

	/** Largest angle between the camera forward and the anchor, degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple", meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float ConeDegrees = 30.f;

	/** Zip speed along the line, cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple", meta = (ClampMin = "1.0"))
	float ZipSpeed = 1800.f;

	/** How far along a zip a second arrow may be fired to chain, 0..1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ChainMinProgress = 0.4f;

	/**
	 * Speed of an arrow fired mid-zip (a chain), cm/s; 0 means it arrives the moment it leaves the
	 * bow. At the 6000 cm/s of a standing shot a short line ended before the arrow got there.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple", meta = (ClampMin = "0.0"))
	float ChainArrowSpeed = 0.f;

	/** A chain turns the direction of travel from the old line onto the new one over this long, s. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple", meta = (ClampMin = "0.0"))
	float RedirectBlendSeconds = 0.1f;

	/** Seconds between target refreshes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple", meta = (ClampMin = "0.01"))
	float RefreshSeconds = 0.1f;

	/** A line-of-sight hit this close to the anchor still counts as seeing it (the parapet it sits on). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple", meta = (ClampMin = "0.0"))
	float SightTolerance = 60.f;

	/**
	 * Static geometry within this of the anchor counts as the anchor's building and does not
	 * block the zip: the line clips the parapet it sits on, and at a corner the neighbouring
	 * tenement's parapet too. cm.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple", meta = (ClampMin = "0.0"))
	float SupportRadius = 150.f;

	/**
	 * From the ground, the straight line starts this far above the capsule centre she stood at:
	 * her feet 120 cm up clear a 90 cm parapet, so level and downward lines work. cm.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple", meta = (ClampMin = "0.0"))
	float ZipLaunchHeight = 120.f;

	/** The hop up to the launch point before the straight line, s. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple", meta = (ClampMin = "0.0"))
	float ZipHopSeconds = 0.15f;

	/**
	 * Static geometry within this of where she stood (her own roof, parapet and fire escape) does
	 * not block the zip; it counts again once the capsule is this far plus its radius away. cm.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple", meta = (ClampMin = "0.0"))
	float ZipStartIgnoreRadius = 250.f;

	/** Within this of an anchor, its stuck arrows go back in the quiver, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple", meta = (ClampMin = "0.0"))
	float RecoverRadius = 200.f;

	/** Side of a spatial grid cell, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple", meta = (ClampMin = "100.0"))
	float GridCellSize = 2500.f;

	/** Where the arrow leaves from when the owner has no bow component, relative to the actor origin. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple")
	FVector ArrowLaunchOffset = FVector(40.f, 0.f, 50.f);

	/** What TryFire spawns when the grapple arrow definition names no grapple projectile class. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple")
	TSubclassOf<AGrappleArrowProjectile> ArrowClass;

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

	/**
	 * The building the anchor stands on, found under its landing point, plus any static actor
	 * within SupportRadius of the anchor. The zip sweep ignores all of them.
	 */
	void FindAnchorSupports(const AGrappleAnchor* Anchor, TArray<AActor*>& OutSupports) const;

	/** The static actors within ZipStartIgnoreRadius of Start and the one under it. */
	void FindStartSupports(const FVector& Start, TArray<AActor*>& OutSupports) const;

	/** Starts or stops the capsule ignoring the current supports. */
	void SetSupportsIgnored(bool bIgnore);

	/** Stops ignoring the start supports that are not also anchor supports. */
	void ReleaseStartSupports();

	/** How far from the start the capsule centre must be before the start supports count again. */
	float GetStartReleaseDistance() const;

	ACharacter* GetCharacter() const;

	UInventoryComponent* GetInventory() const;

	/** The quiver slot holding grapple arrows, or INDEX_NONE. */
	int32 FindGrappleSlot() const;

	/** Spawns the grapple arrow at the bow hand (or ArrowLaunchOffset) aimed at Anchor. */
	AGrappleArrowProjectile* SpawnGrappleArrow(UArrowDefinition* Definition, const AGrappleAnchor* Anchor) const;

	TWeakObjectPtr<AGrappleAnchor> TargetAnchor;
	TWeakObjectPtr<AGrappleAnchor> ZipAnchor;
	TArray<TWeakObjectPtr<AActor>> ZipIgnoredSupports;
	TArray<TWeakObjectPtr<AActor>> ZipStartSupports;
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
	FVector ZipLaunch = FVector::ZeroVector;
	FVector ZipEnd = FVector::ZeroVector;
	bool bHopping = false;
	bool bStartSupportsIgnored = false;
	bool bRedirecting = false;
	float RedirectElapsed = 0.f;
	FVector RedirectFromDirection = FVector::ZeroVector;
	FVector ZipDirection = FVector::ZeroVector;
	int32 RedirectCount = 0;
	float HopElapsed = 0.f;
	float ZipLength = 0.f;
	float ZipTravelled = 0.f;
	float PreZipGravityScale = 1.f;
	float RefreshAccumulator = 0.f;
	int32 UseCount = 0;
};

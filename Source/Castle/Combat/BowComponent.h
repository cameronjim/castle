// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Math/RandomStream.h"
#include "BowComponent.generated.h"

class AArrowProjectile;
class UArrowDefinition;
class UBowDefinition;
class UInventoryComponent;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDrawChangedSignature, float, Fraction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnArrowFiredSignature, UArrowDefinition*, Arrow);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnArrowHitSignature, AActor*, HitActor, float, Damage, bool, bHeadshot);

/**
 * The player's bow (claude-docs/gameplay-semantics.md, "bow and arrows"). Replaces the hitscan
 * weapon for the player; thugs keep UWeaponComponent.
 *
 * StartDraw on the fire press, ReleaseDraw on its release. The draw fraction is the hold time over
 * the bow's FullDrawSeconds; a release below MinDrawFraction cancels and spends nothing. Otherwise
 * one arrow of the active quiver slot is spent and its projectile leaves the bow hand toward the
 * point under the reticle (a camera trace finds it), at a speed, spread and damage set by the draw,
 * with PerfectBonus for a release inside the perfect window. The grapple arrow is not a projectile
 * here: releasing with it nocked hands over to UGrappleComponent::TryFire.
 *
 * Also owns the bow's look: the mesh on the back while holstered, attached to the left hand while
 * drawing, turned toward the aim as the draw builds and the string pulled back. No draw animation
 * exists and the arm cannot be posed without an AnimBP change, so the grip also blends from the
 * hand up to a point held out in front of the left shoulder, where a drawn bow would be. An arrow
 * sits on the string from the nock forward through the grip while drawing.
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = (Castle), meta = (BlueprintSpawnableComponent))
class CASTLE_API UBowComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBowComponent();

	/** Starts drawing. False with no bow, while already drawing, or with a dead owner. */
	UFUNCTION(BlueprintCallable, Category = "Bow")
	bool StartDraw();

	/** Releases: fires if drawn past MinDrawFraction, cancels otherwise. True when an arrow (or grapple) went out. */
	UFUNCTION(BlueprintCallable, Category = "Bow")
	bool ReleaseDraw();

	/** Lets the string down without firing (sprint, a zip, a takedown). */
	UFUNCTION(BlueprintCallable, Category = "Bow")
	void CancelDraw();

	UFUNCTION(BlueprintPure, Category = "Bow")
	bool IsDrawing() const { return bDrawing; }

	/** 0..1, how far the current draw has come. 0 while not drawing. */
	UFUNCTION(BlueprintPure, Category = "Bow")
	float GetDrawFraction() const;

	/** Seconds the current draw has been held. 0 while not drawing. */
	UFUNCTION(BlueprintPure, Category = "Bow")
	float GetDrawElapsed() const;

	/** Cone half-angle a release now would get, degrees. The bow's MaxSpread while not drawing. */
	UFUNCTION(BlueprintPure, Category = "Bow")
	float GetCurrentSpreadDegrees() const;

	/** True while a release now would be perfect. The HUD flashes the draw bar on it. */
	UFUNCTION(BlueprintPure, Category = "Bow")
	bool IsInPerfectWindow() const;

	/** The owner's bow, or null. */
	UFUNCTION(BlueprintPure, Category = "Bow")
	UBowDefinition* GetBow() const;

	/** Where arrows leave from: the bow hand socket, or the owner's origin plus ArrowLaunchOffset. */
	UFUNCTION(BlueprintPure, Category = "Bow")
	FVector GetArrowSpawnLocation() const;

	/** The world point under the reticle: a camera trace on the Weapon channel, or AimTraceDistance ahead. */
	UFUNCTION(BlueprintPure, Category = "Bow")
	FVector ComputeAimPoint() const;

	/** Spawns Arrow's projectile at the bow hand. The grapple uses this for its own flight. */
	AArrowProjectile* SpawnArrowProjectile(UArrowDefinition* Arrow, TSubclassOf<AArrowProjectile> FallbackClass,
		const FVector& Direction) const;

	/** Called by an arrow that hit something with health. Broadcasts OnHit. */
	void NotifyArrowHit(AActor* HitActor, float Damage, bool bHeadshot);

	/** Overrides the clock, for automation tests whose components have no ticking world. */
	void SetTestTimeSeconds(double InSeconds);

	/** Replaces the spread stream so a test gets the same cone every run. */
	void SetTestRandomStream(const FRandomStream& InStream) { SpreadStream = InStream; }

	UFUNCTION(BlueprintPure, Category = "Bow|Visual")
	UStaticMeshComponent* GetBowMeshComponent() const { return BowMesh; }

	/** True while the bow is in the hand (drawing, or just loosed), false on the back. */
	UFUNCTION(BlueprintPure, Category = "Bow|Visual")
	bool IsBowInHand() const;

	/** Every tick while drawing, and 0 on release or cancel. */
	UPROPERTY(BlueprintAssignable, Category = "Bow")
	FOnDrawChangedSignature OnDrawChanged;

	/** After an arrow leaves the bow (or the grapple is handed its shot). */
	UPROPERTY(BlueprintAssignable, Category = "Bow")
	FOnArrowFiredSignature OnArrowFired;

	/** When one of this bow's arrows lands on something with health, after the damage. */
	UPROPERTY(BlueprintAssignable, Category = "Bow")
	FOnArrowHitSignature OnHit;

	/** Used when the arrow definition names no projectile class. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow")
	TSubclassOf<AArrowProjectile> DefaultProjectileClass;

	/** Where arrows leave from when the mesh has no hand socket, relative to the actor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow")
	FVector ArrowLaunchOffset = FVector(40.f, -10.f, 50.f);

	/** How far the reticle trace looks, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow", meta = (ClampMin = "100.0"))
	float AimTraceDistance = 20000.f;

	/** Walk speed multiplier while drawing (the aimed walk is slowed again). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DrawWalkSpeedMultiplier = 0.6f;

	// --- Visual tuning ----------------------------------------------------------------------------

	/** Bone the bow hangs off while holstered. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow|Visual")
	FName HolsterBone = FName(TEXT("spine_03"));

	/** Holstered position relative to HolsterBone, in the actor's frame (X forward, Y right, Z up), cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow|Visual")
	FVector HolsterOffset = FVector(-16.f, 0.f, -6.f);

	/** Holstered tilt across the back, degrees about the actor's forward axis. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow|Visual")
	float HolsterRollDegrees = 35.f;

	/** Pitch of the bow in the hand at the start of the draw, before it comes up to the aim, degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow|Visual")
	float LoweredPitchDegrees = -50.f;

	/** Cant of the drawn bow, degrees of roll; negative tips the top limb out to the left, clear of her head. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow|Visual")
	float DrawnCantDegrees = -12.f;

	/** Grip offset from the hand socket in the aim frame (X along the aim), cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow|Visual")
	FVector HandGripOffset = FVector(0.f, 0.f, 0.f);

	/** Bone the held-out grip is measured from while drawing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow|Visual")
	FName DrawShoulderBone = FName(TEXT("upperarm_l"));

	/** Where the grip is held at full draw, from DrawShoulderBone in the aim frame (X along the aim), cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow|Visual")
	FVector DrawnGripOffset = FVector(58.f, -10.f, -4.f);

	/** Draw fraction by which the bow has come up from the hand to the held-out grip. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow|Visual", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float RaiseByDrawFraction = 0.4f;

	/** Limb tip position in the bow mesh's frame; the string runs between (X, 0, +Z) and (X, 0, -Z), cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow|Visual")
	FVector StringTip = FVector(-12.f, 0.f, 60.f);

	/** Seconds the bow stays up in the hand after an arrow is loosed, before going back on her back. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow|Visual", meta = (ClampMin = "0.0"))
	float FollowThroughSeconds = 0.6f;

	/** How far the string's nock point comes back at full draw, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow|Visual", meta = (ClampMin = "0.0"))
	float StringPullAtFullDraw = 50.f;

	/** String thickness, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow|Visual", meta = (ClampMin = "0.05"))
	float StringThickness = 0.5f;

	/** String colour, through BasicShapeMaterial's Color parameter. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow|Visual")
	FLinearColor StringColor = FLinearColor(0.85f, 0.85f, 0.8f);

	/** Length of the arrow shown on the string while drawing, from the nock forward past the grip, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow|Visual", meta = (ClampMin = "1.0"))
	float NockedArrowLength = 80.f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Test clock or world clock. */
	double GetNowSeconds() const;

	UInventoryComponent* FindInventory() const;

	/** Spends one arrow of the active slot and launches it. False when the slot is at zero. */
	bool FireArrow(float Elapsed);

	/** The grapple arrow's release: the grapple component takes the shot. */
	bool FireGrapple(UArrowDefinition* Arrow);

	/** Ends the draw; the owner drops the forced aim. */
	void EndDraw();

	/** Creates or removes the bow and string components to match whether a bow is owned. */
	void RefreshBowVisual();

	/** Places the bow on the back or in the hand and pulls the string for this frame. */
	void UpdateBowVisual();

	/** Lays one string half from Tip to Nock (world space). */
	void PlaceString(UStaticMeshComponent* String, const FVector& Tip, const FVector& Nock) const;

	UFUNCTION()
	void HandleInventoryChanged();

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> BowMesh = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> StringUpper = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> StringLower = nullptr;

	/** The arrow on the string while drawing: a pale shaft and Kate's purple nock. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> NockedShaft = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> NockedNock = nullptr;

	/** The bow the visual was built for, so a new bow rebuilds it. */
	UPROPERTY(Transient)
	TObjectPtr<UBowDefinition> VisualBow = nullptr;

	UPROPERTY(Transient)
	FRandomStream SpreadStream;

	bool bDrawing = false;
	double DrawStartSeconds = 0.0;

	/** The bow stays in the hand until this time after a shot (the follow-through). */
	double FollowThroughUntilSeconds = -1.0;

	bool bUseTestTime = false;
	double TestTimeOverride = 0.0;
};

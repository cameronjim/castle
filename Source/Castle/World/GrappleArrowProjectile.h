// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GrappleArrowProjectile.generated.h"

class AGrappleAnchor;
class UGrappleComponent;
class UStaticMeshComponent;

/**
 * The grapple arrow in flight: a straight line from the bow to the anchor at Speed, no gravity
 * (unlike a standard arrow, which will arc). On arrival it sticks in the anchor and tells the
 * grapple component, which starts the zip. It stays in the anchor until recovered.
 *
 * TODO(stage2): a placeholder shaft until the bow step brings BP_Arrow_Grapple and a real mesh.
 */
UCLASS(Blueprintable, BlueprintType)
class CASTLE_API AGrappleArrowProjectile : public AActor
{
	GENERATED_BODY()

public:
	AGrappleArrowProjectile();

	/** cm/s. Fast enough that the zip starts a fraction of a second after the release. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grapple", meta = (ClampMin = "1.0"))
	float Speed = 6000.f;

	/** Sends the arrow at Anchor's marker. Grapple is told when it arrives; either may be null. */
	void Launch(AGrappleAnchor* Anchor, UGrappleComponent* Grapple);

	/** Moves the arrow DeltaSeconds along its line. Tick calls this; so does a test with no ticking. */
	void Advance(float DeltaSeconds);

	UFUNCTION(BlueprintPure, Category = "Grapple")
	bool HasArrived() const { return bArrived; }

	UFUNCTION(BlueprintPure, Category = "Grapple")
	AGrappleAnchor* GetTargetAnchor() const { return TargetAnchor.Get(); }

protected:
	virtual void Tick(float DeltaSeconds) override;

	/** Sticks in the anchor and hands over to the grapple component. */
	void Arrive();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grapple")
	TObjectPtr<UStaticMeshComponent> Shaft;

	TWeakObjectPtr<AGrappleAnchor> TargetAnchor;

	TWeakObjectPtr<UGrappleComponent> Grapple;

	bool bArrived = false;
};

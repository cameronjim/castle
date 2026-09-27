// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Combat/BowHandIKTypes.h"
#include "BowIKAnimInstance.generated.h"

class UBowComponent;

/**
 * Parent class of the bow hands post-process AnimBlueprints (ABP_BowIK_Post for the UEFN
 * mannequin, ABP_BowIK_Post_Thug for the old one), which UHawkeyeBowIKGraphBuilder builds headless.
 * UBowComponent sets the Blueprint as its mesh's post-process AnimBP override, so it runs after the
 * main AnimBP (the sample's motion matching, or a thug's single clip) and layers onto that pose:
 * the spine turns side-on toward the aim, the neck turns back, then two-bone IK puts hand_l on the
 * grip and hand_r on the string.
 *
 * This class only computes the graph's inputs, every animation update, from the owner's
 * UBowComponent: the targets in component space from the head bone and the aim, and the two alphas
 * blended over the settings' BlendSeconds. It has no graph of its own; used directly it would pass
 * no pose through, so it is never set as an override by itself.
 */
UCLASS(Transient, Blueprintable)
class HAWKEYE_API UHawkeyeBowIKAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/**
	 * Hand and elbow targets in AimFrame's space for DrawFraction (clamped to 0..1): the bow hand on
	 * the grip, the string hand moving linearly from the rest offset to the full-draw offset, the
	 * draw elbow's hint moving with it. AimFrame's X is the aim; its scale is ignored.
	 */
	static FBowHandIKTargets ComputeHandTargets(const FTransform& AimFrame, float DrawFraction,
		const FBowHandIKSettings& Settings);

	/** Current moved toward Target at a rate that covers 0 to 1 in BlendSeconds. Snaps when BlendSeconds <= 0. */
	static float StepAlpha(float Current, float Target, float DeltaSeconds, float BlendSeconds);

	/**
	 * Spine yaw while the bow is up: the aim's yaw off the body's, clamped to MaxAimTwistDegrees,
	 * plus SideOnDegrees. Degrees; the caller scales it by the bow alpha.
	 */
	static float ComputeSpineTwistDegrees(float AimYaw, float BodyYaw, const FBowHandIKSettings& Settings);

	/**
	 * One update from Bow (null is fine: the hands let go). Alphas step toward 1 while the bow is up
	 * (bow hand) or drawing (string hand); the targets are recomputed while either is above 0.
	 * NativeUpdateAnimation calls this with the owner's bow; tests call it directly.
	 */
	void UpdateFromBow(const UBowComponent* Bow, float DeltaSeconds);

	float GetBowAlpha() const { return BowAlpha; }
	float GetDrawAlpha() const { return DrawAlpha; }

	/** hand_l's target, component space. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Bow IK")
	FVector LeftHandTarget = FVector::ZeroVector;

	/** Where the left elbow points, component space. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Bow IK")
	FVector LeftElbowTarget = FVector::ZeroVector;

	/** hand_r's target (the string), component space. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Bow IK")
	FVector RightHandTarget = FVector::ZeroVector;

	/** Where the right elbow points, component space. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Bow IK")
	FVector RightElbowTarget = FVector::ZeroVector;

	/** 0..1, the bow hand on the grip (and the spine turn). */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Bow IK")
	float BowAlpha = 0.f;

	/** 0..1, the string hand on the string. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Bow IK")
	float DrawAlpha = 0.f;

	/** Added to the spine bone, component space. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Bow IK")
	FRotator SpineTwist = FRotator::ZeroRotator;

	/** Added to the neck bone, component space: the side-on turn taken back so the head stays on the aim. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Bow IK")
	FRotator NeckTwist = FRotator::ZeroRotator;

protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	/** Recomputes the component-space targets and the twists from Bow and the last pose's head bone. */
	void UpdateTargets(const UBowComponent& Bow);
};

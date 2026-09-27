// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Combat/BowHandIKTypes.h"
#include "Combat/StrikePose.h"
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
 *
 * The same graph poses strikes and hit reactions, since there are no attack or flinch clips: the owner
 * hands it a strike pose (SetStrikePose), whose hand targets win over the bow's while their alphas are
 * above 0, and a hit lean (SetHitLean), a few degrees on spine_01 away from the hit. The graph reads
 * LeftArmAlpha and RightArmAlpha (the bow's or the strike's) and HitLean.
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

	/**
	 * This frame's strike override: Sample's hands (in ActorFrame, the striker's actor transform) as
	 * component-space targets. A hand whose alpha is 0 leaves the bow's target alone.
	 */
	void SetStrikePose(const FHawkeyeStrikePoseSample& Sample, const FTransform& ActorFrame);

	/** This frame's hit lean: WorldDirection (flat, the way to lean) at Alpha (0..1). */
	void SetHitLean(const FVector& WorldDirection, float Alpha);

	/** Writes the graph's inputs from the bow targets, the strike override and the hit lean. */
	void ComposeOutputs();

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

	/** hand_l's IK alpha as the graph reads it: the bow hand's, or a strike's while one is on. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Bow IK")
	float LeftArmAlpha = 0.f;

	/** hand_r's IK alpha as the graph reads it: the string hand's, or a strike's while one is on. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Bow IK")
	float RightArmAlpha = 0.f;

	/** Added to spine_01, component space: the lean away from a hit. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Bow IK")
	FRotator HitLean = FRotator::ZeroRotator;

	/** The way the upper body leans, component space, flat. Zero for none. */
	UPROPERTY(Transient, BlueprintReadWrite, Category = "Bow IK|Hit Lean")
	FVector HitLeanDirection = FVector::ZeroVector;

	/** 0..1, how far into the lean. */
	UPROPERTY(Transient, BlueprintReadWrite, Category = "Bow IK|Hit Lean")
	float HitLeanAlpha = 0.f;

	/** The lean at full alpha, degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bow IK|Hit Lean")
	float HitLeanDegrees = 5.f;

	/** A strike's right hand target, component space; wins over the string hand while its alpha is above 0. */
	UPROPERTY(Transient, BlueprintReadWrite, Category = "Bow IK|Strike")
	FVector StrikeRightHandTarget = FVector::ZeroVector;

	UPROPERTY(Transient, BlueprintReadWrite, Category = "Bow IK|Strike")
	FVector StrikeRightElbowTarget = FVector::ZeroVector;

	UPROPERTY(Transient, BlueprintReadWrite, Category = "Bow IK|Strike")
	float StrikeRightHandAlpha = 0.f;

	/** A strike's left hand target, component space; wins over the bow hand while its alpha is above 0. */
	UPROPERTY(Transient, BlueprintReadWrite, Category = "Bow IK|Strike")
	FVector StrikeLeftHandTarget = FVector::ZeroVector;

	UPROPERTY(Transient, BlueprintReadWrite, Category = "Bow IK|Strike")
	FVector StrikeLeftElbowTarget = FVector::ZeroVector;

	UPROPERTY(Transient, BlueprintReadWrite, Category = "Bow IK|Strike")
	float StrikeLeftHandAlpha = 0.f;

protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	/** Recomputes the component-space targets and the twists from Bow and the last pose's head bone. */
	void UpdateTargets(const UBowComponent& Bow);
};

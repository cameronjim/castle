// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/PoseableMeshComponent.h"
#include "FirstPersonArmsComponent.generated.h"

/** Which hand pose the first-person arms hold. */
UENUM(BlueprintType)
enum class ECastleArmsPose : uint8
{
	/** Empty handed: both fists up in front of the chest. */
	Fists,
	/** Pistol grip: right arm out towards screen centre, left hand supporting under it. */
	Pistol
};

/**
 * One bone of a hand-authored pose: point this bone at Direction.
 *
 * A limb is easier to author as "where does it point" than as an Euler triple, because the
 * mannequin's reference pose is an A-pose with a different rotation on every bone. Direction
 * is in component space, where +Y is forward (down the camera), +X is the character's left
 * and +Z is up. ChildBone is the joint at the far end, which is what says where the bone
 * currently points.
 */
USTRUCT(BlueprintType)
struct FCastleArmBonePose
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arms")
	FName BoneName;

	/** The next joint down the limb. The bone "points" from itself to this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arms")
	FName ChildBone;

	/** Where that limb should point, in component space. Normalised on use. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arms")
	FVector Direction = FVector(0.f, 1.f, 0.f);

	/** Roll about the limb's own axis, for turning a palm or a wrist. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arms")
	float TwistDegrees = 0.f;
};

/**
 * One finger joint and how far down its chain it sits.
 *
 * Fingers are not authored as direction targets: a closed hand is fifteen joints per side all
 * doing the same thing, so they are curled procedurally about one axis instead. Depth is 1 at
 * the knuckle, 3 at the tip, and the curl angle is multiplied by it so the chain closes evenly.
 */
struct FCastleFingerJoint
{
	FName BoneName;

	/** 1 at the knuckle joint, 2 and 3 further down the finger. */
	int32 Depth = 1;

	/** The thumb opposes rather than flexes, so it takes a fraction of the curl. */
	bool bThumb = false;

	bool bRightHand = true;
};

/**
 * The arms Frank sees in front of him. A poseable mesh rather than a skeletal one: UE 5.8
 * ships no arms-only asset and no arms AnimBP, and a poseable mesh is the only component that
 * takes per-bone rotations without one. It holds two hand-authored poses and blends between
 * them; it never plays an animation, so the body mesh (which does) stays the thing that
 * animates, casts shadows and shows up in a mirror.
 */
UCLASS(ClassGroup = (Castle), meta = (BlueprintSpawnableComponent))
class CASTLE_API UFirstPersonArmsComponent : public UPoseableMeshComponent
{
	GENERATED_BODY()

public:
	UFirstPersonArmsComponent();

	/** Hides the non-arm bones and caches the reference pose. Safe to call more than once. */
	UFUNCTION(BlueprintCallable, Category = "Castle|Arms")
	void InitialiseArms();

	/** Blends to NewPose over PoseBlendSeconds, or snaps to it when bImmediate. */
	UFUNCTION(BlueprintCallable, Category = "Castle|Arms")
	void SetPose(ECastleArmsPose NewPose, bool bImmediate = false);

	UFUNCTION(BlueprintPure, Category = "Castle|Arms")
	ECastleArmsPose GetActivePose() const { return ActivePose; }

	/** 0 while the blend still reads as the previous pose, 1 once it has arrived. */
	UFUNCTION(BlueprintPure, Category = "Castle|Arms")
	float GetPoseBlendAlpha() const { return BlendAlpha; }

	/** The authored table for a pose. Exposed so a test can check its bone names. */
	const TArray<FCastleArmBonePose>& GetPoseTable(ECastleArmsPose Pose) const;

	/** Every bone either pose touches, plus the hidden and curled ones. Used by the bone-name test. */
	TArray<FName> GetAllPoseBoneNames() const;

	/** The thirty finger joints the curl pass writes to, five fingers by three joints by two hands. */
	UFUNCTION(BlueprintPure, Category = "Castle|Arms")
	TArray<FName> GetFingerBoneNames() const;

	/**
	 * Throws a 0.25 s jab with the fist that did not throw the last one, starting with the right.
	 * Purely cosmetic: melee damage is the caller's business.
	 */
	UFUNCTION(BlueprintCallable, Category = "Castle|Arms")
	void PlayPunch();

	/** True while a jab is still travelling out or coming back. */
	UFUNCTION(BlueprintPure, Category = "Castle|Arms")
	bool IsPunching() const { return PunchElapsed >= 0.f; }

	/** Which fist the current (or most recent) jab used. The first call ever is the right. */
	UFUNCTION(BlueprintPure, Category = "Castle|Arms")
	bool IsPunchingRightHand() const { return bPunchRightHand; }

	/** 0 at rest, 1 at full extension. A half sine, so the jab lands and returns in one curve. */
	UFUNCTION(BlueprintPure, Category = "Castle|Arms")
	float GetPunchAlpha() const;

	UFUNCTION(BlueprintPure, Category = "Castle|Arms")
	TArray<FName> GetHiddenArmBones() const { return HiddenArmBones; }

	//~ Begin UActorComponent interface
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	//~ End UActorComponent interface

protected:
	/** Writes the current blend of the two pose tables onto the bones. */
	void ApplyPose();

	/** The delta rotation Pose asks of BoneName, or identity when it does not mention it. */
	FQuat FindPoseDelta(ECastleArmsPose Pose, FName BoneName) const;

	/** Closes the fingers of both hands by the current blend of the two poses' curl angles. */
	void ApplyFingerCurl();

	/** Slides the arm roots forward while a jab is out, and back to the reference otherwise. */
	void ApplyPunchOffset();

	/** Degrees one finger joint closes in Pose. Right and left differ once a pistol is in them. */
	float GetCurlDegrees(ECastleArmsPose Pose, bool bRightHand) const;

	/**
	 * The component-space axis the fingers of one hand flex about, measured from the reference
	 * pose: across the knuckles, signed so that a positive rotation closes the hand.
	 */
	FVector ComputeCurlAxis(bool bRightHand);

	/**
	 * Empty-handed pose: forearms up, fists at chest height, elbows bent about 100 degrees,
	 * hands turned slightly inward so the knuckles face the camera.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Castle|Arms|Pose")
	TArray<FCastleArmBonePose> PoseFists;

	/**
	 * Pistol pose: right arm out towards screen centre with the elbow about 150 degrees, left
	 * hand brought across to support it under the grip.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Castle|Arms|Pose")
	TArray<FCastleArmBonePose> PosePistol;

	/**
	 * Bones hidden so the full-body mannequin reads as a pair of arms. The spine is not in the
	 * list on purpose: the clavicles descend from it, so hiding it takes the arms with it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Castle|Arms")
	TArray<FName> HiddenArmBones;

	/** Seconds a switch between poses takes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Castle|Arms", meta = (ClampMin = "0.0"))
	float PoseBlendSeconds = 0.2f;

	// --- finger curl --------------------------------------------------------------------------

	/** Degrees each of the three joints closes in the fists pose. Three times this is a fist. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Castle|Arms|Fingers", meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float FistsCurlDegrees = 70.f;

	/** The trigger hand: closed round the grip, not balled up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Castle|Arms|Fingers", meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float PistolRightCurlDegrees = 50.f;

	/** The support hand wraps the shooting hand, so it closes a little less. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Castle|Arms|Fingers", meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float PistolLeftCurlDegrees = 40.f;

	/** The thumb lies across the fingers rather than folding into the palm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Castle|Arms|Fingers", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ThumbCurlScale = 0.45f;

	/**
	 * Flip to -1 if the fingers ever open backwards. The flex axis is measured from the skeleton
	 * (see ComputeCurlAxis), and the one thing it assumes is that the thumb sits on the palm side.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Castle|Arms|Fingers")
	float FingerCurlSign = 1.f;

	// --- punch --------------------------------------------------------------------------------

	/** How far the jabbing arm travels down the camera's forward axis, in centimetres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Castle|Arms|Punch", meta = (ClampMin = "0.0"))
	float PunchDistance = 20.f;

	/** Seconds the whole jab takes, out and back. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Castle|Arms|Punch", meta = (ClampMin = "0.01"))
	float PunchSeconds = 0.25f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Castle|Arms")
	ECastleArmsPose ActivePose = ECastleArmsPose::Fists;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Castle|Arms")
	ECastleArmsPose PreviousPose = ECastleArmsPose::Fists;

private:
	/** Reference-pose component-space rotation of every posed bone, read once at init. */
	TMap<FName, FQuat> ReferenceRotations;

	/** Which way each posed bone points in the reference pose, from bone to child. */
	TMap<FName, FVector> ReferenceDirections;

	/** Union of both tables' bone names, so a bone dropped from one pose still blends back. */
	TArray<FName> PosedBones;

	/** Every finger joint of both hands, built once in the constructor. */
	TArray<FCastleFingerJoint> FingerJoints;

	/** Reference-pose component-space location of each arm root, so the jab can slide it. */
	TMap<FName, FVector> ReferenceLocations;

	/** Component-space flex axis per hand, measured from the reference pose at init. */
	FVector CurlAxisRight = FVector::ZeroVector;
	FVector CurlAxisLeft = FVector::ZeroVector;

	float BlendAlpha = 1.f;

	/** Seconds into the current jab, or negative when no fist is out. */
	float PunchElapsed = -1.f;

	/** Which fist threw the last jab. Starts false so the first PlayPunch() is a right. */
	bool bPunchRightHand = false;

	bool bArmsInitialised = false;
};

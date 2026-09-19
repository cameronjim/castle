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

	/** Every bone either pose touches, plus the hidden ones. Used by the bone-name test. */
	TArray<FName> GetAllPoseBoneNames() const;

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

	float BlendAlpha = 1.f;

	bool bArmsInitialised = false;
};

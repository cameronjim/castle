// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CombatMontageBuilder.generated.h"

class UAnimMontage;
class UAnimNotifyState;
class UAnimSequence;
class UAnimSequenceBase;

/**
 * How one combat montage is laid out. Windows are fractions of the clip's length (0..1); a start
 * below 0 leaves that window out. Tools/Data/Anims/manifest.json gives these per clip, with per-role
 * defaults in Tools/Editor/import_combat_anims.py.
 */
USTRUCT(BlueprintType)
struct HAWKEYE_API FHawkeyeCombatMontageSpec
{
	GENERATED_BODY()

	/** DefaultSlot (full body) or UpperBody (the bow clips, layered over locomotion). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Montage")
	FName SlotName = FName(TEXT("DefaultSlot"));

	/** ANS_HitWindow: when the swing sweeps for its target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Montage")
	float HitStart = -1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Montage")
	float HitEnd = -1.f;

	/** ANS_ComboWindow: when the next strike may cut this one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Montage")
	float ComboStart = -1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Montage")
	float ComboEnd = -1.f;

	/** A motion warping window from the start to here (skew warp to WarpTargetName). Below 0: none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Montage")
	float WarpEnd = -1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Montage")
	FName WarpTargetName = FName(TEXT("CombatTarget"));

	/** The one section loops (BowAimIdle). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Montage")
	bool bLoop = false;

	/** No auto blend-out: the last frame holds until the montage is stopped (Knockdown). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Montage")
	bool bHoldLastFrame = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Montage", meta = (ClampMin = "0.0"))
	float BlendInSeconds = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Montage", meta = (ClampMin = "0.0"))
	float BlendOutSeconds = 0.2f;
};

/**
 * Builds the AM_<Role>_<Variant> combat montages headless from retargeted sequences
 * (Tools/Editor/import_combat_anims.py calls it), through the engine's own montage factory and
 * UAnimationBlueprintLibrary's notify calls. Editor builds only; in a cooked game it logs an error
 * and returns null.
 */
UCLASS()
class HAWKEYE_API UHawkeyeCombatMontageBuilder : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Creates (or relays out in place) the montage at PackageName (e.g.
	 * "/Game/Characters/UEFN_Mannequin/Animations/Combat/AM_Light1_Jab") over Sequence, with Spec's
	 * slot, notify windows, warp window, loop and blends. The slot is registered on the skeleton. An
	 * existing montage over a different sequence is refused (null); delete it first. Returns the
	 * montage dirty and unsaved; the caller saves it and the skeleton.
	 */
	UFUNCTION(BlueprintCallable, Category = "Hawkeye|Animation")
	static UAnimMontage* BuildCombatMontage(UAnimSequence* Sequence, const FString& PackageName,
		const FHawkeyeCombatMontageSpec& Spec);

	/**
	 * "slot=DefaultSlot hit=0.18-0.30 combo=0.35-0.60 warp=0.00-0.18 loop=0 hold=0 length=1.20" for the
	 * verify scripts and the import log. Times in seconds; a missing window reads "-".
	 */
	UFUNCTION(BlueprintCallable, Category = "Hawkeye|Animation")
	static FString DescribeMontage(UAnimMontage* Montage);

	/**
	 * Where Bone is at Time in Sequence's raw data, component space (cm): each bone's own track, or
	 * its reference pose where it has none, walked up the skeleton's hierarchy by name. The import
	 * self-test reads poses this way because it does not depend on how the evaluated pose indexes bones.
	 */
	UFUNCTION(BlueprintCallable, Category = "Hawkeye|Animation")
	static FVector GetRawBoneLocation(UAnimSequence* Sequence, FName Bone, float Time);

	/** Bone names of Sequence's skeleton, in hierarchy order. */
	UFUNCTION(BlueprintCallable, Category = "Hawkeye|Animation")
	static TArray<FName> GetSkeletonBoneNames(UAnimSequence* Sequence);

	/** Animation's first window of NotifyClass: start and end in seconds. False when it has none. */
	UFUNCTION(BlueprintCallable, Category = "Hawkeye|Animation")
	static bool GetNotifyWindow(UAnimSequenceBase* Animation, TSubclassOf<UAnimNotifyState> NotifyClass, float& Start,
		float& End);
};

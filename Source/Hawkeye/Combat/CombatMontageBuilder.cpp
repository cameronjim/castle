// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/CombatMontageBuilder.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Combat/AnimNotifyState_ComboWindow.h"
#include "Combat/AnimNotifyState_HitWindow.h"
#include "Combat/CombatAnimPlayback.h"
#include "Hawkeye.h"

#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/Skeleton.h"

#if WITH_EDITOR
#include "AnimNotifyState_MotionWarping.h"
#include "AnimationBlueprintLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Factories/AnimMontageFactory.h"
#include "Misc/FeedbackContext.h"
#include "Misc/PackageName.h"
#include "RootMotionModifier_SkewWarp.h"
#include "UObject/Package.h"

namespace HawkeyeCombatMontage
{
	static const FName NotifyTrack(TEXT("Hawkeye"));

	static UAnimMontage* LoadOrCreate(UAnimSequence& Sequence, const FString& PackageName)
	{
		const FString AssetName = FPackageName::GetShortName(PackageName);
		const FString ObjectPath = PackageName + TEXT(".") + AssetName;
		if (UAnimMontage* Existing = LoadObject<UAnimMontage>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			return Existing;
		}
		UAnimMontageFactory* Factory = NewObject<UAnimMontageFactory>();
		Factory->SourceAnimation = &Sequence;
		Factory->TargetSkeleton = Sequence.GetSkeleton();
		UPackage* Package = CreatePackage(*PackageName);
		UAnimMontage* Created = Cast<UAnimMontage>(Factory->FactoryCreateNew(UAnimMontage::StaticClass(), Package,
			FName(*AssetName), RF_Public | RF_Standalone | RF_Transactional, nullptr, GWarn));
		if (Created)
		{
			FAssetRegistryModule::AssetCreated(Created);
		}
		return Created;
	}

	static const UAnimSequenceBase* FirstSegmentAnim(const UAnimMontage& Montage)
	{
		if (Montage.SlotAnimTracks.Num() == 0 || Montage.SlotAnimTracks[0].AnimTrack.AnimSegments.Num() == 0)
		{
			return nullptr;
		}
		return Montage.SlotAnimTracks[0].AnimTrack.AnimSegments[0].GetAnimReference();
	}

	/** One window from Start to End (fractions of Length), clamped inside the clip. Null when Start < 0. */
	static UAnimNotifyState* AddWindow(UAnimMontage& Montage, float Start, float End, float Length,
		TSubclassOf<UAnimNotifyState> Class)
	{
		if (Start < 0.f || Length <= 0.f)
		{
			return nullptr;
		}
		const float From = FMath::Clamp(Start, 0.f, 1.f) * Length;
		const float To = FMath::Clamp(FMath::Max(End, Start + 0.01f), 0.f, 1.f) * Length;
		return UAnimationBlueprintLibrary::AddAnimationNotifyStateEvent(&Montage, NotifyTrack, From,
			FMath::Max(To - From, 1.f / 60.f), Class);
	}

	static FString WindowText(UAnimSequenceBase* Animation, const UClass* Class)
	{
		float Start = 0.f;
		float End = 0.f;
		return HawkeyeCombatAnim::FindNotifyWindow(Animation, Class, Start, End)
			? FString::Printf(TEXT("%.2f-%.2f"), Start, End) : FString(TEXT("-"));
	}
}
#endif

UAnimMontage* UHawkeyeCombatMontageBuilder::BuildCombatMontage(UAnimSequence* Sequence, const FString& PackageName,
	const FHawkeyeCombatMontageSpec& Spec)
{
#if WITH_EDITOR
	using namespace HawkeyeCombatMontage;
	USkeleton* Skeleton = Sequence ? Sequence->GetSkeleton() : nullptr;
	if (!Skeleton)
	{
		UE_LOG(LogHawkeye, Error, TEXT("BuildCombatMontage(%s): no sequence, or one without a skeleton."), *PackageName);
		return nullptr;
	}
	UAnimMontage* Montage = LoadOrCreate(*Sequence, PackageName);
	if (!Montage || Montage->SlotAnimTracks.Num() == 0)
	{
		UE_LOG(LogHawkeye, Error, TEXT("BuildCombatMontage(%s): could not create the montage."), *PackageName);
		return nullptr;
	}
	if (FirstSegmentAnim(*Montage) != Sequence)
	{
		UE_LOG(LogHawkeye, Error, TEXT("BuildCombatMontage(%s): exists over %s, not %s; delete it first."), *PackageName,
			*GetNameSafe(FirstSegmentAnim(*Montage)), *Sequence->GetName());
		return nullptr;
	}

	Montage->Modify();
	Montage->SlotAnimTracks[0].SlotName = Spec.SlotName;
	Skeleton->RegisterSlotNode(Spec.SlotName);

	// The builder is the one description of the notifies: whatever was there goes.
	Montage->Notifies.Reset();
	Montage->AnimNotifyTracks.Reset();
	UAnimationBlueprintLibrary::AddAnimationNotifyTrack(Montage, NotifyTrack);
	const float Length = Montage->GetPlayLength();
	AddWindow(*Montage, Spec.HitStart, Spec.HitEnd, Length, UAnimNotifyState_HitWindow::StaticClass());
	AddWindow(*Montage, Spec.ComboStart, Spec.ComboEnd, Length, UAnimNotifyState_ComboWindow::StaticClass());
	if (Spec.WarpEnd > 0.f)
	{
		UAnimNotifyState_MotionWarping* Warp = Cast<UAnimNotifyState_MotionWarping>(
			AddWindow(*Montage, 0.f, Spec.WarpEnd, Length, UAnimNotifyState_MotionWarping::StaticClass()));
		if (Warp)
		{
			URootMotionModifier_SkewWarp* Skew = NewObject<URootMotionModifier_SkewWarp>(Warp, NAME_None, RF_Transactional);
			Skew->WarpTargetName = Spec.WarpTargetName;
			Skew->bWarpTranslation = true;
			Skew->bIgnoreZAxis = true;
			Skew->bWarpRotation = true;
			Warp->RootMotionModifier = Skew;
		}
	}

	Montage->bEnableAutoBlendOut = !Spec.bHoldLastFrame;
	Montage->BlendIn.SetBlendTime(Spec.BlendInSeconds);
	Montage->BlendOut.SetBlendTime(Spec.BlendOutSeconds);
	if (Montage->CompositeSections.Num() > 0)
	{
		FCompositeSection& Section = Montage->CompositeSections[0];
		Section.NextSectionName = Spec.bLoop ? Section.SectionName : NAME_None;
	}
	Montage->RefreshCacheData();
	Montage->PostEditChange();
	Montage->MarkPackageDirty();
	UE_LOG(LogHawkeye, Log, TEXT("BuildCombatMontage: %s over %s: %s"), *PackageName, *Sequence->GetName(),
		*DescribeMontage(Montage));
	return Montage;
#else
	UE_LOG(LogHawkeye, Error, TEXT("BuildCombatMontage(%s): editor builds only."), *PackageName);
	return nullptr;
#endif
}

FString UHawkeyeCombatMontageBuilder::DescribeMontage(UAnimMontage* Montage)
{
	if (!Montage)
	{
		return TEXT("none");
	}
#if WITH_EDITOR
	using namespace HawkeyeCombatMontage;
	const FName Slot = Montage->SlotAnimTracks.Num() > 0 ? Montage->SlotAnimTracks[0].SlotName : NAME_None;
	const bool bLoop = Montage->CompositeSections.Num() > 0
		&& Montage->CompositeSections[0].NextSectionName == Montage->CompositeSections[0].SectionName;
	return FString::Printf(TEXT("slot=%s hit=%s combo=%s warp=%s loop=%d hold=%d rootmotion=%d length=%.2f"), *Slot.ToString(),
		*WindowText(Montage, UAnimNotifyState_HitWindow::StaticClass()),
		*WindowText(Montage, UAnimNotifyState_ComboWindow::StaticClass()),
		*WindowText(Montage, UAnimNotifyState_MotionWarping::StaticClass()), bLoop ? 1 : 0,
		Montage->bEnableAutoBlendOut ? 0 : 1, Montage->HasRootMotion() ? 1 : 0, Montage->GetPlayLength());
#else
	return Montage->GetName();
#endif
}

FVector UHawkeyeCombatMontageBuilder::GetRawBoneLocation(UAnimSequence* Sequence, FName Bone, float Time)
{
#if WITH_EDITOR
	const USkeleton* Skeleton = Sequence ? Sequence->GetSkeleton() : nullptr;
	const IAnimationDataModel* Model = Sequence ? Sequence->GetDataModel() : nullptr;
	if (!Skeleton || !Model)
	{
		return FVector::ZeroVector;
	}
	const FReferenceSkeleton& Reference = Skeleton->GetReferenceSkeleton();
	const FFrameTime Frame = Model->GetFrameRate().AsFrameTime(FMath::Max(Time, 0.f));
	FTransform Accumulated = FTransform::Identity;
	for (int32 Index = Reference.FindBoneIndex(Bone); Index != INDEX_NONE; Index = Reference.GetParentIndex(Index))
	{
		const FName Name = Reference.GetBoneName(Index);
		const FTransform Local = Model->IsValidBoneTrackName(Name)
			? Model->EvaluateBoneTrackTransform(Name, Frame, EAnimInterpolationType::Linear)
			: Reference.GetRefBonePose()[Index];
		Accumulated = Accumulated * Local;
	}
	return Accumulated.GetLocation();
#else
	return FVector::ZeroVector;
#endif
}

TArray<FName> UHawkeyeCombatMontageBuilder::GetSkeletonBoneNames(UAnimSequence* Sequence)
{
	TArray<FName> Names;
	if (const USkeleton* Skeleton = Sequence ? Sequence->GetSkeleton() : nullptr)
	{
		const FReferenceSkeleton& Reference = Skeleton->GetReferenceSkeleton();
		for (int32 Index = 0; Index < Reference.GetNum(); ++Index)
		{
			Names.Add(Reference.GetBoneName(Index));
		}
	}
	return Names;
}

bool UHawkeyeCombatMontageBuilder::GetNotifyWindow(UAnimSequenceBase* Animation, TSubclassOf<UAnimNotifyState> NotifyClass,
	float& Start, float& End)
{
	return HawkeyeCombatAnim::FindNotifyWindow(Animation, NotifyClass.Get(), Start, End);
}

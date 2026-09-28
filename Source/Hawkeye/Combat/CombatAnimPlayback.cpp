// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/CombatAnimPlayback.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Combat/CombatAnimSet.h"
#include "Components/SkeletalMeshComponent.h"

namespace HawkeyeCombatAnim
{
	const FName UpperBodySlot(TEXT("UpperBody"));
	const FName FullBodySlot(TEXT("DefaultSlot"));

	bool IsUpperBodyMontage(const UAnimMontage* Montage)
	{
		return Montage && Montage->SlotAnimTracks.Num() > 0 && Montage->SlotAnimTracks[0].SlotName == UpperBodySlot;
	}

	UAnimInstance* PickInstance(const USkeletalMeshComponent* Mesh, const UAnimMontage* Montage)
	{
		if (!Mesh || !Montage)
		{
			return nullptr;
		}
		UAnimInstance* PostProcess = Mesh->GetPostProcessInstance();
		if (IsUpperBodyMontage(Montage))
		{
			return PostProcess;
		}
		// A single-clip instance has no slot to play into; the post-process graph's DefaultSlot does.
		UAnimInstance* Main = Mesh->GetAnimInstance();
		if (Main && !Main->IsA<UAnimSingleNodeInstance>())
		{
			return Main;
		}
		return PostProcess;
	}

	UAnimInstance* Play(USkeletalMeshComponent* Mesh, UAnimMontage* Montage, float PlayRate)
	{
		UAnimInstance* Instance = PickInstance(Mesh, Montage);
		if (!Instance)
		{
			return nullptr;
		}
		// Montage_Play refuses a montage for another skeleton and returns 0, which is the fallback.
		const float Length = Instance->Montage_Play(Montage, FMath::Max(PlayRate, 0.01f));
		return Length > 0.f ? Instance : nullptr;
	}

	UAnimInstance* PlayRole(USkeletalMeshComponent* Mesh, const UCombatAnimSet* Set, ECombatAnimRole Role, float FitToSeconds)
	{
		UAnimMontage* Montage = UCombatAnimSet::Resolve(Set, Role);
		if (!Montage || !Mesh)
		{
			return nullptr;
		}
		const float Rate = FitToSeconds > 0.f ? FitRate(Montage->GetPlayLength(), FitToSeconds) : 1.f;
		return Play(Mesh, Montage, Rate);
	}

	void Stop(UAnimInstance* Instance, const UAnimMontage* Montage, float BlendOutSeconds)
	{
		if (Instance && Montage && Instance->Montage_IsActive(Montage))
		{
			Instance->Montage_Stop(FMath::Max(BlendOutSeconds, 0.f), Montage);
		}
	}

	bool IsPlaying(const UAnimInstance* Instance, const UAnimMontage* Montage)
	{
		return Instance && Montage && Instance->Montage_IsPlaying(Montage);
	}

	float FitRate(float ClipSeconds, float WantedSeconds)
	{
		if (ClipSeconds <= 0.f || WantedSeconds <= 0.f)
		{
			return 1.f;
		}
		return FMath::Clamp(ClipSeconds / WantedSeconds, 0.25f, 4.f);
	}

	bool FindNotifyWindow(const UAnimSequenceBase* Animation, const UClass* NotifyClass, float& OutStart, float& OutEnd)
	{
		OutStart = 0.f;
		OutEnd = 0.f;
		if (!Animation || !NotifyClass)
		{
			return false;
		}
		for (const FAnimNotifyEvent& Event : Animation->Notifies)
		{
			if (Event.NotifyStateClass && Event.NotifyStateClass->IsA(NotifyClass))
			{
				OutStart = Event.GetTriggerTime();
				OutEnd = Event.GetEndTriggerTime();
				return true;
			}
		}
		return false;
	}

	bool DrivesRootMotion(const USkeletalMeshComponent* Mesh, const UAnimMontage* Montage)
	{
		const UAnimInstance* Main = Mesh ? Mesh->GetAnimInstance() : nullptr;
		return Montage && Montage->HasRootMotion() && Main && PickInstance(Mesh, Montage) == Main
			&& Main->RootMotionMode != ERootMotionMode::IgnoreRootMotion && Main->RootMotionMode != ERootMotionMode::NoRootMotionExtraction;
	}
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/BowIKAnimInstance.h"

#include "Combat/BowComponent.h"
#include "Combat/MeleeRules.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

FBowHandIKTargets UHawkeyeBowIKAnimInstance::ComputeHandTargets(const FTransform& AimFrame, float DrawFraction,
	const FBowHandIKSettings& Settings)
{
	const float Draw = FMath::Clamp(DrawFraction, 0.f, 1.f);
	const FTransform Frame(AimFrame.GetRotation(), AimFrame.GetLocation());

	FBowHandIKTargets Targets;
	Targets.BowHand = Frame.TransformPosition(Settings.GripOffset);
	Targets.BowElbow = Frame.TransformPosition(Settings.BowElbowHint);
	Targets.StringHand = Frame.TransformPosition(FMath::Lerp(Settings.StringHandRestOffset, Settings.StringHandFullOffset, Draw));
	Targets.StringElbow = Frame.TransformPosition(FMath::Lerp(Settings.StringElbowHintRest, Settings.StringElbowHintFull, Draw));
	return Targets;
}

float UHawkeyeBowIKAnimInstance::StepAlpha(float Current, float Target, float DeltaSeconds, float BlendSeconds)
{
	if (BlendSeconds <= 0.f)
	{
		return Target;
	}
	return FMath::FInterpConstantTo(Current, Target, FMath::Max(0.f, DeltaSeconds), 1.f / BlendSeconds);
}

float UHawkeyeBowIKAnimInstance::ComputeSpineTwistDegrees(float AimYaw, float BodyYaw, const FBowHandIKSettings& Settings)
{
	const float TowardAim = FMath::Clamp(FRotator::NormalizeAxis(AimYaw - BodyYaw), -Settings.MaxAimTwistDegrees,
		Settings.MaxAimTwistDegrees);
	return TowardAim + Settings.SideOnDegrees;
}

void UHawkeyeBowIKAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	const AActor* Owner = GetOwningActor();
	UpdateFromBow(Owner ? Owner->FindComponentByClass<UBowComponent>() : nullptr, DeltaSeconds);
}

void UHawkeyeBowIKAnimInstance::UpdateFromBow(const UBowComponent* Bow, float DeltaSeconds)
{
	const bool bHasBow = Bow && Bow->GetBow();
	const float BlendSeconds = Bow ? Bow->HandsIK.BlendSeconds : FBowHandIKSettings().BlendSeconds;
	BowAlpha = StepAlpha(BowAlpha, (bHasBow && Bow->IsBowRaised()) ? 1.f : 0.f, DeltaSeconds, BlendSeconds);
	DrawAlpha = StepAlpha(DrawAlpha, (bHasBow && Bow->IsDrawing()) ? 1.f : 0.f, DeltaSeconds, BlendSeconds);

	if (!bHasBow || (BowAlpha <= 0.f && DrawAlpha <= 0.f))
	{
		SpineTwist = FRotator::ZeroRotator;
		NeckTwist = FRotator::ZeroRotator;
	}
	else
	{
		UpdateTargets(*Bow);
	}
	ComposeOutputs();
}

void UHawkeyeBowIKAnimInstance::ComposeOutputs()
{
	LeftArmAlpha = BowAlpha;
	RightArmAlpha = DrawAlpha;
	// A strike owns a hand outright while it is on: the fist goes where the strike says, not the string.
	if (StrikeRightHandAlpha > 0.f)
	{
		RightHandTarget = StrikeRightHandTarget;
		RightElbowTarget = StrikeRightElbowTarget;
		RightArmAlpha = FMath::Clamp(StrikeRightHandAlpha, 0.f, 1.f);
	}
	if (StrikeLeftHandAlpha > 0.f)
	{
		LeftHandTarget = StrikeLeftHandTarget;
		LeftElbowTarget = StrikeLeftElbowTarget;
		LeftArmAlpha = FMath::Clamp(StrikeLeftHandAlpha, 0.f, 1.f);
	}
	HitLean = UHawkeyeMeleeRules::ComputeHitLeanRotation(HitLeanDirection, HitLeanAlpha, HitLeanDegrees);
}

void UHawkeyeBowIKAnimInstance::SetStrikePose(const FHawkeyeStrikePoseSample& Sample, const FTransform& ActorFrame)
{
	const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	const FTransform Component = Mesh ? Mesh->GetComponentTransform() : FTransform::Identity;
	const FTransform Frame(ActorFrame.GetRotation(), ActorFrame.GetLocation());
	auto ToComponent = [&Component, &Frame](const FVector& Local)
	{
		return Component.InverseTransformPosition(Frame.TransformPosition(Local));
	};
	StrikeRightHandAlpha = Sample.RightAlpha;
	StrikeLeftHandAlpha = Sample.LeftAlpha;
	if (Sample.RightAlpha > 0.f)
	{
		StrikeRightHandTarget = ToComponent(Sample.RightHand);
		StrikeRightElbowTarget = ToComponent(Sample.RightElbow);
	}
	if (Sample.LeftAlpha > 0.f)
	{
		StrikeLeftHandTarget = ToComponent(Sample.LeftHand);
		StrikeLeftElbowTarget = ToComponent(Sample.LeftElbow);
	}
}

void UHawkeyeBowIKAnimInstance::SetHitLean(const FVector& WorldDirection, float Alpha)
{
	const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	HitLeanAlpha = FMath::Clamp(Alpha, 0.f, 1.f);
	HitLeanDirection = Mesh ? Mesh->GetComponentTransform().InverseTransformVectorNoScale(WorldDirection.GetSafeNormal2D())
		: WorldDirection.GetSafeNormal2D();
}

void UHawkeyeBowIKAnimInstance::UpdateTargets(const UBowComponent& Bow)
{
	const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	const AActor* Owner = Bow.GetOwner();
	if (!Mesh || !Owner)
	{
		return;
	}
	const FBowHandIKSettings& Settings = Bow.HandsIK;

	// The head from the last pose: IK never moves it, so the targets do not chase themselves, and the
	// component's current transform keeps them on the body however fast it is moving.
	const FVector Anchor = Mesh->DoesSocketExist(Settings.AnchorBone)
		? Mesh->GetSocketLocation(Settings.AnchorBone) : Mesh->GetComponentLocation();
	const FRotator Aim = Bow.ComputeHandsAimRotation(Anchor);
	const FBowHandIKTargets World = ComputeHandTargets(FTransform(Aim, Anchor), Bow.GetDrawFraction(), Settings);

	const FTransform& Component = Mesh->GetComponentTransform();
	LeftHandTarget = Component.InverseTransformPosition(World.BowHand);
	LeftElbowTarget = Component.InverseTransformPosition(World.BowElbow);
	RightHandTarget = Component.InverseTransformPosition(World.StringHand);
	RightElbowTarget = Component.InverseTransformPosition(World.StringElbow);

	// Component space is only yawed off the actor, so a yaw here is a yaw about world up.
	SpineTwist = FRotator(0.f, ComputeSpineTwistDegrees(Aim.Yaw, Owner->GetActorRotation().Yaw, Settings) * BowAlpha, 0.f);
	NeckTwist = FRotator(0.f, -Settings.SideOnDegrees * BowAlpha, 0.f);
}

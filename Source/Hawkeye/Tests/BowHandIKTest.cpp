// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/BowComponent.h"
#include "Combat/BowDefinition.h"
#include "Combat/BowIKAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Player/HawkeyeCharacter.h"
#include "Player/InventoryComponent.h"
#include "Tests/HawkeyeTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * The bow hands: the post-process AnimBP's inputs come from UHawkeyeBowIKAnimInstance, which puts
 * the bow hand on a grip point in the aim frame, moves the string hand from the string at rest to
 * the cheek as the draw fraction rises, blends each hand on and off over 0.15 s, and turns the
 * spine side-on toward the aim. UBowComponent sets the AnimBP as its mesh's post-process override
 * at BeginPlay. No content is loaded: the anim instance class stands in for the Blueprint.
 */
namespace HawkeyeBowIKTest
{
	static AHawkeyeCharacter* SpawnKate(const FHawkeyeTestWorld& TestWorld, TSubclassOf<UAnimInstance> HandsClass)
	{
		UWorld* World = TestWorld.Get();
		if (!World)
		{
			return nullptr;
		}
		const FTransform Where(FRotator::ZeroRotator, FVector(0.f, 0.f, 100.f));
		AHawkeyeCharacter* Kate = World->SpawnActorDeferred<AHawkeyeCharacter>(AHawkeyeCharacter::StaticClass(), Where,
			nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Kate && Kate->GetBowComponent())
		{
			Kate->GetBowComponent()->HandsIKClass = HandsClass;
		}
		if (Kate)
		{
			Kate->FinishSpawning(Where);
		}
		return Kate;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHawkeyeBowIKStringFollowsDraw, "Hawkeye.BowIK.TargetsFromDrawFraction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHawkeyeBowIKStringFollowsDraw::RunTest(const FString& Parameters)
{
	const FBowHandIKSettings Settings;
	const FTransform Identity = FTransform::Identity;

	const FBowHandIKTargets Rest = UHawkeyeBowIKAnimInstance::ComputeHandTargets(Identity, 0.f, Settings);
	const FBowHandIKTargets Full = UHawkeyeBowIKAnimInstance::ComputeHandTargets(Identity, 1.f, Settings);
	const FBowHandIKTargets Half = UHawkeyeBowIKAnimInstance::ComputeHandTargets(Identity, 0.5f, Settings);
	const FBowHandIKTargets Over = UHawkeyeBowIKAnimInstance::ComputeHandTargets(Identity, 3.f, Settings);

	TestTrue(TEXT("No draw: the string hand is on the string by the bow"), Rest.StringHand.Equals(Settings.StringHandRestOffset, 0.01));
	TestTrue(TEXT("Full draw: the string hand is at the cheek"), Full.StringHand.Equals(Settings.StringHandFullOffset, 0.01));
	TestTrue(TEXT("Half draw: half way along"), Half.StringHand.Equals(
		(Settings.StringHandRestOffset + Settings.StringHandFullOffset) * 0.5, 0.01));
	TestTrue(TEXT("Past full draw clamps to the cheek"), Over.StringHand.Equals(Full.StringHand, 0.01));
	TestTrue(TEXT("The bow hand stays on the grip through the draw"), Rest.BowHand.Equals(Full.BowHand, 0.01)
		&& Rest.BowHand.Equals(Settings.GripOffset, 0.01));
	TestTrue(TEXT("The draw elbow swings from its rest hint to its full hint"),
		Rest.StringElbow.Equals(Settings.StringElbowHintRest, 0.01) && Full.StringElbow.Equals(Settings.StringElbowHintFull, 0.01));
	TestTrue(TEXT("Full draw brings the string hand back behind the grip"), Full.StringHand.X < Full.BowHand.X - 40.0);

	// Turned to face +Y (yaw 90) at (100, 0, 150), scaled: forward offsets land along +Y, scale ignored.
	const FTransform Turned(FRotator(0.f, 90.f, 0.f), FVector(100.f, 0.f, 150.f), FVector(2.f));
	const FBowHandIKTargets TurnedFull = UHawkeyeBowIKAnimInstance::ComputeHandTargets(Turned, 1.f, Settings);
	const FVector Expected = FVector(100.f, 0.f, 150.f) + FRotator(0.f, 90.f, 0.f).RotateVector(Settings.GripOffset);
	TestTrue(TEXT("The grip is placed in the aim frame, unscaled"), TurnedFull.BowHand.Equals(Expected, 0.01));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHawkeyeBowIKAlphaBlend, "Hawkeye.BowIK.AlphasBlendOverPoint15s",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHawkeyeBowIKAlphaBlend::RunTest(const FString& Parameters)
{
	const float Blend = FBowHandIKSettings().BlendSeconds;
	TestEqual(TEXT("The settings blend over 0.15 s"), Blend, 0.15f);

	float Alpha = UHawkeyeBowIKAnimInstance::StepAlpha(0.f, 1.f, 0.075f, Blend);
	TestEqual(TEXT("Half way on after 0.075 s"), Alpha, 0.5f, 0.001f);
	Alpha = UHawkeyeBowIKAnimInstance::StepAlpha(Alpha, 1.f, 0.075f, Blend);
	TestEqual(TEXT("Fully on after 0.15 s"), Alpha, 1.f, 0.001f);
	Alpha = UHawkeyeBowIKAnimInstance::StepAlpha(Alpha, 1.f, 1.f, Blend);
	TestEqual(TEXT("Never past 1"), Alpha, 1.f);
	Alpha = UHawkeyeBowIKAnimInstance::StepAlpha(Alpha, 0.f, 0.05f, Blend);
	TestEqual(TEXT("A third of the way off after 0.05 s"), Alpha, 2.f / 3.f, 0.001f);
	Alpha = UHawkeyeBowIKAnimInstance::StepAlpha(Alpha, 0.f, 0.1f, Blend);
	TestEqual(TEXT("Fully off 0.15 s after letting go"), Alpha, 0.f, 0.001f);
	TestEqual(TEXT("A zero blend snaps"), UHawkeyeBowIKAnimInstance::StepAlpha(0.f, 1.f, 0.001f, 0.f), 1.f);

	FBowHandIKSettings Settings;
	Settings.SideOnDegrees = 15.f;
	Settings.MaxAimTwistDegrees = 45.f;
	TestEqual(TEXT("Facing the aim: only the side-on turn"), UHawkeyeBowIKAnimInstance::ComputeSpineTwistDegrees(30.f, 30.f, Settings), 15.f, 0.01f);
	TestEqual(TEXT("Aim 20 deg right of the body"), UHawkeyeBowIKAnimInstance::ComputeSpineTwistDegrees(10.f, -10.f, Settings), 35.f, 0.01f);
	TestEqual(TEXT("Aim far right clamps"), UHawkeyeBowIKAnimInstance::ComputeSpineTwistDegrees(170.f, 0.f, Settings), 60.f, 0.01f);
	TestEqual(TEXT("Across the wrap: 350 is 10 left of 0"), UHawkeyeBowIKAnimInstance::ComputeSpineTwistDegrees(350.f, 0.f, Settings), 5.f, 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHawkeyeBowIKOverrideAtBeginPlay, "Hawkeye.BowIK.PostProcessOverrideSetAtBeginPlay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHawkeyeBowIKOverrideAtBeginPlay::RunTest(const FString& Parameters)
{
	FHawkeyeTestWorld TestWorld;
	const TSubclassOf<UAnimInstance> HandsClass = UHawkeyeBowIKAnimInstance::StaticClass();
	AHawkeyeCharacter* Kate = HawkeyeBowIKTest::SpawnKate(TestWorld, HandsClass);
	AHawkeyeCharacter* Plain = HawkeyeBowIKTest::SpawnKate(TestWorld, nullptr);
	if (!TestNotNull(TEXT("Kate spawned"), Kate) || !TestNotNull(TEXT("A second character spawned"), Plain))
	{
		return false;
	}
	TestTrue(TEXT("Kate's mesh runs the bow hands post-process after BeginPlay"),
		Kate->GetMesh()->GetPostProcessAnimBPClassToBeUsed() == HandsClass);
	TestTrue(TEXT("Without a HandsIKClass the mesh is left alone"), Plain->GetMesh()->GetPostProcessAnimBPClassToBeUsed() == nullptr);

	// Applying again is a no-op, not a second re-initialise.
	Kate->GetBowComponent()->ApplyHandsIK();
	TestTrue(TEXT("Still the same override"), Kate->GetMesh()->GetPostProcessAnimBPClassToBeUsed() == HandsClass);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHawkeyeBowIKHandsOnly, "Hawkeye.BowIK.NoBowNoCrash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHawkeyeBowIKHandsOnly::RunTest(const FString& Parameters)
{
	FHawkeyeTestWorld TestWorld;
	AHawkeyeCharacter* Kate = HawkeyeBowIKTest::SpawnKate(TestWorld, UHawkeyeBowIKAnimInstance::StaticClass());
	UBowComponent* Bow = Kate ? Kate->GetBowComponent() : nullptr;
	if (!TestNotNull(TEXT("Kate with a bow component"), Bow))
	{
		return false;
	}

	// Hands only: no bow in the inventory.
	UHawkeyeBowIKAnimInstance* Hands = NewObject<UHawkeyeBowIKAnimInstance>(Kate->GetMesh());
	Hands->UpdateFromBow(Bow, 0.1f);
	Hands->UpdateFromBow(nullptr, 0.1f);
	TestFalse(TEXT("Nothing to draw"), Bow->StartDraw());
	TestEqual(TEXT("Bow hand stays off"), Hands->GetBowAlpha(), 0.f);
	TestEqual(TEXT("String hand stays off"), Hands->GetDrawAlpha(), 0.f);
	TestTrue(TEXT("No spine turn"), Hands->SpineTwist.IsNearlyZero());
	Bow->bHolsterWhenIdle = false;
	Bow->ApplyHandsIK();
	TestNull(TEXT("No bow, so none in the hand to show"), Bow->GetBowMeshComponent());

	// With a bow and a draw the hands come on over the blend and the targets are filled in.
	UBowDefinition* Definition = NewObject<UBowDefinition>(Kate);
	Kate->GetInventoryComponent()->GiveBow(Definition);
	Bow->SetTestTimeSeconds(10.0);
	TestTrue(TEXT("Draws once a bow is given"), Bow->StartDraw());
	Bow->SetTestTimeSeconds(10.0 + Definition->FullDrawSeconds);
	Hands->UpdateFromBow(Bow, 0.075f);
	TestEqual(TEXT("Half way on"), Hands->GetBowAlpha(), 0.5f, 0.001f);
	Hands->UpdateFromBow(Bow, 0.075f);
	TestEqual(TEXT("Bow hand on"), Hands->GetBowAlpha(), 1.f, 0.001f);
	TestEqual(TEXT("String hand on"), Hands->GetDrawAlpha(), 1.f, 0.001f);
	TestFalse(TEXT("The string hand has somewhere to go"), Hands->RightHandTarget.IsNearlyZero());
	TestTrue(TEXT("The spine turns side-on"), Hands->SpineTwist.Yaw > 0.f);

	// The bow taken away mid-draw: the hands let go, nothing dereferences a missing definition.
	Kate->GetInventoryComponent()->GiveBow(nullptr);
	Hands->UpdateFromBow(Bow, 0.15f);
	TestEqual(TEXT("Bow hand let go"), Hands->GetBowAlpha(), 0.f, 0.001f);
	TestEqual(TEXT("String hand let go"), Hands->GetDrawAlpha(), 0.f, 0.001f);
	return true;
}

#endif

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/Skeleton.h"
#include "Combat/WeaponComponent.h"
#include "Misc/AutomationTest.h"
#include "Player/CastleCharacter.h"
#include "Player/FirstPersonArmsComponent.h"
#include "Tests/CastleTestUtils.h"
#include "UObject/UObjectGlobals.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * The view model's rules: the arms' pose tables name bones the mannequin actually has, the
 * pose follows whether Frank is armed, and his own head is off the body mesh he is inside.
 * How the hands look is not a rule and is checked with the screenshot pass instead.
 */

namespace CastleViewModelTest
{
	/** The skeleton every character here shares, or null when Content is not available. */
	static USkeleton* LoadMannequinSkeleton()
	{
		return LoadObject<USkeleton>(
			nullptr, TEXT("/Game/Mannequin/Character/Mesh/SK_Mannequin_Skeleton"));
	}

	/** Where a bone sits in the reference pose, in component space, by walking up to the root. */
	static FVector ReferenceBoneLocation(const FReferenceSkeleton& ReferenceSkeleton, FName BoneName)
	{
		const TArray<FTransform>& Pose = ReferenceSkeleton.GetRefBonePose();
		FTransform Accumulated = FTransform::Identity;
		int32 Index = ReferenceSkeleton.FindBoneIndex(BoneName);
		while (Pose.IsValidIndex(Index))
		{
			Accumulated = Accumulated * Pose[Index];
			Index = ReferenceSkeleton.GetParentIndex(Index);
		}
		return Accumulated.GetLocation();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleViewModelPoseBonesExist, "Castle.ViewModel.PoseBonesExist",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleViewModelPoseBonesExist::RunTest(const FString& Parameters)
{
	UFirstPersonArmsComponent* Arms = NewObject<UFirstPersonArmsComponent>();

	const TArray<FName> BoneNames = Arms->GetAllPoseBoneNames();
	TestTrue(TEXT("The poses name some bones at all"), BoneNames.Num() > 0);

	for (const FName& BoneName : BoneNames)
	{
		TestFalse(TEXT("No pose entry has an empty bone name"), BoneName.IsNone());
	}

	// A typo in a pose table is silent at runtime - the bone is skipped and the arm simply
	// stays in its reference pose - so the skeleton is the only thing that can catch it.
	USkeleton* Skeleton = CastleViewModelTest::LoadMannequinSkeleton();
	if (!Skeleton)
	{
		AddInfo(TEXT("SK_Mannequin_Skeleton did not load; skipping the bone-name check."));
		return true;
	}

	const FReferenceSkeleton& ReferenceSkeleton = Skeleton->GetReferenceSkeleton();
	for (const FName& BoneName : BoneNames)
	{
		TestTrue(FString::Printf(TEXT("SK_Mannequin has a bone called %s"), *BoneName.ToString()),
			ReferenceSkeleton.FindBoneIndex(BoneName) != INDEX_NONE);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleViewModelFingerBonesExist, "Castle.ViewModel.FingerBonesExist",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleViewModelFingerBonesExist::RunTest(const FString& Parameters)
{
	UFirstPersonArmsComponent* Arms = NewObject<UFirstPersonArmsComponent>();

	const TArray<FName> Fingers = Arms->GetFingerBoneNames();
	// Five fingers, three joints each, two hands. The curl pass writes to every one of them, and
	// a name the skeleton does not have is silently skipped, which is how an open hand happens.
	TestEqual(TEXT("Both hands offer thirty finger joints"), Fingers.Num(), 30);

	USkeleton* Skeleton = CastleViewModelTest::LoadMannequinSkeleton();
	if (!Skeleton)
	{
		AddInfo(TEXT("SK_Mannequin_Skeleton did not load; skipping the finger-bone check."));
		return true;
	}

	const FReferenceSkeleton& ReferenceSkeleton = Skeleton->GetReferenceSkeleton();
	for (const FName& BoneName : Fingers)
	{
		TestTrue(FString::Printf(TEXT("SK_Mannequin has a finger bone called %s"), *BoneName.ToString()),
			ReferenceSkeleton.FindBoneIndex(BoneName) != INDEX_NONE);
	}

	// Not an assertion: the pose tables are solved by hand from these numbers, so the test
	// prints them rather than making the next person open the editor to measure again.
	for (const TCHAR* BoneName : { TEXT("upperarm_r"), TEXT("lowerarm_r"), TEXT("hand_r"),
		TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("hand_l"), TEXT("middle_01_r"), TEXT("head") })
	{
		AddInfo(FString::Printf(TEXT("reference %s at %s"), BoneName,
			*CastleViewModelTest::ReferenceBoneLocation(ReferenceSkeleton, FName(BoneName)).ToCompactString()));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleViewModelPunchAlternates, "Castle.ViewModel.PunchAlternates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleViewModelPunchAlternates::RunTest(const FString& Parameters)
{
	UFirstPersonArmsComponent* Arms = NewObject<UFirstPersonArmsComponent>();

	TestFalse(TEXT("Nothing is punching until something asks for it"), Arms->IsPunching());
	TestEqual(TEXT("And the fists are at rest"), Arms->GetPunchAlpha(), 0.f);

	Arms->PlayPunch();
	TestTrue(TEXT("The first jab is out"), Arms->IsPunching());
	TestTrue(TEXT("And it is the right fist"), Arms->IsPunchingRightHand());

	Arms->PlayPunch();
	TestTrue(TEXT("The second jab is the left"), !Arms->IsPunchingRightHand());

	Arms->PlayPunch();
	TestTrue(TEXT("And the third is the right again"), Arms->IsPunchingRightHand());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleViewModelPoseFollowsWeapon, "Castle.ViewModel.PoseFollowsWeapon",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleViewModelPoseFollowsWeapon::RunTest(const FString& Parameters)
{
	const FCastleTestWorld TestWorld;
	ACastleCharacter* Frank = Cast<ACastleCharacter>(TestWorld.SpawnActor(
		ACastleAimTestCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator));
	if (!Frank)
	{
		AddError(TEXT("Could not spawn the test character."));
		return false;
	}

	UFirstPersonArmsComponent* Arms = Frank->GetArmsMesh();
	UWeaponComponent* Weapon = Frank->GetWeaponComponent();
	if (!Arms || !Weapon)
	{
		AddError(TEXT("The test character has no arms or no weapon component."));
		return false;
	}

	TestTrue(TEXT("The arms are the view model by default"), Frank->UsesArmsMesh());

	Frank->RefreshViewModelForWeapon();
	TestTrue(TEXT("Empty-handed Frank holds his fists up"),
		Arms->GetActivePose() == ECastleArmsPose::Fists);

	Weapon->GiveWeapon(12, 24);
	Frank->RefreshViewModelForWeapon();
	TestTrue(TEXT("With the pistol he takes the pistol grip"),
		Arms->GetActivePose() == ECastleArmsPose::Pistol);
	TestTrue(TEXT("And blends into it rather than snapping"), Arms->GetPoseBlendAlpha() < 1.f);

	Weapon->bHasWeapon = false;
	Frank->RefreshViewModelForWeapon();
	TestTrue(TEXT("Losing the pistol puts the fists back"),
		Arms->GetActivePose() == ECastleArmsPose::Fists);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleViewModelHeadHidden, "Castle.ViewModel.HeadHidden",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleViewModelHeadHidden::RunTest(const FString& Parameters)
{
	const FCastleTestWorld TestWorld;
	ACastleCharacter* Frank = Cast<ACastleCharacter>(TestWorld.SpawnActor(
		ACastleAimTestCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator));
	if (!Frank)
	{
		AddError(TEXT("Could not spawn the test character."));
		return false;
	}

	const TArray<FName> Hidden = Frank->GetHiddenBodyBones();
	TestTrue(TEXT("The head comes off the body Frank is inside"),
		Hidden.Contains(FName(TEXT("head"))));
	TestTrue(TEXT("So does the neck, which would otherwise poke into the camera"),
		Hidden.Contains(FName(TEXT("neck_01"))));
	// With the poseable arms on, the body's own arms would be a second pair in frame.
	TestTrue(TEXT("And the body's arm chains, while the poseable arms are the hands"),
		Hidden.Contains(FName(TEXT("clavicle_r"))));
	TestFalse(TEXT("The spine stays: it is what Frank sees when he looks down"),
		Hidden.Contains(FName(TEXT("spine_01"))));

	return true;
}

#endif

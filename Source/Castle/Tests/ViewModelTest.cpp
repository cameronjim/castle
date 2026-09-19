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

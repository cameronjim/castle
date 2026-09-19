// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/Skeleton.h"
#include "Combat/WeaponComponent.h"
#include "Combat/WeaponDefinition.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Misc/AutomationTest.h"
#include "Player/CastleCharacter.h"
#include "Player/FirstPersonArmsComponent.h"
#include "Player/InventoryComponent.h"
#include "Player/LocomotionAnim.h"
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

/**
 * The first playtest of the fists pose came back as "the hands are completely screwed up, they
 * are like touching". They were: both fists were solved to within 12.5 cm of the centre line and
 * met in the middle of the screen. The targets are data now, so the rule can be checked here and
 * the render only has to confirm it.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleViewModelFistsDoNotTouch, "Castle.ViewModel.FistsDoNotTouch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleViewModelFistsDoNotTouch::RunTest(const FString& Parameters)
{
	const UFirstPersonArmsComponent* Arms = NewObject<UFirstPersonArmsComponent>();

	const FVector Right = Arms->GetFistTarget(/*bRightHand=*/true);
	const FVector Left = Arms->GetFistTarget(/*bRightHand=*/false);
	const float Centre = Arms->GetViewCentreX();

	// A closed fist is about 9 cm across, so anything under 20 cm apart reads as touching.
	TestTrue(FString::Printf(TEXT("The fists are %.0f cm apart, which is a clear gap"),
		Arms->GetFistSeparation()), Arms->GetFistSeparation() >= 30.f);

	// +X is his left in component space, so the right fist has to sit on the low side of centre.
	TestTrue(TEXT("The right fist is on the right of the centre line"), Right.X < Centre);
	TestTrue(TEXT("The left fist is on the left of it"), Left.X > Centre);
	TestTrue(TEXT("Each fist is at least 10 cm clear of the centre line"),
		FMath::Min(Centre - Right.X, Left.X - Centre) >= 10.f);

	// The lead hand rides higher; a level pair reads as a shove rather than a guard.
	TestTrue(TEXT("The right fist leads, so it is the higher one"), Right.Z > Left.Z);

	// Both still have to be in front of the camera rather than beside it.
	TestTrue(TEXT("Both fists are out in front of the camera"), Right.Y > 20.f && Left.Y > 20.f);

	return true;
}

/**
 * The bug Cameron reported as "the gun is non existent". The screenshot pass armed Frank with
 * WeaponComponent::GiveWeapon, which leaves ActiveDefinition null and so never touches the view
 * model mesh - the Blueprint's default pistol was what showed up. The real game goes through the
 * pickup, whose definition carries a soft pointer to the mesh and its own grip rotation, and that
 * path was the one nothing exercised. This test takes it: an unloaded soft pointer, the pickup's
 * own entry point, and then the mesh has to be there and visible.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleViewModelPickupShowsTheGun, "Castle.ViewModel.PickupShowsTheGun",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleViewModelPickupShowsTheGun::RunTest(const FString& Parameters)
{
	const FCastleTestWorld TestWorld;
	ACastleCharacter* Frank = Cast<ACastleCharacter>(TestWorld.SpawnActor(
		ACastleAimTestCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator));
	if (!Frank || !Frank->GetInventoryComponent() || !Frank->GetWeaponMesh())
	{
		AddError(TEXT("Could not spawn a character with an inventory and a weapon mesh."));
		return false;
	}

	// An engine shape rather than the project's SM_Pistol: tests do not depend on Content, and
	// the point is the soft pointer, not which mesh is on the end of it.
	const TCHAR* MeshPath = TEXT("/Engine/BasicShapes/Cone.Cone");

	UWeaponDefinition* Pistol = NewObject<UWeaponDefinition>();
	Pistol->Slot = EHotbarSlot::Pistol;
	Pistol->bIsMelee = false;
	Pistol->MagazineSize = 12;
	Pistol->DefaultReserve = 24;
	Pistol->ArmsPoseName = FName(TEXT("Pistol"));
	Pistol->ViewModelMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(MeshPath));

	// Deliberately not resolved here: RefreshViewModelForWeapon has to load it itself. Get() on an
	// unloaded soft pointer returns null, and reaching for the mesh that way is how the gun went
	// missing in a packaged run. Whether this particular editor already has the engine shape
	// resident is luck, so it is reported rather than asserted.
	AddInfo(FString::Printf(TEXT("Soft pointer was %s before the refresh."),
		Pistol->ViewModelMesh.Get() ? TEXT("already resident") : TEXT("unloaded")));

	UStaticMeshComponent* WeaponMesh = Frank->GetWeaponMesh();
	TestTrue(TEXT("The weapon mesh starts hidden, because Frank starts empty handed"),
		WeaponMesh->bHiddenInGame);

	TestTrue(TEXT("The pickup path adds the weapon"),
		Frank->GetInventoryComponent()->AddWeaponWithAmmo(Pistol, 8, 16));
	TestEqual(TEXT("And auto-selects it, because Hands were in hand"),
		static_cast<int32>(Frank->GetInventoryComponent()->GetActiveSlot()),
		static_cast<int32>(EHotbarSlot::Pistol));
	TestTrue(TEXT("So the weapon component is armed with that definition"),
		Frank->GetWeaponComponent() != nullptr
			&& Frank->GetWeaponComponent()->GetActiveDefinition() == Pistol);

	Frank->RefreshViewModelForWeapon();

	UStaticMesh* Loaded = LoadObject<UStaticMesh>(nullptr, MeshPath);
	if (!Loaded)
	{
		AddWarning(TEXT("/Engine/BasicShapes/Cone is not available; skipped the mesh assertion."));
	}
	else
	{
		TestTrue(TEXT("The view model is wearing the mesh the definition named"),
			WeaponMesh->GetStaticMesh() == Loaded);
	}

	TestTrue(TEXT("The view model has a mesh at all"), WeaponMesh->GetStaticMesh() != nullptr);
	TestFalse(TEXT("And it is visible, which is the whole complaint"), WeaponMesh->bHiddenInGame);

	return true;
}

/**
 * Frank's body is the mannequin at a -90 degree yaw, exactly like the guards', so his legs face
 * the way he walks when he looks down at them. Same check, same reason: see
 * Castle.Guard.FacesTravelDirection.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleViewModelBodyFacesForward, "Castle.ViewModel.BodyFacesForward",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleViewModelBodyFacesForward::RunTest(const FString& Parameters)
{
	const FCastleTestWorld TestWorld;
	ACastleCharacter* Frank = Cast<ACastleCharacter>(TestWorld.SpawnActor(
		ACastleAimTestCharacter::StaticClass(), FVector::ZeroVector, FRotator(0.f, 35.f, 0.f)));
	if (!Frank || !Frank->GetMesh())
	{
		AddError(TEXT("Could not spawn a character with a body mesh."));
		return false;
	}

	const FVector Facing = CastleLocomotion::GetMeshFacing(Frank->GetMesh());
	TestTrue(TEXT("The body mesh faces the way the actor does"),
		FVector::DotProduct(Facing.GetSafeNormal2D(), Frank->GetActorForwardVector()) > 0.99f);
	TestTrue(TEXT("Walking forwards, his legs point forwards"),
		CastleLocomotion::GetFacingAlongVelocity(
			Frank->GetMesh(), Frank->GetActorForwardVector() * 450.f) > 0.7f);

	return true;
}

#endif

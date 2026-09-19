// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/AutomationTest.h"
#include "Player/LocomotionAnim.h"
#include "Tests/CastleTestUtils.h"
#include "World/GuardCharacter.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * How a guard reads: a torch cone that shows where he is looking, and a body that is idling or
 * walking rather than standing in a T-pose. The mannequin pack's AnimBP does not compile in a
 * headless editor, so the guard drives two sequences itself.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleGuardHasFlashlight, "Castle.Guard.HasFlashlight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleGuardHasFlashlight::RunTest(const FString& Parameters)
{
	FCastleTestWorld TestWorld;

	AGuardCharacter* Guard = Cast<AGuardCharacter>(TestWorld.SpawnActor(
		AGuardCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator));
	if (!TestNotNull(TEXT("Guard spawned"), Guard))
	{
		return false;
	}

	USpotLightComponent* Flashlight = Guard->GetFlashlight();
	if (!TestNotNull(TEXT("The guard carries a flashlight"), Flashlight))
	{
		return false;
	}

	TestTrue(TEXT("It is on"), Flashlight->GetVisibleFlag());
	TestEqual(TEXT("Cone angles are 25 inner / 35 outer"), Flashlight->OuterConeAngle, 35.f);
	TestTrue(TEXT("It is movable, so it can follow the head"),
		Flashlight->Mobility == EComponentMobility::Movable);

	// A body on the floor does not keep sweeping the corridor.
	Guard->GoLimp(nullptr);
	TestFalse(TEXT("A downed guard's flashlight is off"), Flashlight->GetVisibleFlag());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleGuardAnimSwitchesOnSpeed, "Castle.Guard.AnimSwitchesOnSpeed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleGuardAnimSwitchesOnSpeed::RunTest(const FString& Parameters)
{
	FCastleTestWorld TestWorld;

	AGuardCharacter* Guard = Cast<AGuardCharacter>(TestWorld.SpawnActor(
		AGuardCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator));
	if (!TestNotNull(TEXT("Guard spawned"), Guard))
	{
		return false;
	}

	// Tests never load Content, so these stand in for the mannequin pack's clips.
	Guard->IdleAnim = NewObject<UAnimSequence>();
	Guard->WalkAnim = NewObject<UAnimSequence>();

	UCharacterMovementComponent* Movement = Guard->GetCharacterMovement();
	if (!TestNotNull(TEXT("Guard has movement"), Movement))
	{
		return false;
	}

	Movement->Velocity = FVector::ZeroVector;
	Guard->UpdateLocomotionAnimation();
	TestEqual(TEXT("Standing still, he idles"),
		Guard->GetCurrentLocomotionAnim(), Guard->IdleAnim.Get());

	Movement->Velocity = FVector(120.f, 0.f, 0.f);
	Guard->UpdateLocomotionAnimation();
	TestEqual(TEXT("Moving, he walks"),
		Guard->GetCurrentLocomotionAnim(), Guard->WalkAnim.Get());

	// A twitch under the threshold is not walking.
	Movement->Velocity = FVector(5.f, 0.f, 0.f);
	Guard->UpdateLocomotionAnimation();
	TestEqual(TEXT("Below the threshold he is idle again"),
		Guard->GetCurrentLocomotionAnim(), Guard->IdleAnim.Get());

	return true;
}

/**
 * "The guards walk backwards." They did: the movement component was set to chase the controller's
 * rotation, and the AI points that at the player the moment it sees him, so an alerted guard
 * closing the distance played a forward walk cycle while travelling sideways or backwards.
 *
 * Two things have to hold for the walk cycle to point the right way, and this checks both: the
 * body turns to face its own velocity, and the mannequin is mounted at the -90 degree yaw that
 * makes the mesh's right vector the actor's forward.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleGuardFacesTravelDirection, "Castle.Guard.FacesTravelDirection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleGuardFacesTravelDirection::RunTest(const FString& Parameters)
{
	FCastleTestWorld TestWorld;

	AGuardCharacter* Guard = Cast<AGuardCharacter>(TestWorld.SpawnActor(
		AGuardCharacter::StaticClass(), FVector::ZeroVector, FRotator(0.f, 125.f, 0.f)));
	UCharacterMovementComponent* Movement = Guard ? Guard->GetCharacterMovement() : nullptr;
	if (!TestNotNull(TEXT("Guard has movement"), Movement) || !Guard->GetMesh())
	{
		return false;
	}

	TestTrue(TEXT("He turns to face where he is going"), Movement->bOrientRotationToMovement);
	TestFalse(TEXT("And not to face wherever the AI is aiming"),
		Movement->bUseControllerDesiredRotation);
	TestFalse(TEXT("The pawn does not take the controller's yaw either"),
		Guard->bUseControllerRotationYaw);

	// The mesh is the mannequin, mounted at -90 degrees, so its right vector is the body's front.
	const FVector Facing = CastleLocomotion::GetMeshFacing(Guard->GetMesh());
	TestTrue(TEXT("The mesh faces the way the actor does"),
		FVector::DotProduct(Facing.GetSafeNormal2D(), Guard->GetActorForwardVector()) > 0.99f);

	// bOrientRotationToMovement keeps the actor pointed along its velocity, so this is what a
	// patrolling guard looks like three seconds in.
	const FVector Forward = Guard->GetActorForwardVector() * 300.f;
	TestTrue(TEXT("Walking his own way, the body leads"),
		CastleLocomotion::GetFacingAlongVelocity(Guard->GetMesh(), Forward) > 0.7f);

	// And the check has teeth: the moonwalk it was written for fails it.
	TestTrue(TEXT("Walking the other way is exactly what this catches"),
		CastleLocomotion::GetFacingAlongVelocity(Guard->GetMesh(), -Forward) < -0.7f);

	// Standing still is not facing the wrong way; it has no direction to be wrong about.
	TestEqual(TEXT("A stopped guard passes"),
		CastleLocomotion::GetFacingAlongVelocity(Guard->GetMesh(), FVector::ZeroVector), 1.f);

	return true;
}

#endif

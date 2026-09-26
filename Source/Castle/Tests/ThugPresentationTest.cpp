// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/AutomationTest.h"
#include "Player/LocomotionAnim.h"
#include "Tests/CastleTestUtils.h"
#include "World/ThugCharacter.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * How a thug reads: a street thug in a tracksuit, not a guard with a head torch. No light on him;
 * the bat shows only in a Bat thug's hand; the body idles or walks rather than standing in a
 * T-pose (the mannequin pack's AnimBP does not compile headless, so the thug drives sequences).
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleThugStreetLook, "Castle.Thug.StreetLook",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleThugStreetLook::RunTest(const FString& Parameters)
{
	FCastleTestWorld TestWorld;

	AThugCharacter* Thug = Cast<AThugCharacter>(TestWorld.SpawnActor(
		AThugCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator));
	if (!TestNotNull(TEXT("Thug spawned"), Thug))
	{
		return false;
	}

	TestNull(TEXT("Street thugs carry no flashlight"), Thug->FindComponentByClass<USpotLightComponent>());

	UStaticMeshComponent* Held = Thug->GetHeldWeaponComponent();
	if (!TestNotNull(TEXT("He has a hand to hold a bat in"), Held))
	{
		return false;
	}
	Thug->BatMesh = NewObject<UStaticMesh>();
	Thug->Weapon = EThugWeapon::Fists;
	Thug->RefreshHeldWeapon();
	TestFalse(TEXT("A Fists thug shows no bat"), Held->GetVisibleFlag());
	Thug->Weapon = EThugWeapon::Bat;
	Thug->RefreshHeldWeapon();
	TestTrue(TEXT("A Bat thug shows it"), Held->GetVisibleFlag());
	TestTrue(TEXT("And it is the bat mesh"), Held->GetStaticMesh() == Thug->BatMesh.Get());
	TestTrue(TEXT("The bat never collides; the swing is a sweep"),
		Held->GetCollisionEnabled() == ECollisionEnabled::NoCollision);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleThugAnimSwitchesOnSpeed, "Castle.Thug.AnimSwitchesOnSpeed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleThugAnimSwitchesOnSpeed::RunTest(const FString& Parameters)
{
	FCastleTestWorld TestWorld;

	AThugCharacter* Thug = Cast<AThugCharacter>(TestWorld.SpawnActor(
		AThugCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator));
	if (!TestNotNull(TEXT("Thug spawned"), Thug))
	{
		return false;
	}

	// Tests never load Content, so these stand in for the mannequin pack's clips.
	Thug->IdleAnim = NewObject<UAnimSequence>();
	Thug->WalkAnim = NewObject<UAnimSequence>();

	UCharacterMovementComponent* Movement = Thug->GetCharacterMovement();
	if (!TestNotNull(TEXT("Thug has movement"), Movement))
	{
		return false;
	}

	Movement->Velocity = FVector::ZeroVector;
	Thug->UpdateLocomotionAnimation();
	TestEqual(TEXT("Standing still, he idles"),
		Thug->GetCurrentLocomotionAnim(), Thug->IdleAnim.Get());

	Movement->Velocity = FVector(120.f, 0.f, 0.f);
	Thug->UpdateLocomotionAnimation();
	TestEqual(TEXT("Moving, he walks"),
		Thug->GetCurrentLocomotionAnim(), Thug->WalkAnim.Get());

	// A twitch under the threshold is not walking.
	Movement->Velocity = FVector(5.f, 0.f, 0.f);
	Thug->UpdateLocomotionAnimation();
	TestEqual(TEXT("Below the threshold he is idle again"),
		Thug->GetCurrentLocomotionAnim(), Thug->IdleAnim.Get());

	return true;
}

/**
 * "The thugs walk backwards." They did: the movement component was set to chase the controller's
 * rotation, and the AI points that at the player the moment it sees him, so an alerted thug
 * closing the distance played a forward walk cycle while travelling sideways or backwards.
 *
 * Two things have to hold for the walk cycle to point the right way, and this checks both: the
 * body turns to face its own velocity, and the mannequin is mounted at the -90 degree yaw that
 * makes the mesh's right vector the actor's forward.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleThugFacesTravelDirection, "Castle.Thug.FacesTravelDirection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleThugFacesTravelDirection::RunTest(const FString& Parameters)
{
	FCastleTestWorld TestWorld;

	AThugCharacter* Thug = Cast<AThugCharacter>(TestWorld.SpawnActor(
		AThugCharacter::StaticClass(), FVector::ZeroVector, FRotator(0.f, 125.f, 0.f)));
	UCharacterMovementComponent* Movement = Thug ? Thug->GetCharacterMovement() : nullptr;
	if (!TestNotNull(TEXT("Thug has movement"), Movement) || !Thug->GetMesh())
	{
		return false;
	}

	TestTrue(TEXT("He turns to face where he is going"), Movement->bOrientRotationToMovement);
	TestFalse(TEXT("And not to face wherever the AI is aiming"),
		Movement->bUseControllerDesiredRotation);
	TestFalse(TEXT("The pawn does not take the controller's yaw either"),
		Thug->bUseControllerRotationYaw);

	// The mesh is the mannequin, mounted at -90 degrees, so its right vector is the body's front.
	const FVector Facing = CastleLocomotion::GetMeshFacing(Thug->GetMesh());
	TestTrue(TEXT("The mesh faces the way the actor does"),
		FVector::DotProduct(Facing.GetSafeNormal2D(), Thug->GetActorForwardVector()) > 0.99f);

	// bOrientRotationToMovement keeps the actor pointed along its velocity, so this is what a
	// patrolling thug looks like three seconds in.
	const FVector Forward = Thug->GetActorForwardVector() * 300.f;
	TestTrue(TEXT("Walking his own way, the body leads"),
		CastleLocomotion::GetFacingAlongVelocity(Thug->GetMesh(), Forward) > 0.7f);

	// And the check has teeth: the moonwalk it was written for fails it.
	TestTrue(TEXT("Walking the other way is exactly what this catches"),
		CastleLocomotion::GetFacingAlongVelocity(Thug->GetMesh(), -Forward) < -0.7f);

	// Standing still is not facing the wrong way; it has no direction to be wrong about.
	TestEqual(TEXT("A stopped thug passes"),
		CastleLocomotion::GetFacingAlongVelocity(Thug->GetMesh(), FVector::ZeroVector), 1.f);

	return true;
}

#endif

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimSequence.h"
#include "Components/SpotLightComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/AutomationTest.h"
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
	Guard->GoLimp();
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

#endif

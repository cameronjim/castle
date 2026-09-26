// Copyright Epic Games, Inc. All Rights Reserved.

#include "Camera/CameraComponent.h"
#include "Combat/HealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Misc/AutomationTest.h"
#include "Player/CastleCharacter.h"
#include "Tests/CastleTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * The third-person camera and movement rules from claude-docs/gameplay-semantics.md (PLANNED:
 * third-person camera and look; PLANNED: traversal), plus the stage 2 feel numbers: camera
 * targets and blend, gait selection, jump apex, slide, landing roll and fall damage.
 */
namespace CastleMovementTest
{
	static ACastleAimTestCharacter* Spawn(const FCastleTestWorld& TestWorld, const FVector& Location = FVector::ZeroVector)
	{
		return Cast<ACastleAimTestCharacter>(TestWorld.SpawnActor(
			ACastleAimTestCharacter::StaticClass(), Location, FRotator::ZeroRotator));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleCameraTargetsTest, "Castle.Camera.Targets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleCameraTargetsTest::RunTest(const FString& Parameters)
{
	const FCastleTestWorld TestWorld;
	ACastleAimTestCharacter* Kate = CastleMovementTest::Spawn(TestWorld);
	if (!TestNotNull(TEXT("Kate spawned"), Kate))
	{
		return false;
	}

	const FCastleCameraTargets Hip = Kate->ComputeCameraTargets(false);
	TestEqual(TEXT("Hip arm is 350"), Hip.ArmLength, 350.f);
	TestEqual(TEXT("Hip socket is lifted 60 and 70 right"), Hip.SocketOffset, FVector(0.f, 70.f, 60.f));
	TestEqual(TEXT("Hip FOV is 90"), Hip.FieldOfView, 90.f);

	const FCastleCameraTargets Aim = Kate->ComputeCameraTargets(true);
	TestEqual(TEXT("Aim arm is 180"), Aim.ArmLength, 180.f);
	TestEqual(TEXT("Aim socket is over the right shoulder"), Aim.SocketOffset, FVector(0.f, 45.f, 55.f));
	TestEqual(TEXT("Aim FOV is 70"), Aim.FieldOfView, 70.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleCameraLookUp, "Castle.Camera.LookUpShortensAndLifts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleCameraLookUp::RunTest(const FString& Parameters)
{
	const FCastleTestWorld TestWorld;
	ACastleAimTestCharacter* Kate = CastleMovementTest::Spawn(TestWorld);
	if (!TestNotNull(TEXT("Kate spawned"), Kate))
	{
		return false;
	}

	struct FCase
	{
		float Pitch;
		float Arm;
		float SocketZ;
	};
	// 350 / 60 up to +20 degrees, a straight blend to 220 / 110 at +60, held beyond.
	const FCase Cases[] = {
		{ -40.f, 350.f, 60.f }, { 0.f, 350.f, 60.f }, { 20.f, 350.f, 60.f }, { 30.f, 317.5f, 72.5f },
		{ 40.f, 285.f, 85.f }, { 60.f, 220.f, 110.f }, { 75.f, 220.f, 110.f },
	};
	for (const FCase& Case : Cases)
	{
		const FCastleCameraTargets Hip = Kate->ComputeCameraTargets(false, Case.Pitch);
		TestEqual(FString::Printf(TEXT("Arm at %.0f degrees"), Case.Pitch), Hip.ArmLength, Case.Arm, 0.01f);
		TestEqual(FString::Printf(TEXT("Socket Z at %.0f degrees"), Case.Pitch), static_cast<float>(Hip.SocketOffset.Z), Case.SocketZ, 0.01f);
		TestEqual(FString::Printf(TEXT("Shoulder offset unchanged at %.0f degrees"), Case.Pitch), static_cast<float>(Hip.SocketOffset.Y), 70.f);
		TestEqual(FString::Printf(TEXT("FOV unchanged at %.0f degrees"), Case.Pitch), Hip.FieldOfView, 90.f);
	}
	TestEqual(TEXT("Aiming ignores the pitch"), Kate->ComputeCameraTargets(true, 60.f).ArmLength, 180.f);
	TestEqual(TEXT("The one-argument form is the level camera"), Kate->ComputeCameraTargets(false).ArmLength, 350.f);

	TestEqual(TEXT("Pitch clamps at +75"), Kate->ClampCameraPitch(89.f), 75.f);
	TestEqual(TEXT("Pitch clamps at -70"), Kate->ClampCameraPitch(-89.f), -70.f);
	TestEqual(TEXT("Pitch inside the limits is untouched"), Kate->ClampCameraPitch(30.f), 30.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleCameraAimBlend, "Castle.Camera.AimBlendEndpoints",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleCameraAimBlend::RunTest(const FString& Parameters)
{
	const FCastleTestWorld TestWorld;
	ACastleAimTestCharacter* Kate = CastleMovementTest::Spawn(TestWorld);
	if (!Kate || !Kate->GetCameraBoom())
	{
		AddError(TEXT("Could not spawn a character with a camera boom."));
		return false;
	}
	USpringArmComponent* Boom = Kate->GetCameraBoom();

	Kate->TestTickAim(0.f);
	TestEqual(TEXT("Starts on the hip arm"), Boom->TargetArmLength, 350.f);

	Kate->StartAim();
	Kate->TestTickAim(0.f);
	TestEqual(TEXT("No snap on the frame aim starts"), Boom->TargetArmLength, 350.f);

	Kate->TestTickAim(Kate->TestAimBlendSeconds() * 0.5f);
	TestTrue(TEXT("Half a blend is half way in"), FMath::IsNearlyEqual(Boom->TargetArmLength, 265.f, 1.f));

	Kate->TestTickAim(Kate->TestAimBlendSeconds());
	TestTrue(TEXT("A full blend reaches 180"), FMath::IsNearlyEqual(Boom->TargetArmLength, 180.f, 0.01f));
	TestTrue(TEXT("Over the right shoulder"), Boom->SocketOffset.Equals(FVector(0.f, 45.f, 55.f), 0.01f));
	TestTrue(TEXT("At the aim FOV"), FMath::IsNearlyEqual(Kate->GetCurrentFOV(), 70.f, 0.01f));

	Kate->StopAim();
	Kate->TestTickAim(Kate->TestAimBlendSeconds());
	TestTrue(TEXT("Back to 350"), FMath::IsNearlyEqual(Boom->TargetArmLength, 350.f, 0.01f));
	TestTrue(TEXT("Back to the hip offset"), Boom->SocketOffset.Equals(FVector(0.f, 70.f, 60.f), 0.01f));
	TestTrue(TEXT("Back to the hip FOV"), FMath::IsNearlyEqual(Kate->GetCurrentFOV(), 90.f, 0.01f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleMovementGaitSelection, "Castle.Movement.GaitSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleMovementGaitSelection::RunTest(const FString& Parameters)
{
	const FCastleTestWorld TestWorld;
	ACastleAimTestCharacter* Kate = CastleMovementTest::Spawn(TestWorld);
	if (!TestNotNull(TEXT("Kate spawned"), Kate))
	{
		return false;
	}

	auto Gait = [Kate](float Magnitude, float Held, bool bSprint, bool bCrouch, bool bSlide, bool bAim)
	{
		return Kate->SelectGait(Magnitude, Held, bSprint, bCrouch, bSlide, bAim);
	};

	TestEqual(TEXT("Keyboard runs at once"), Gait(1.f, 0.f, false, false, false, false), ECastleGait::Run);
	TestEqual(TEXT("A full stick runs at once"), Gait(0.95f, 0.f, false, false, false, false), ECastleGait::Run);
	TestEqual(TEXT("A medium stick walks at first"), Gait(0.6f, 0.1f, false, false, false, false), ECastleGait::Walk);
	TestEqual(TEXT("And runs once held past 0.2 s"), Gait(0.6f, 0.25f, false, false, false, false), ECastleGait::Run);
	TestEqual(TEXT("A light stick always walks"), Gait(0.3f, 5.f, false, false, false, false), ECastleGait::Walk);
	TestEqual(TEXT("Shift sprints"), Gait(1.f, 1.f, true, false, false, false), ECastleGait::Sprint);
	TestEqual(TEXT("Aiming walks"), Gait(1.f, 1.f, false, false, false, true), ECastleGait::Walk);
	TestEqual(TEXT("Crouch beats sprint"), Gait(1.f, 1.f, true, true, false, false), ECastleGait::Crouch);
	TestEqual(TEXT("Slide beats crouch"), Gait(1.f, 1.f, true, true, true, false), ECastleGait::Slide);

	TestEqual(TEXT("Walk is 250"), Kate->GetGaitSpeed(ECastleGait::Walk), 250.f);
	TestEqual(TEXT("Run is 500"), Kate->GetGaitSpeed(ECastleGait::Run), 500.f);
	TestEqual(TEXT("Sprint is 700"), Kate->GetGaitSpeed(ECastleGait::Sprint), 700.f);
	TestEqual(TEXT("Crouch is 200"), Kate->GetGaitSpeed(ECastleGait::Crouch), 200.f);

	Kate->TestSetMoveInput(1.f, 0.f);
	TestEqual(TEXT("The movement component gets the run speed"), Kate->MaxWalkSpeed(), 500.f);
	Kate->TestSetSprinting(true);
	TestEqual(TEXT("And the sprint speed"), Kate->MaxWalkSpeed(), 700.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleFallDamageCurve, "Castle.Movement.FallDamageCurve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleFallDamageCurve::RunTest(const FString& Parameters)
{
	const FCastleTestWorld TestWorld;
	ACastleAimTestCharacter* Kate = CastleMovementTest::Spawn(TestWorld);
	if (!TestNotNull(TEXT("Kate spawned"), Kate))
	{
		return false;
	}

	TestEqual(TEXT("Nothing at 899 cm"), Kate->ComputeFallDamageFraction(899.f), 0.f);
	TestTrue(TEXT("Something at 900 cm"), Kate->ComputeFallDamageFraction(900.f) > 0.f);
	TestTrue(TEXT("More from higher up"),
		Kate->ComputeFallDamageFraction(1700.f) > Kate->ComputeFallDamageFraction(900.f));
	TestTrue(TEXT("60% at 2500 cm"), FMath::IsNearlyEqual(Kate->ComputeFallDamageFraction(2500.f), 0.6f));
	TestTrue(TEXT("Capped at 60% above it"), FMath::IsNearlyEqual(Kate->ComputeFallDamageFraction(6000.f), 0.6f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleFallNeverKills, "Castle.Movement.FallNeverKills",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleFallNeverKills::RunTest(const FString& Parameters)
{
	const FCastleTestWorld TestWorld;
	ACastleAimTestCharacter* Kate = CastleMovementTest::Spawn(TestWorld);
	UHealthComponent* Health = Kate ? Kate->GetHealthComponent() : nullptr;
	if (!Health)
	{
		AddError(TEXT("Could not spawn a character with health."));
		return false;
	}

	const float Max = Health->GetMaxHealth();
	Kate->TestApplyLanding(2500.f);
	TestTrue(TEXT("A tenement drop costs 60%"), FMath::IsNearlyEqual(Health->GetCurrentHealth(), Max * 0.4f, 0.01f));
	TestEqual(TEXT("The landing height is recorded"), Kate->GetLastFallHeight(), 2500.f);

	Kate->TestApplyLanding(2500.f);
	TestTrue(TEXT("A second one leaves her standing"), Health->IsAlive());
	TestTrue(TEXT("On one point"), FMath::IsNearlyEqual(Health->GetCurrentHealth(), 1.f, 0.01f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleLandingRoll, "Castle.Movement.LandingRoll",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleLandingRoll::RunTest(const FString& Parameters)
{
	const FCastleTestWorld TestWorld;
	ACastleAimTestCharacter* Kate = CastleMovementTest::Spawn(TestWorld);
	if (!TestNotNull(TEXT("Kate spawned"), Kate))
	{
		return false;
	}

	Kate->TestSetMoveInput(1.f, 1.f);
	Kate->TestApplyLanding(390.f);
	TestFalse(TEXT("A 3.9 m drop does not roll"), Kate->IsRecoveringFromLanding());
	TestEqual(TEXT("Nor cost health"), Kate->GetHealthComponent()->GetCurrentHealth(),
		Kate->GetHealthComponent()->GetMaxHealth());

	Kate->TestApplyLanding(450.f);
	TestTrue(TEXT("A 4.5 m drop rolls"), Kate->IsRecoveringFromLanding());
	TestEqual(TEXT("At half speed"), Kate->MaxWalkSpeed(), Kate->TestRunSpeed() * 0.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleSlideDurationAndCapsule, "Castle.Movement.SlideDurationAndCapsule",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleSlideDurationAndCapsule::RunTest(const FString& Parameters)
{
	const FCastleTestWorld TestWorld;
	ACastleAimTestCharacter* Kate = CastleMovementTest::Spawn(TestWorld);
	UCharacterMovementComponent* Movement = Kate ? Kate->GetCharacterMovement() : nullptr;
	if (!Movement)
	{
		AddError(TEXT("Could not spawn a character with movement."));
		return false;
	}

	const float Standing = Kate->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	Movement->SetMovementMode(MOVE_Walking);
	Movement->Velocity = FVector(700.f, 0.f, 0.f);

	TestFalse(TEXT("No slide without a sprint"), Kate->StartSlide());

	Kate->TestSetSprinting(true);
	Movement->Velocity = FVector(100.f, 0.f, 0.f);
	TestFalse(TEXT("No slide from a crawl"), Kate->StartSlide());

	Movement->Velocity = FVector(700.f, 0.f, 0.f);
	TestTrue(TEXT("Crouch mid-sprint slides"), Kate->StartSlide());
	TestTrue(TEXT("Sliding"), Kate->IsSliding());
	TestEqual(TEXT("On the slide gait"), Kate->GetGait(), ECastleGait::Slide);
	TestTrue(TEXT("The capsule is shrunk"),
		Kate->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() < Standing - 10.f);

	Kate->TestTickSlide(Kate->TestSlideSeconds() - 0.05f);
	TestTrue(TEXT("Still sliding just before the end"), Kate->IsSliding());

	Kate->TestTickSlide(0.1f);
	TestFalse(TEXT("Done after 0.7 s"), Kate->IsSliding());
	TestEqual(TEXT("The capsule is back to standing height"),
		Kate->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(), Standing);
	TestFalse(TEXT("And she is not left crouched"), Kate->bIsCrouched);
	return true;
}

/**
 * The jump apex in a real, ticking movement component: no floor, no controller, the component
 * ticked by hand at 60 Hz. The engine integrates falling with the midpoint rule, so the apex is
 * exact to within one frame's travel.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleJumpHeight, "Castle.Movement.JumpHeight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleJumpHeight::RunTest(const FString& Parameters)
{
	const FCastleTestWorld TestWorld;
	ACastleAimTestCharacter* Kate = CastleMovementTest::Spawn(TestWorld, FVector(0.f, 0.f, 50000.f));
	UCharacterMovementComponent* Movement = Kate ? Kate->GetCharacterMovement() : nullptr;
	if (!Movement)
	{
		AddError(TEXT("Could not spawn a character with movement."));
		return false;
	}

	Movement->bRunPhysicsWithNoController = true;
	Movement->SetMovementMode(MOVE_Falling);
	Movement->Velocity = FVector(0.f, 0.f, Movement->JumpZVelocity);

	const float StartZ = Kate->GetActorLocation().Z;
	float ApexZ = StartZ;
	for (int32 Frame = 0; Frame < 90; ++Frame)
	{
		Movement->TickComponent(1.f / 60.f, LEVELTICK_All, nullptr);
		ApexZ = FMath::Max(ApexZ, Kate->GetActorLocation().Z);
	}

	const float Apex = ApexZ - StartZ;
	AddInfo(FString::Printf(TEXT("JumpZVelocity %.1f, apex %.1f cm"), Movement->JumpZVelocity, Apex));
	TestTrue(TEXT("The jump rose at all"), Apex > 1.f);
	TestTrue(TEXT("The apex is JumpHeight within 5 cm"), FMath::IsNearlyEqual(Apex, Kate->TestJumpHeight(), 5.f));
	TestTrue(TEXT("Air control is 0.3"), FMath::IsNearlyEqual(Movement->AirControl, 0.3f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

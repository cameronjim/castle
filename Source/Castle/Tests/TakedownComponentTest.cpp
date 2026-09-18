// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/TakedownComponent.h"
#include "Combat/Takedownable.h"
#include "Misc/AutomationTest.h"
#include "Tests/CastleTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CastleTakedownTest
{
	/** A point Degrees away from directly behind a target that stands at the origin facing +X. */
	static FVector BehindAt(float Degrees, float Distance)
	{
		const float Radians = FMath::DegreesToRadians(Degrees);
		return FVector(-FMath::Cos(Radians) * Distance, FMath::Sin(Radians) * Distance, 0.f);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleTakedownAngleCheck, "Castle.Takedown.AngleCheck",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleTakedownAngleCheck::RunTest(const FString& Parameters)
{
	const FVector TargetLocation = FVector::ZeroVector;
	const FVector TargetForward = FVector::ForwardVector;
	const float MaxAngle = 60.f;

	TestTrue(TEXT("Directly behind passes"),
		UTakedownComponent::IsBehindTarget(CastleTakedownTest::BehindAt(0.f, 100.f), TargetLocation, TargetForward, MaxAngle));

	TestTrue(TEXT("Just inside the cone passes"),
		UTakedownComponent::IsBehindTarget(CastleTakedownTest::BehindAt(59.f, 100.f), TargetLocation, TargetForward, MaxAngle));

	TestTrue(TEXT("Exactly at MaxAngleDegrees passes"),
		UTakedownComponent::IsBehindTarget(CastleTakedownTest::BehindAt(60.f, 100.f), TargetLocation, TargetForward, MaxAngle));

	TestFalse(TEXT("Just outside the cone fails"),
		UTakedownComponent::IsBehindTarget(CastleTakedownTest::BehindAt(61.f, 100.f), TargetLocation, TargetForward, MaxAngle));

	TestFalse(TEXT("Directly in front fails"),
		UTakedownComponent::IsBehindTarget(FVector(100.f, 0.f, 0.f), TargetLocation, TargetForward, MaxAngle));

	TestFalse(TEXT("Directly beside fails"),
		UTakedownComponent::IsBehindTarget(FVector(0.f, 100.f, 0.f), TargetLocation, TargetForward, MaxAngle));

	// Height must not matter: the check is ground-projected.
	TestTrue(TEXT("Standing on a crate behind the target still passes"),
		UTakedownComponent::IsBehindTarget(FVector(-100.f, 0.f, 300.f), TargetLocation, TargetForward, MaxAngle));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleTakedownRangeCheck, "Castle.Takedown.RangeCheck",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleTakedownRangeCheck::RunTest(const FString& Parameters)
{
	FCastleTestWorld TestWorld;
	UTakedownComponent* Takedown = NewObject<UTakedownComponent>();

	TestEqual(TEXT("Default range"), Takedown->Range, 150.f);
	TestEqual(TEXT("Default cone"), Takedown->MaxAngleDegrees, 60.f);

	AActor* Guard = TestWorld.SpawnActor(ACastleTestTakedownTarget::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
	if (!Guard)
	{
		AddError(TEXT("Failed to spawn the test guard."));
		return false;
	}
	Guard->Tags.Add(FName(TEXT("Guard")));

	TestTrue(TEXT("Inside range is valid"), Takedown->IsValidTakedownTarget(Guard, FVector(-100.f, 0.f, 0.f)));
	TestFalse(TEXT("Outside range is not"), Takedown->IsValidTakedownTarget(Guard, FVector(-200.f, 0.f, 0.f)));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleTakedownTagRequired, "Castle.Takedown.TagRequired",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleTakedownTagRequired::RunTest(const FString& Parameters)
{
	FCastleTestWorld TestWorld;
	UTakedownComponent* Takedown = NewObject<UTakedownComponent>();

	AActor* Untagged = TestWorld.SpawnActor(ACastleTestTakedownTarget::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
	AActor* PlainActor = TestWorld.SpawnActor(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
	if (!Untagged || !PlainActor)
	{
		AddError(TEXT("Failed to spawn the test actors."));
		return false;
	}
	PlainActor->Tags.Add(FName(TEXT("Guard")));

	const FVector Behind(-100.f, 0.f, 0.f);

	TestFalse(TEXT("An untagged takedownable is rejected"), Takedown->IsValidTakedownTarget(Untagged, Behind));
	TestFalse(TEXT("A tagged actor that is not ITakedownable is rejected"), Takedown->IsValidTakedownTarget(PlainActor, Behind));
	TestFalse(TEXT("Null is rejected"), Takedown->IsValidTakedownTarget(nullptr, Behind));

	Untagged->Tags.Add(FName(TEXT("Guard")));

	// Asserted piece by piece so a failure says which half of the check went wrong.
	TestTrue(TEXT("Tag present"), Untagged->ActorHasTag(FName(TEXT("Guard"))));
	TestTrue(TEXT("Implements ITakedownable"), Untagged->GetClass()->ImplementsInterface(UTakedownable::StaticClass()));
	TestTrue(TEXT("Attacker is behind"),
		UTakedownComponent::IsBehindTarget(Behind, Untagged->GetActorLocation(), Untagged->GetActorForwardVector(), 60.f));
	TestTrue(TEXT("Target does not veto"), ITakedownable::Execute_CanBeTakenDown(Untagged, nullptr));
	TestTrue(TEXT("Tagged and takedownable is accepted"), Takedown->IsValidTakedownTarget(Untagged, Behind));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleTakedownAlertedGuardRejected, "Castle.Takedown.AlertedGuardRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleTakedownAlertedGuardRejected::RunTest(const FString& Parameters)
{
	FCastleTestWorld TestWorld;
	UTakedownComponent* Takedown = NewObject<UTakedownComponent>();

	ACastleTestTakedownTarget* Guard = Cast<ACastleTestTakedownTarget>(
		TestWorld.SpawnActor(ACastleTestTakedownTarget::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator));
	if (!Guard)
	{
		AddError(TEXT("Failed to spawn the test guard."));
		return false;
	}
	Guard->Tags.Add(FName(TEXT("Guard")));

	const FVector Behind(-100.f, 0.f, 0.f);

	TestTrue(TEXT("A calm guard can be taken down"), Takedown->IsValidTakedownTarget(Guard, Behind));

	Guard->bAlerted = true;
	TestFalse(TEXT("An alerted guard cannot"), Takedown->IsValidTakedownTarget(Guard, Behind));

	Takedown->bAlertedGuardsAreValid = true;
	TestTrue(TEXT("bAlertedGuardsAreValid overrides the veto"), Takedown->IsValidTakedownTarget(Guard, Behind));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

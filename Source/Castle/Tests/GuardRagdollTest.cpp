// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/HealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/AutomationTest.h"
#include "Tests/CastleTestUtils.h"
#include "World/GuardCharacter.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CastleRagdollTest
{
	static AGuardCharacter* SpawnGuard(const FCastleTestWorld& TestWorld)
	{
		return Cast<AGuardCharacter>(TestWorld.SpawnActor(
			AGuardCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleGuardGoLimpDisablesTheGuard, "Castle.Guard.GoLimpDisablesTheGuard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleGuardGoLimpDisablesTheGuard::RunTest(const FString& Parameters)
{
	const FCastleTestWorld TestWorld;
	AGuardCharacter* Guard = CastleRagdollTest::SpawnGuard(TestWorld);
	if (!Guard)
	{
		AddError(TEXT("Could not spawn the guard."));
		return false;
	}

	UCapsuleComponent* Capsule = Guard->GetCapsuleComponent();
	UCharacterMovementComponent* Movement = Guard->GetCharacterMovement();

	TestTrue(TEXT("The capsule collides while the guard is alive"),
		Capsule && Capsule->GetCollisionEnabled() != ECollisionEnabled::NoCollision);

	Guard->GoLimp();

	TestTrue(TEXT("The capsule stops colliding"),
		Capsule && Capsule->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
	TestTrue(TEXT("Movement is disabled"),
		Movement && Movement->MovementMode == EMovementMode::MOVE_None);
	TestNull(TEXT("The AI no longer possesses the body"), Guard->GetController());

	// Takedown then bullet, or bullet then death delegate: GoLimp has to be safe twice.
	Guard->GoLimp();
	TestTrue(TEXT("A second GoLimp is harmless"),
		Capsule && Capsule->GetCollisionEnabled() == ECollisionEnabled::NoCollision);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleGuardDeathGoesLimpOnce, "Castle.Guard.DeathGoesLimpOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleGuardDeathGoesLimpOnce::RunTest(const FString& Parameters)
{
	const FCastleTestWorld TestWorld;
	AGuardCharacter* Guard = CastleRagdollTest::SpawnGuard(TestWorld);
	if (!Guard)
	{
		AddError(TEXT("Could not spawn the guard."));
		return false;
	}

	UHealthComponent* Health = Guard->GetHealthComponent();
	if (!Health)
	{
		AddError(TEXT("The guard has no health component."));
		return false;
	}

	TestEqual(TEXT("Guards have 100 HP"), Health->MaxHealth, 100.f);

	Health->ApplyDamage(150.f, nullptr);

	TestFalse(TEXT("The guard is dead"), Health->IsAlive());
	TestTrue(TEXT("And the body went limp"),
		Guard->GetCapsuleComponent()->GetCollisionEnabled() == ECollisionEnabled::NoCollision);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

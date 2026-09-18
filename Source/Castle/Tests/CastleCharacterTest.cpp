// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/WeaponComponent.h"
#include "Misc/AutomationTest.h"
#include "Player/CastleCharacter.h"
#include "Tests/CastleTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CastleAimTest
{
	static ACastleAimTestCharacter* Spawn(const FCastleTestWorld& TestWorld)
	{
		return Cast<ACastleAimTestCharacter>(TestWorld.SpawnActor(
			ACastleAimTestCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleCharacterAimSlowsAndTightens, "Castle.Character.AimSlowsAndTightens",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleCharacterAimSlowsAndTightens::RunTest(const FString& Parameters)
{
	const FCastleTestWorld TestWorld;
	ACastleAimTestCharacter* Frank = CastleAimTest::Spawn(TestWorld);
	if (!Frank)
	{
		AddError(TEXT("Could not spawn the test character."));
		return false;
	}

	UWeaponComponent* Weapon = Frank->GetWeaponComponent();
	if (!Weapon)
	{
		AddError(TEXT("The character has no weapon component."));
		return false;
	}
	Weapon->GiveWeapon(12, 24);

	const float Walk = Frank->TestWalkSpeed();

	TestFalse(TEXT("Starts hip-firing"), Frank->IsAiming());
	TestEqual(TEXT("At full walk speed"), Frank->MaxWalkSpeed(), Walk);

	Frank->StartAim();
	TestTrue(TEXT("Aiming"), Frank->IsAiming());
	TestEqual(TEXT("Walk speed is multiplied down"),
		Frank->MaxWalkSpeed(), Walk * Frank->TestAimSpeedMultiplier());
	TestTrue(TEXT("The weapon is aiming too"), Weapon->IsAiming());
	TestEqual(TEXT("And uses the tight cone"), Weapon->GetCurrentSpreadDegrees(), Weapon->AimSpreadDegrees);

	Frank->StopAim();
	TestFalse(TEXT("No longer aiming"), Frank->IsAiming());
	TestEqual(TEXT("Walk speed restored"), Frank->MaxWalkSpeed(), Walk);
	TestFalse(TEXT("The weapon lowered as well"), Weapon->IsAiming());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleCharacterSprintCancelsAim, "Castle.Character.SprintCancelsAim",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleCharacterSprintCancelsAim::RunTest(const FString& Parameters)
{
	const FCastleTestWorld TestWorld;
	ACastleAimTestCharacter* Frank = CastleAimTest::Spawn(TestWorld);
	if (!Frank)
	{
		AddError(TEXT("Could not spawn the test character."));
		return false;
	}

	UWeaponComponent* Weapon = Frank->GetWeaponComponent();
	Weapon->GiveWeapon(12, 24);

	Frank->StartAim();
	TestTrue(TEXT("Aiming"), Frank->IsAiming());

	Frank->TestSetSprinting(true);
	TestFalse(TEXT("Breaking into a sprint drops the aim"), Frank->IsAiming());
	TestFalse(TEXT("And the weapon with it"), Weapon->IsAiming());
	TestEqual(TEXT("Sprint speed wins"), Frank->MaxWalkSpeed(), Frank->TestSprintSpeed());

	// Holding the aim button while sprinting must not sneak the aim back on.
	Frank->StartAim();
	TestFalse(TEXT("Aiming is refused while sprinting"), Frank->IsAiming());
	TestEqual(TEXT("Still at sprint speed"), Frank->MaxWalkSpeed(), Frank->TestSprintSpeed());

	Frank->TestSetSprinting(false);
	Frank->StartAim();
	TestTrue(TEXT("Aiming works again once the sprint ends"), Frank->IsAiming());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleCharacterAimFOVBlend, "Castle.Character.AimFOVBlend",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleCharacterAimFOVBlend::RunTest(const FString& Parameters)
{
	const FCastleTestWorld TestWorld;
	ACastleAimTestCharacter* Frank = CastleAimTest::Spawn(TestWorld);
	if (!Frank)
	{
		AddError(TEXT("Could not spawn the test character."));
		return false;
	}

	Frank->GetWeaponComponent()->GiveWeapon(12, 24);

	TestEqual(TEXT("Starts at the hip FOV"), Frank->GetCurrentFOV(), Frank->TestHipFOV());

	Frank->StartAim();
	TestEqual(TEXT("The FOV does not snap on the frame the aim starts"),
		Frank->GetCurrentFOV(), Frank->TestHipFOV());

	// Half the blend should land half way between the two values.
	const float Blend = Frank->TestAimBlendSeconds();
	Frank->TestTickAim(Blend * 0.5f);
	const float Midpoint = (Frank->TestHipFOV() + Frank->TestAimFOV()) * 0.5f;
	TestTrue(TEXT("Half a blend is half way down"), FMath::IsNearlyEqual(Frank->GetCurrentFOV(), Midpoint, 0.5f));

	Frank->TestTickAim(Blend);
	TestTrue(TEXT("A full blend arrives at the aim FOV"),
		FMath::IsNearlyEqual(Frank->GetCurrentFOV(), Frank->TestAimFOV(), 0.05f));

	Frank->StopAim();
	Frank->TestTickAim(Blend * 2.f);
	TestTrue(TEXT("Lowering the sights returns to the hip FOV"),
		FMath::IsNearlyEqual(Frank->GetCurrentFOV(), Frank->TestHipFOV(), 0.05f));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "UI/CastleHudWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * The third-person reticle's rules, without building Slate: hidden at the hip, a small dot while
 * aiming, white on a hit and back again. Where it lands on screen is checked by eye.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleHudReticleOnlyWhileAiming, "Castle.Hud.ReticleOnlyWhileAiming",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleHudReticleOnlyWhileAiming::RunTest(const FString& Parameters)
{
	UCastleHudWidget* Hud = NewObject<UCastleHudWidget>();

	TestFalse(TEXT("No reticle at the hip"), Hud->IsReticleVisible());

	Hud->SetReticleAiming(true);
	TestTrue(TEXT("Aiming shows it"), Hud->IsReticleVisible());

	Hud->SetReticleAiming(false);
	TestFalse(TEXT("And lowering hides it again"), Hud->IsReticleVisible());

	return true;
}

/** A 4 pixel dot. A later tweak that quietly grows it into a target is the regression to catch. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleHudReticleSize, "Castle.Hud.ReticleSize",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleHudReticleSize::RunTest(const FString& Parameters)
{
	UCastleHudWidget* Hud = NewObject<UCastleHudWidget>();
	TestEqual(TEXT("The dot is 4 pixels"), Hud->GetReticleSize(), 4.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleHudHitMarkerFlash, "Castle.Hud.HitMarkerFlash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleHudHitMarkerFlash::RunTest(const FString& Parameters)
{
	UCastleHudWidget* Hud = NewObject<UCastleHudWidget>();

	const FLinearColor Resting = Hud->GetReticleColor();
	TestNotEqual(TEXT("At rest the dot is not white"), Resting, FLinearColor::White);

	Hud->FlashHitMarker();
	TestEqual(TEXT("A hit turns it white"), Hud->GetReticleColor(), FLinearColor::White);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleHudDebugLineOffByDefault, "Castle.Hud.DebugLineOffByDefault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleHudDebugLineOffByDefault::RunTest(const FString& Parameters)
{
	UCastleHudWidget* Hud = NewObject<UCastleHudWidget>();
	TestTrue(TEXT("castle.DebugMovement is off unless asked for"), Hud->GetMovementDebugText().IsEmpty());
	return true;
}

#endif

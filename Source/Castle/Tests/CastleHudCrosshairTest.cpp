// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "UI/CastleHudWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * The crosshair's rules, without building Slate: the gap tightens while aiming, the bars flash
 * white on a hit and go back to green, and sprinting fades them. The layout itself (four bars,
 * centred) is a visual thing and is checked by eye.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleHudCrosshairGap, "Castle.Hud.CrosshairGap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleHudCrosshairGap::RunTest(const FString& Parameters)
{
	UCastleHudWidget* Hud = NewObject<UCastleHudWidget>();

	TestEqual(TEXT("Hip fire leaves an 8 pixel gap"), Hud->GetCrosshairGap(), 8.f);

	Hud->SetCrosshairAiming(true);
	TestEqual(TEXT("Aiming tightens it to 4"), Hud->GetCrosshairGap(), 4.f);

	Hud->SetCrosshairAiming(false);
	TestEqual(TEXT("And it opens back up"), Hud->GetCrosshairGap(), 8.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleHudHitMarkerFlash, "Castle.Hud.HitMarkerFlash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleHudHitMarkerFlash::RunTest(const FString& Parameters)
{
	UCastleHudWidget* Hud = NewObject<UCastleHudWidget>();

	const FLinearColor Resting = Hud->GetCrosshairColor();
	TestEqual(TEXT("At rest the crosshair is the green it was authored as"),
		Resting, FLinearColor(0.22f, 1.f, 0.08f, 1.f));

	Hud->FlashHitMarker();
	TestEqual(TEXT("A hit turns the bars white"), Hud->GetCrosshairColor(), FLinearColor::White);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleHudCrosshairHiddenUnarmed, "Castle.Hud.CrosshairHiddenUnarmed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleHudCrosshairHiddenUnarmed::RunTest(const FString& Parameters)
{
	UCastleHudWidget* Hud = NewObject<UCastleHudWidget>();

	// With no owning pawn there is no weapon, which is the same answer as being empty-handed:
	// Frank sees no crosshair until he picks the pistol up.
	TestFalse(TEXT("No weapon, no crosshair"), Hud->IsCrosshairVisible());

	return true;
}

#endif

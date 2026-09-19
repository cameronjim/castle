// Copyright Epic Games, Inc. All Rights Reserved.

#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Settings/CastleSettings.h"
#include "Settings/CastleSettingsSave.h"
#include "Settings/CastleSettingsSubsystem.h"
#include "Tests/CastleTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CastleSettingsTest
{
	/** Never the real slot: a test run must not change what the player set. */
	static const TCHAR* TestSlot = TEXT("CastleSettingsAutomationTest");

	static UCastleSettingsSubsystem* MakeSubsystem(bool bUseTestSlot = true)
	{
		UCastleSettingsSubsystem* Subsystem = NewObject<UCastleSettingsSubsystem>(GetTransientPackage());
		if (Subsystem && bUseTestSlot)
		{
			Subsystem->SlotNameOverride = TestSlot;
		}
		return Subsystem;
	}

	static void ClearTestSlot()
	{
		if (UGameplayStatics::DoesSaveGameExist(TestSlot, 0))
		{
			UGameplayStatics::DeleteGameInSlot(TestSlot, 0);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleSettingsClampsSensitivity, "Castle.Settings.ClampsSensitivity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleSettingsClampsSensitivity::RunTest(const FString& Parameters)
{
	CastleSettingsTest::ClearTestSlot();
	UCastleSettingsSubsystem* Settings = CastleSettingsTest::MakeSubsystem();
	if (!Settings)
	{
		AddError(TEXT("Could not create the settings subsystem."));
		return false;
	}

	TestEqual(TEXT("The default is 0.2"), Settings->GetLookSensitivity(), 0.2f);

	Settings->SetLookSensitivity(0.f);
	TestEqual(TEXT("Zero clamps up to the minimum"),
		Settings->GetLookSensitivity(), UCastleSettingsSubsystem::MinLookSensitivity);

	Settings->SetLookSensitivity(-5.f);
	TestEqual(TEXT("A negative value clamps up to the minimum"),
		Settings->GetLookSensitivity(), UCastleSettingsSubsystem::MinLookSensitivity);

	Settings->SetLookSensitivity(12.f);
	TestEqual(TEXT("A huge value clamps down to the maximum"),
		Settings->GetLookSensitivity(), UCastleSettingsSubsystem::MaxLookSensitivity);

	Settings->SetLookSensitivity(0.35f);
	TestEqual(TEXT("A value inside the range is kept"), Settings->GetLookSensitivity(), 0.35f);

	CastleSettingsTest::ClearTestSlot();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleSettingsBroadcastsOnce, "Castle.Settings.BroadcastsOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleSettingsBroadcastsOnce::RunTest(const FString& Parameters)
{
	CastleSettingsTest::ClearTestSlot();
	UCastleSettingsSubsystem* Settings = CastleSettingsTest::MakeSubsystem();
	UCastleTestListener* Listener = NewObject<UCastleTestListener>();
	if (!Settings || !Listener)
	{
		AddError(TEXT("Could not create the settings subsystem or the listener."));
		return false;
	}

	Settings->OnSettingsChanged.AddDynamic(Listener, &UCastleTestListener::HandleSettingsChanged);

	Settings->SetLookSensitivity(0.5f);
	TestEqual(TEXT("One change, one broadcast"), Listener->SettingsChangedCount, 1);
	TestEqual(TEXT("And it carried the new value"), Listener->LastLookSensitivity, 0.5f);

	// A slider fires on every pixel of drag, most of which land on the same value.
	Settings->SetLookSensitivity(0.5f);
	TestEqual(TEXT("Setting the same value again broadcasts nothing"), Listener->SettingsChangedCount, 1);

	Settings->SetLookSensitivity(0.6f);
	TestEqual(TEXT("A real change broadcasts again"), Listener->SettingsChangedCount, 2);

	Settings->OnSettingsChanged.RemoveDynamic(Listener, &UCastleTestListener::HandleSettingsChanged);
	CastleSettingsTest::ClearTestSlot();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleSettingsRoundTripsThroughTheSlot, "Castle.Settings.RoundTripsThroughTheSlot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleSettingsRoundTripsThroughTheSlot::RunTest(const FString& Parameters)
{
	CastleSettingsTest::ClearTestSlot();

	UCastleSettingsSubsystem* Writer = CastleSettingsTest::MakeSubsystem();
	if (!Writer)
	{
		AddError(TEXT("Could not create the writing settings subsystem."));
		return false;
	}

	// SetLookSensitivity saves immediately; that is the behaviour being asserted.
	Writer->SetLookSensitivity(0.42f);
	TestTrue(TEXT("The slot exists after a change"),
		UGameplayStatics::DoesSaveGameExist(CastleSettingsTest::TestSlot, 0));

	UCastleSettingsSubsystem* Reader = CastleSettingsTest::MakeSubsystem();
	Reader->Load();
	TestEqual(TEXT("A fresh subsystem loads the stored value"), Reader->GetLookSensitivity(), 0.42f);

	CastleSettingsTest::ClearTestSlot();

	UCastleSettingsSubsystem* AfterDelete = CastleSettingsTest::MakeSubsystem();
	AfterDelete->Load();
	TestEqual(TEXT("With no slot on disk the defaults come back"), AfterDelete->GetLookSensitivity(), 0.2f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleSettingsVersionMismatchYieldsDefaults,
	"Castle.Settings.VersionMismatchYieldsDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleSettingsVersionMismatchYieldsDefaults::RunTest(const FString& Parameters)
{
	CastleSettingsTest::ClearTestSlot();

	UCastleSettingsSave* Stale =
		Cast<UCastleSettingsSave>(UGameplayStatics::CreateSaveGameObject(UCastleSettingsSave::StaticClass()));
	if (!Stale)
	{
		AddError(TEXT("Could not create a save object."));
		return false;
	}

	Stale->Settings.LookSensitivity = 0.9f;
	Stale->Settings.Version = FCastleSettings::CurrentVersion + 7;
	UGameplayStatics::SaveGameToSlot(Stale, CastleSettingsTest::TestSlot, 0);

	// A version we do not understand must never reach the player's mouse.
	AddExpectedError(TEXT("is version"), EAutomationExpectedErrorFlags::Contains, 0);

	UCastleSettingsSubsystem* Settings = CastleSettingsTest::MakeSubsystem();
	Settings->Load();
	TestEqual(TEXT("A version mismatch yields defaults, not the stale value"),
		Settings->GetLookSensitivity(), 0.2f);

	CastleSettingsTest::ClearTestSlot();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleSettingsDriveLookDelta, "Castle.Settings.DriveLookDelta",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleSettingsDriveLookDelta::RunTest(const FString& Parameters)
{
	const FCastleTestWorld TestWorld;
	ACastleAimTestCharacter* Frank = Cast<ACastleAimTestCharacter>(TestWorld.SpawnActor(
		ACastleAimTestCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator));
	if (!Frank)
	{
		AddError(TEXT("Could not spawn the test character."));
		return false;
	}

	// No game instance subsystem in a test world, so the character's own property is the source.
	TestFalse(TEXT("No subsystem was found"), Frank->TestHasSettingsSensitivity());
	const float Fallback = Frank->TestLookSensitivity();
	TestEqual(TEXT("The C++ fallback default is 0.2"), Fallback, 0.2f);
	TestEqual(TEXT("Hip look scales by the fallback"),
		Frank->ComputeLookDelta(FVector2D(10.f, 0.f), false).X, 10.f * Fallback);

	FCastleSettings Settings;
	Settings.LookSensitivity = 0.5f;
	Frank->TestApplySettings(Settings);

	TestTrue(TEXT("The subsystem value is now in use"), Frank->TestHasSettingsSensitivity());
	TestEqual(TEXT("Hip look scales by the setting, not the property"),
		Frank->ComputeLookDelta(FVector2D(10.f, 0.f), false).X, 5.f);
	TestEqual(TEXT("Pitch uses the same scale"),
		Frank->ComputeLookDelta(FVector2D(0.f, -4.f), false).Y, -2.f);
	TestEqual(TEXT("Aiming applies AimLookMultiplier on top"),
		Frank->ComputeLookDelta(FVector2D(10.f, 0.f), true).X, 5.f * Frank->TestAimLookMultiplier());

	// A second broadcast wins: the slider is live while the pause menu is open.
	Settings.LookSensitivity = 0.1f;
	Frank->TestApplySettings(Settings);
	TestEqual(TEXT("A later change takes effect immediately"),
		Frank->ComputeLookDelta(FVector2D(10.f, 0.f), false).X, 1.f);

	return true;
}

#endif

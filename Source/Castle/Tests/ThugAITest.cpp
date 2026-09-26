// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/Takedownable.h"
#include "EngineUtils.h"
#include "Combat/HealthComponent.h"
#include "Combat/WeaponComponent.h"
#include "Misc/AutomationTest.h"
#include "Tests/CastleTestUtils.h"
#include "World/ThugAIController.h"
#include "World/ThugCharacter.h"
#include "World/PickupActor.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CastleThugTest
{
	/** A thug with a controller possessing it, so the state machine has a pawn to drive. */
	static AThugAIController* SpawnPossessedThug(const FCastleTestWorld& TestWorld, AThugCharacter*& OutThug)
	{
		OutThug = Cast<AThugCharacter>(
			TestWorld.SpawnActor(AThugCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator));
		AThugAIController* Controller = Cast<AThugAIController>(
			TestWorld.SpawnActor(AThugAIController::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator));

		if (OutThug && Controller)
		{
			Controller->Possess(OutThug);
		}
		return Controller;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleThugDefaults, "Castle.Thug.Defaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleThugDefaults::RunTest(const FString& Parameters)
{
	FCastleTestWorld TestWorld;
	AThugCharacter* Thug = Cast<AThugCharacter>(
		TestWorld.SpawnActor(AThugCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator));
	if (!Thug)
	{
		AddError(TEXT("Failed to spawn the thug."));
		return false;
	}

	TestTrue(TEXT("Tagged Thug so the takedown sweep finds him"), Thug->ActorHasTag(FName(TEXT("Thug"))));
	TestEqual(TEXT("Starts Calm"), Thug->GetAlertState(), EThugAlertState::Calm);

	const UWeaponComponent* Weapon = Thug->GetWeaponComponent();
	if (!Weapon)
	{
		AddError(TEXT("The thug has no UWeaponComponent."));
		return false;
	}
	TestTrue(TEXT("Thugs start armed"), Weapon->HasWeapon());
	TestEqual(TEXT("Thugs hit for 12"), Weapon->Damage, 12.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleThugAlertedRefusesTakedown, "Castle.Thug.AlertedRefusesTakedown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleThugAlertedRefusesTakedown::RunTest(const FString& Parameters)
{
	FCastleTestWorld TestWorld;
	AThugCharacter* Thug = Cast<AThugCharacter>(
		TestWorld.SpawnActor(AThugCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator));
	if (!Thug)
	{
		AddError(TEXT("Failed to spawn the thug."));
		return false;
	}

	TestTrue(TEXT("A calm thug can be taken down"), ITakedownable::Execute_CanBeTakenDown(Thug, nullptr));

	Thug->SetAlertState(EThugAlertState::Suspicious);
	TestTrue(TEXT("A suspicious thug can still be taken down"), ITakedownable::Execute_CanBeTakenDown(Thug, nullptr));

	Thug->SetAlertState(EThugAlertState::Alerted);
	TestFalse(TEXT("An alerted thug cannot"), ITakedownable::Execute_CanBeTakenDown(Thug, nullptr));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleThugAlertStateChangeBroadcasts, "Castle.Thug.AlertStateChangeBroadcasts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleThugAlertStateChangeBroadcasts::RunTest(const FString& Parameters)
{
	FCastleTestWorld TestWorld;
	AThugCharacter* Thug = Cast<AThugCharacter>(
		TestWorld.SpawnActor(AThugCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator));
	if (!Thug)
	{
		AddError(TEXT("Failed to spawn the thug."));
		return false;
	}

	UCastleTestListener* Listener = NewObject<UCastleTestListener>();
	Thug->OnAlertStateChanged.AddDynamic(Listener, &UCastleTestListener::HandleAlertStateChanged);

	Thug->SetAlertState(EThugAlertState::Suspicious);
	TestEqual(TEXT("One broadcast"), Listener->AlertStateChangedCount, 1);
	TestEqual(TEXT("New state reported"), Listener->LastNewAlertState, EThugAlertState::Suspicious);

	Thug->SetAlertState(EThugAlertState::Suspicious);
	TestEqual(TEXT("Setting the same state is silent"), Listener->AlertStateChangedCount, 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleThugHearingDrivesState, "Castle.Thug.HearingDrivesState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleThugHearingDrivesState::RunTest(const FString& Parameters)
{
	FCastleTestWorld TestWorld;
	AThugCharacter* Thug = nullptr;
	AThugAIController* Controller = CastleThugTest::SpawnPossessedThug(TestWorld, Thug);
	if (!Controller || !Thug)
	{
		AddError(TEXT("Failed to spawn the thug and its controller."));
		return false;
	}

	const FVector Noise(800.f, 0.f, 0.f);

	// Footsteps: loudness below the gunshot threshold.
	Controller->ReportStimulus(EStimulusKind::Hearing, Noise, /*bSuccessful=*/true, /*Loudness=*/1.f);
	TestEqual(TEXT("Footsteps make him suspicious"), Thug->GetAlertState(), EThugAlertState::Suspicious);
	TestEqual(TEXT("And he remembers where"), Controller->GetLastStimulusLocation(), Noise);

	// A gunshot is unambiguous.
	Controller->ReportStimulus(EStimulusKind::Hearing, Noise, /*bSuccessful=*/true, /*Loudness=*/3.f);
	TestEqual(TEXT("A gunshot alerts him at once"), Thug->GetAlertState(), EThugAlertState::Alerted);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleThugSightConfirmsBeforeAlerting, "Castle.Thug.SightConfirmsBeforeAlerting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleThugSightConfirmsBeforeAlerting::RunTest(const FString& Parameters)
{
	FCastleTestWorld TestWorld;
	AThugCharacter* Thug = nullptr;
	AThugAIController* Controller = CastleThugTest::SpawnPossessedThug(TestWorld, Thug);
	if (!Controller || !Thug)
	{
		AddError(TEXT("Failed to spawn the thug and its controller."));
		return false;
	}

	Controller->ReportStimulus(EStimulusKind::Sight, FVector(600.f, 0.f, 0.f), /*bSuccessful=*/true);
	TestEqual(TEXT("A first sighting is only suspicious"), Thug->GetAlertState(), EThugAlertState::Suspicious);

	// Still not long enough: SightConfirmSeconds is 0.6.
	Controller->Think(0.25f);
	TestEqual(TEXT("A glimpse does not alert him"), Thug->GetAlertState(), EThugAlertState::Suspicious);

	Controller->Think(0.5f);
	TestEqual(TEXT("A sustained look does"), Thug->GetAlertState(), EThugAlertState::Alerted);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleThugLosesTargetBackToSuspicious, "Castle.Thug.LosesTargetBackToSuspicious",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleThugLosesTargetBackToSuspicious::RunTest(const FString& Parameters)
{
	FCastleTestWorld TestWorld;
	AThugCharacter* Thug = nullptr;
	AThugAIController* Controller = CastleThugTest::SpawnPossessedThug(TestWorld, Thug);
	if (!Controller || !Thug)
	{
		AddError(TEXT("Failed to spawn the thug and its controller."));
		return false;
	}

	Controller->ReportStimulus(EStimulusKind::Hearing, FVector(400.f, 0.f, 0.f), /*bSuccessful=*/true, /*Loudness=*/3.f);
	TestEqual(TEXT("Alerted by the gunshot"), Thug->GetAlertState(), EThugAlertState::Alerted);

	// LoseTargetSeconds is 5; six seconds of nothing drops him back.
	for (int32 Step = 0; Step < 6; ++Step)
	{
		Controller->Think(1.f);
	}

	TestEqual(TEXT("With nothing to shoot at he falls back to Suspicious"),
		Thug->GetAlertState(), EThugAlertState::Suspicious);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleThugDropsLootOnDeath, "Castle.Thug.DropsLootOnDeath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleThugDropsLootOnDeath::RunTest(const FString& Parameters)
{
	FCastleTestWorld TestWorld;
	AThugCharacter* Thug = Cast<AThugCharacter>(
		TestWorld.SpawnActor(AThugCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator));
	if (!Thug)
	{
		AddError(TEXT("Failed to spawn the thug."));
		return false;
	}

	Thug->DropOnDeath.Add(APickupActor::StaticClass());
	Thug->DropOnDeath.Add(APickupActor::StaticClass());

	ITakedownable::Execute_OnTakedown(Thug, nullptr);

	TArray<AActor*> Pickups;
	for (TActorIterator<APickupActor> It(TestWorld.Get()); It; ++It)
	{
		Pickups.Add(*It);
	}

	TestEqual(TEXT("Both pickups dropped"), Pickups.Num(), 2);
	TestFalse(TEXT("The thug is dead"), Thug->GetHealthComponent()->IsAlive());

	// OnDeath also drops, so the takedown must not produce a second set.
	Thug->DropLoot();
	int32 SecondCount = 0;
	for (TActorIterator<APickupActor> It(TestWorld.Get()); It; ++It)
	{
		++SecondCount;
	}
	TestEqual(TEXT("Loot only ever drops once"), SecondCount, 2);

	return true;
}

#endif

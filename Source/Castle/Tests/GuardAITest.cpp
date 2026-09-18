// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/Takedownable.h"
#include "EngineUtils.h"
#include "Combat/HealthComponent.h"
#include "Combat/WeaponComponent.h"
#include "Misc/AutomationTest.h"
#include "Tests/CastleTestUtils.h"
#include "World/GuardAIController.h"
#include "World/GuardCharacter.h"
#include "World/PickupActor.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CastleGuardTest
{
	/** A guard with a controller possessing it, so the state machine has a pawn to drive. */
	static AGuardAIController* SpawnPossessedGuard(const FCastleTestWorld& TestWorld, AGuardCharacter*& OutGuard)
	{
		OutGuard = Cast<AGuardCharacter>(
			TestWorld.SpawnActor(AGuardCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator));
		AGuardAIController* Controller = Cast<AGuardAIController>(
			TestWorld.SpawnActor(AGuardAIController::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator));

		if (OutGuard && Controller)
		{
			Controller->Possess(OutGuard);
		}
		return Controller;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleGuardDefaults, "Castle.Guard.Defaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleGuardDefaults::RunTest(const FString& Parameters)
{
	FCastleTestWorld TestWorld;
	AGuardCharacter* Guard = Cast<AGuardCharacter>(
		TestWorld.SpawnActor(AGuardCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator));
	if (!Guard)
	{
		AddError(TEXT("Failed to spawn the guard."));
		return false;
	}

	TestTrue(TEXT("Tagged Guard so the takedown sweep finds him"), Guard->ActorHasTag(FName(TEXT("Guard"))));
	TestEqual(TEXT("Starts Calm"), Guard->GetAlertState(), EGuardAlertState::Calm);

	const UWeaponComponent* Weapon = Guard->GetWeaponComponent();
	if (!Weapon)
	{
		AddError(TEXT("The guard has no UWeaponComponent."));
		return false;
	}
	TestTrue(TEXT("Guards start armed"), Weapon->HasWeapon());
	TestEqual(TEXT("Guards hit for 12"), Weapon->Damage, 12.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleGuardAlertedRefusesTakedown, "Castle.Guard.AlertedRefusesTakedown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleGuardAlertedRefusesTakedown::RunTest(const FString& Parameters)
{
	FCastleTestWorld TestWorld;
	AGuardCharacter* Guard = Cast<AGuardCharacter>(
		TestWorld.SpawnActor(AGuardCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator));
	if (!Guard)
	{
		AddError(TEXT("Failed to spawn the guard."));
		return false;
	}

	TestTrue(TEXT("A calm guard can be taken down"), ITakedownable::Execute_CanBeTakenDown(Guard, nullptr));

	Guard->SetAlertState(EGuardAlertState::Suspicious);
	TestTrue(TEXT("A suspicious guard can still be taken down"), ITakedownable::Execute_CanBeTakenDown(Guard, nullptr));

	Guard->SetAlertState(EGuardAlertState::Alerted);
	TestFalse(TEXT("An alerted guard cannot"), ITakedownable::Execute_CanBeTakenDown(Guard, nullptr));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleGuardAlertStateChangeBroadcasts, "Castle.Guard.AlertStateChangeBroadcasts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleGuardAlertStateChangeBroadcasts::RunTest(const FString& Parameters)
{
	FCastleTestWorld TestWorld;
	AGuardCharacter* Guard = Cast<AGuardCharacter>(
		TestWorld.SpawnActor(AGuardCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator));
	if (!Guard)
	{
		AddError(TEXT("Failed to spawn the guard."));
		return false;
	}

	UCastleTestListener* Listener = NewObject<UCastleTestListener>();
	Guard->OnAlertStateChanged.AddDynamic(Listener, &UCastleTestListener::HandleAlertStateChanged);

	Guard->SetAlertState(EGuardAlertState::Suspicious);
	TestEqual(TEXT("One broadcast"), Listener->AlertStateChangedCount, 1);
	TestEqual(TEXT("New state reported"), Listener->LastNewAlertState, EGuardAlertState::Suspicious);

	Guard->SetAlertState(EGuardAlertState::Suspicious);
	TestEqual(TEXT("Setting the same state is silent"), Listener->AlertStateChangedCount, 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleGuardHearingDrivesState, "Castle.Guard.HearingDrivesState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleGuardHearingDrivesState::RunTest(const FString& Parameters)
{
	FCastleTestWorld TestWorld;
	AGuardCharacter* Guard = nullptr;
	AGuardAIController* Controller = CastleGuardTest::SpawnPossessedGuard(TestWorld, Guard);
	if (!Controller || !Guard)
	{
		AddError(TEXT("Failed to spawn the guard and its controller."));
		return false;
	}

	const FVector Noise(800.f, 0.f, 0.f);

	// Footsteps: loudness below the gunshot threshold.
	Controller->ReportStimulus(EStimulusKind::Hearing, Noise, /*bSuccessful=*/true, /*Loudness=*/1.f);
	TestEqual(TEXT("Footsteps make him suspicious"), Guard->GetAlertState(), EGuardAlertState::Suspicious);
	TestEqual(TEXT("And he remembers where"), Controller->GetLastStimulusLocation(), Noise);

	// A gunshot is unambiguous.
	Controller->ReportStimulus(EStimulusKind::Hearing, Noise, /*bSuccessful=*/true, /*Loudness=*/3.f);
	TestEqual(TEXT("A gunshot alerts him at once"), Guard->GetAlertState(), EGuardAlertState::Alerted);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleGuardSightConfirmsBeforeAlerting, "Castle.Guard.SightConfirmsBeforeAlerting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleGuardSightConfirmsBeforeAlerting::RunTest(const FString& Parameters)
{
	FCastleTestWorld TestWorld;
	AGuardCharacter* Guard = nullptr;
	AGuardAIController* Controller = CastleGuardTest::SpawnPossessedGuard(TestWorld, Guard);
	if (!Controller || !Guard)
	{
		AddError(TEXT("Failed to spawn the guard and its controller."));
		return false;
	}

	Controller->ReportStimulus(EStimulusKind::Sight, FVector(600.f, 0.f, 0.f), /*bSuccessful=*/true);
	TestEqual(TEXT("A first sighting is only suspicious"), Guard->GetAlertState(), EGuardAlertState::Suspicious);

	// Still not long enough: SightConfirmSeconds is 0.6.
	Controller->Think(0.25f);
	TestEqual(TEXT("A glimpse does not alert him"), Guard->GetAlertState(), EGuardAlertState::Suspicious);

	Controller->Think(0.5f);
	TestEqual(TEXT("A sustained look does"), Guard->GetAlertState(), EGuardAlertState::Alerted);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleGuardLosesTargetBackToSuspicious, "Castle.Guard.LosesTargetBackToSuspicious",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleGuardLosesTargetBackToSuspicious::RunTest(const FString& Parameters)
{
	FCastleTestWorld TestWorld;
	AGuardCharacter* Guard = nullptr;
	AGuardAIController* Controller = CastleGuardTest::SpawnPossessedGuard(TestWorld, Guard);
	if (!Controller || !Guard)
	{
		AddError(TEXT("Failed to spawn the guard and its controller."));
		return false;
	}

	Controller->ReportStimulus(EStimulusKind::Hearing, FVector(400.f, 0.f, 0.f), /*bSuccessful=*/true, /*Loudness=*/3.f);
	TestEqual(TEXT("Alerted by the gunshot"), Guard->GetAlertState(), EGuardAlertState::Alerted);

	// LoseTargetSeconds is 5; six seconds of nothing drops him back.
	for (int32 Step = 0; Step < 6; ++Step)
	{
		Controller->Think(1.f);
	}

	TestEqual(TEXT("With nothing to shoot at he falls back to Suspicious"),
		Guard->GetAlertState(), EGuardAlertState::Suspicious);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleGuardDropsLootOnDeath, "Castle.Guard.DropsLootOnDeath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleGuardDropsLootOnDeath::RunTest(const FString& Parameters)
{
	FCastleTestWorld TestWorld;
	AGuardCharacter* Guard = Cast<AGuardCharacter>(
		TestWorld.SpawnActor(AGuardCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator));
	if (!Guard)
	{
		AddError(TEXT("Failed to spawn the guard."));
		return false;
	}

	Guard->DropOnDeath.Add(APickupActor::StaticClass());
	Guard->DropOnDeath.Add(APickupActor::StaticClass());

	ITakedownable::Execute_OnTakedown(Guard, nullptr);

	TArray<AActor*> Pickups;
	for (TActorIterator<APickupActor> It(TestWorld.Get()); It; ++It)
	{
		Pickups.Add(*It);
	}

	TestEqual(TEXT("Both pickups dropped"), Pickups.Num(), 2);
	TestFalse(TEXT("The guard is dead"), Guard->GetHealthComponent()->IsAlive());

	// OnDeath also drops, so the takedown must not produce a second set.
	Guard->DropLoot();
	int32 SecondCount = 0;
	for (TActorIterator<APickupActor> It(TestWorld.Get()); It; ++It)
	{
		++SecondCount;
	}
	TestEqual(TEXT("Loot only ever drops once"), SecondCount, 2);

	return true;
}

#endif

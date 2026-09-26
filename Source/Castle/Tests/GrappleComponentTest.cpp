// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/ArrowDefinition.h"
#include "Combat/HealthComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/AutomationTest.h"
#include "Player/GrappleComponent.h"
#include "Player/InventoryComponent.h"
#include "Tests/CastleTestUtils.h"
#include "World/GrappleAnchor.h"
#include "World/GrappleArrowProjectile.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * The grapple arrow rules from claude-docs/gameplay-semantics.md (PLANNED: traversal): anchors
 * within 2500 cm and 30 degrees of the camera forward, in sight; a straight zip at 1800 cm/s to
 * the landing point; chaining from 40% of the line, redirected in the air; one arrow per shot, recoverable at the
 * anchor; a blocked zip drops the character.
 */
namespace CastleGrappleTest
{
	/** A grapple arrow definition for slot 2, built in code. */
	static UArrowDefinition* MakeGrappleArrow(UObject* Outer, int32 Cap)
	{
		UArrowDefinition* Arrow = NewObject<UArrowDefinition>(Outer);
		Arrow->Slot = 2;
		Arrow->Cap = Cap;
		Arrow->OnHitEffect = EArrowHitEffect::Grapple;
		Arrow->bRecoverable = true;
		return Arrow;
	}

	/** Kate with GrappleArrows grapple arrows in her quiver (cap 99, so the counts are easy to read). */
	static ACastleAimTestCharacter* SpawnKate(const FCastleTestWorld& TestWorld, const FVector& Location,
		int32 GrappleArrows = 99)
	{
		ACastleAimTestCharacter* Kate = Cast<ACastleAimTestCharacter>(
			TestWorld.SpawnActor(ACastleAimTestCharacter::StaticClass(), Location, FRotator::ZeroRotator));
		if (UInventoryComponent* Inventory = Kate ? Kate->GetInventoryComponent() : nullptr)
		{
			Inventory->AddArrows(MakeGrappleArrow(Inventory, 99), GrappleArrows);
		}
		return Kate;
	}

	static AGrappleAnchor* SpawnAnchor(const FCastleTestWorld& TestWorld, const FVector& Location)
	{
		return Cast<AGrappleAnchor>(
			TestWorld.SpawnActor(AGrappleAnchor::StaticClass(), Location, FRotator::ZeroRotator));
	}

	static ACastleTestBlocker* SpawnBlocker(const FCastleTestWorld& TestWorld, const FVector& Centre,
		const FVector& HalfExtent)
	{
		ACastleTestBlocker* Blocker = Cast<ACastleTestBlocker>(
			TestWorld.SpawnActor(ACastleTestBlocker::StaticClass(), Centre, FRotator::ZeroRotator));
		if (Blocker)
		{
			Blocker->SetExtent(HalfExtent);
		}
		return Blocker;
	}

	/** Advances the zip in DeltaSeconds steps until it ends or MaxSeconds pass. Returns the steps run. */
	static int32 RunZip(UGrappleComponent* Grapple, float DeltaSeconds, float MaxSeconds)
	{
		int32 Steps = 0;
		for (float Elapsed = 0.f; Grapple->IsZipping() && Elapsed < MaxSeconds; Elapsed += DeltaSeconds)
		{
			Grapple->AdvanceZip(DeltaSeconds);
			++Steps;
		}
		return Steps;
	}

	static int32 ModeOf(const ACharacter* Character)
	{
		return static_cast<int32>(Character->GetCharacterMovement()->MovementMode.GetValue());
	}

	static AGrappleArrowProjectile* FindArrowInFlight(UWorld* World)
	{
		for (TActorIterator<AGrappleArrowProjectile> It(World); It; ++It)
		{
			if (!It->HasArrived())
			{
				return *It;
			}
		}
		return nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleGrappleSelection, "Castle.Grapple.Selection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleGrappleSelection::RunTest(const FString& Parameters)
{
	using namespace CastleGrappleTest;
	const FCastleTestWorld TestWorld;
	ACastleAimTestCharacter* Kate = SpawnKate(TestWorld, FVector::ZeroVector);
	if (!TestNotNull(TEXT("Kate spawned"), Kate)
		|| !TestNotNull(TEXT("She has a grapple"), Kate->GetGrappleComponent()))
	{
		return false;
	}
	UGrappleComponent* Grapple = Kate->GetGrappleComponent();

	// Markers sit 20 cm above each anchor, so anchors at z = 0 are level with a view at z = 20.
	const FVector View(0.f, 0.f, 20.f);
	const FVector Forward = FVector::ForwardVector;
	const float Tan29 = FMath::Tan(FMath::DegreesToRadians(29.f));
	const float Tan31 = FMath::Tan(FMath::DegreesToRadians(31.f));

	AGrappleAnchor* AtFeet = SpawnAnchor(TestWorld, FVector(200.f, 0.f, 0.f));           // 0 deg, 200 cm
	AGrappleAnchor* Ahead = SpawnAnchor(TestWorld, FVector(2000.f, 0.f, 0.f));           // 0 deg, 2000 cm
	AGrappleAnchor* NearEdge = SpawnAnchor(TestWorld, FVector(2450.f, 300.f, 0.f));      // 7 deg, 2468 cm
	AGrappleAnchor* Side = SpawnAnchor(TestWorld, FVector(2000.f, 700.f, 0.f));          // 19 deg
	AGrappleAnchor* TooFar = SpawnAnchor(TestWorld, FVector(2600.f, 100.f, 0.f));        // 2 deg, 2602 cm
	AGrappleAnchor* Wide = SpawnAnchor(TestWorld, FVector(1000.f, 1000.f, 0.f));         // 45 deg
	AGrappleAnchor* Cone29 = SpawnAnchor(TestWorld, FVector(1500.f, -1500.f * Tan29, 0.f));
	AGrappleAnchor* Cone31 = SpawnAnchor(TestWorld, FVector(1500.f, -1500.f * Tan31, 0.f));
	if (!AtFeet || !Ahead || !NearEdge || !Side || !TooFar || !Wide || !Cone29 || !Cone31)
	{
		AddError(TEXT("Could not spawn the anchors."));
		return false;
	}

	TestEqual(TEXT("Range is 2500"), Grapple->Range, 2500.f);
	TestEqual(TEXT("Cone is 30 degrees"), Grapple->ConeDegrees, 30.f);
	TestTrue(TEXT("The smallest angle in range wins; one under 300 cm away does not count"),
		Grapple->SelectBestAnchor(View, Forward) == Ahead);

	Ahead->bEnabled = false;
	TestTrue(TEXT("A disabled anchor is skipped; next smallest angle, just inside 2500 cm"),
		Grapple->SelectBestAnchor(View, Forward) == NearEdge);

	NearEdge->bEnabled = false;
	TestTrue(TEXT("Out of range at 2 degrees loses to in range at 19"),
		Grapple->SelectBestAnchor(View, Forward) == Side);

	Side->bEnabled = false;
	TestTrue(TEXT("29 degrees is inside the cone"), Grapple->SelectBestAnchor(View, Forward) == Cone29);

	Cone29->bEnabled = false;
	TestNull(TEXT("31 and 45 degrees are outside it, 2600 cm is out of range"),
		Grapple->SelectBestAnchor(View, Forward));

	// Line of sight: a wall between the camera and the side anchor, clear of the straight-ahead line.
	Side->bEnabled = true;
	SpawnBlocker(TestWorld, FVector(1000.f, 350.f, 20.f), FVector(20.f, 100.f, 100.f));
	TestNull(TEXT("A wall in front of the only candidate leaves no target"), Grapple->SelectBestAnchor(View, Forward));

	Ahead->bEnabled = true;
	TestTrue(TEXT("An anchor the wall does not cover is still found"),
		Grapple->SelectBestAnchor(View, Forward) == Ahead);

	Grapple->UpdateTarget(View, Forward);
	TestTrue(TEXT("UpdateTarget stores the pick"), Grapple->GetTargetAnchor() == Ahead);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleGrappleZipKinematics, "Castle.Grapple.ZipKinematics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleGrappleZipKinematics::RunTest(const FString& Parameters)
{
	using namespace CastleGrappleTest;
	const FCastleTestWorld TestWorld;
	const FVector Start(0.f, 0.f, 200.f);
	ACastleAimTestCharacter* Kate = SpawnKate(TestWorld, Start);
	AGrappleAnchor* Anchor = SpawnAnchor(TestWorld, FVector(1800.f, 0.f, 500.f));
	if (!Kate || !Anchor)
	{
		AddError(TEXT("Could not spawn Kate and an anchor."));
		return false;
	}
	UGrappleComponent* Grapple = Kate->GetGrappleComponent();
	UCharacterMovementComponent* Movement = Kate->GetCharacterMovement();
	UCastleTestListener* Listener = NewObject<UCastleTestListener>();
	Grapple->OnGrappleLanded.AddDynamic(Listener, &UCastleTestListener::HandleGrappleLanded);
	const float HealthBefore = Kate->GetHealthComponent()->GetCurrentHealth();

	TestEqual(TEXT("Zip speed is 1800"), Grapple->ZipSpeed, 1800.f);
	const FVector End = Grapple->ComputeZipEnd(Anchor);
	TestTrue(TEXT("The zip ends with her feet on the landing point, 60 cm inboard of the anchor"),
		End.Equals(FVector(1860.f, 0.f, 500.f + Kate->GetSimpleCollisionHalfHeight() + 2.f), 0.5f));

	TestTrue(TEXT("StartZip accepts a valid anchor"), Grapple->StartZip(Anchor));
	TestTrue(TEXT("Zipping"), Grapple->IsZipping());
	TestEqual(TEXT("Flying while zipping"), ModeOf(Kate), static_cast<int32>(MOVE_Flying));
	TestEqual(TEXT("No gravity while zipping"), Movement->GravityScale, 0.f);

	for (int32 Step = 0; Step < 5; ++Step)
	{
		Grapple->AdvanceZip(0.1f);
	}
	const FVector Direction = (End - Start).GetSafeNormal();
	const float Length = FVector::Dist(Start, End);
	TestTrue(TEXT("After 0.5 s she is 900 cm along the straight line"),
		Kate->GetActorLocation().Equals(Start + Direction * 900.f, 0.5f));
	TestEqual(TEXT("Progress is the distance over the line length"), Grapple->GetZipProgress(), 900.f / Length, 0.001f);
	TestEqual(TEXT("Still Flying mid-zip"), ModeOf(Kate), static_cast<int32>(MOVE_Flying));

	const int32 Steps = RunZip(Grapple, 0.1f, 5.f);
	TestEqual(TEXT("The whole line takes ceil(length / 1800 / 0.1) steps"), Steps + 5,
		FMath::CeilToInt(Length / 1800.f / 0.1f));
	TestFalse(TEXT("The zip is over"), Grapple->IsZipping());
	TestTrue(TEXT("She is exactly on the end point"), Kate->GetActorLocation().Equals(End, 0.5f));
	TestEqual(TEXT("Landing sets Walking"), ModeOf(Kate), static_cast<int32>(MOVE_Walking));
	TestEqual(TEXT("Gravity is back"), Movement->GravityScale, 1.f);
	TestEqual(TEXT("OnGrappleLanded fired once"), Listener->GrappleLandedCount, 1);
	TestTrue(TEXT("For this anchor"), Listener->LastGrappleAnchor == Anchor);
	TestFalse(TEXT("A zip landing is not a roll: no camera dip, no speed cut"), Kate->IsRecoveringFromLanding());
	TestEqual(TEXT("A zip landing is a 0 cm landing"), Kate->GetLastFallHeight(), 0.f);
	TestEqual(TEXT("And costs no health"), Kate->GetHealthComponent()->GetCurrentHealth(), HealthBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleGrappleIgnoresAnchorBuilding, "Castle.Grapple.ZipIgnoresAnchorBuilding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleGrappleIgnoresAnchorBuilding::RunTest(const FString& Parameters)
{
	using namespace CastleGrappleTest;
	const FCastleTestWorld TestWorld;
	ACastleAimTestCharacter* Kate = SpawnKate(TestWorld, FVector(0.f, 30.f, 0.f));
	// Two adjoining buildings with roofs at z = 200, sharing a wall along y = 0; the anchor stands
	// on the first one's front corner, right by the second. The line up from the street clips both
	// roof edges on the way in, which must not count as blocked.
	ACastleTestBlocker* Building = SpawnBlocker(TestWorld, FVector(2200.f, 300.f, 100.f), FVector(200.f, 300.f, 100.f));
	ACastleTestBlocker* Neighbour = SpawnBlocker(TestWorld, FVector(2200.f, -300.f, 100.f), FVector(200.f, 300.f, 100.f));
	AGrappleAnchor* Anchor = SpawnAnchor(TestWorld, FVector(2010.f, 30.f, 200.f));
	if (!Kate || !Building || !Neighbour || !Anchor)
	{
		AddError(TEXT("Could not spawn the scene."));
		return false;
	}
	UGrappleComponent* Grapple = Kate->GetGrappleComponent();

	Grapple->StartZip(Anchor);
	RunZip(Grapple, 1.f / 60.f, 5.f);
	TestTrue(TEXT("She arrives on the roof"), Kate->GetActorLocation().Equals(Grapple->ComputeZipEnd(Anchor), 0.5f));
	TestEqual(TEXT("Walking"), ModeOf(Kate), static_cast<int32>(MOVE_Walking));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleGrappleChain, "Castle.Grapple.ChainRedirectsMidAirFrom40Percent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleGrappleChain::RunTest(const FString& Parameters)
{
	using namespace CastleGrappleTest;
	const FCastleTestWorld TestWorld;
	ACastleAimTestCharacter* Kate = SpawnKate(TestWorld, FVector(0.f, 0.f, 200.f));
	AGrappleAnchor* First = SpawnAnchor(TestWorld, FVector(1800.f, 0.f, 500.f));
	AGrappleAnchor* Second = SpawnAnchor(TestWorld, FVector(1800.f, 1500.f, 500.f));
	if (!Kate || !First || !Second)
	{
		AddError(TEXT("Could not spawn the scene."));
		return false;
	}
	UGrappleComponent* Grapple = Kate->GetGrappleComponent();
	TestEqual(TEXT("The chain window opens at 40%"), Grapple->ChainMinProgress, 0.4f);
	TestEqual(TEXT("A chain arrow arrives at once"), Grapple->ChainArrowSpeed, 0.f);
	TestEqual(TEXT("The turn onto the new line takes 0.1 s"), Grapple->RedirectBlendSeconds, 0.1f);

	Grapple->StartZip(First);
	const float Length = Grapple->GetZipLength();
	const FVector FirstEnd = Grapple->ComputeZipEnd(First);
	auto AimAtSecond = [Grapple, Kate, Second]()
	{
		const FVector View = Kate->GetActorLocation();
		Grapple->UpdateTarget(View, (Second->GetMarkerLocation() - View).GetSafeNormal());
	};

	Grapple->AdvanceZip(0.35f * Length / Grapple->ZipSpeed);
	AimAtSecond();
	TestTrue(TEXT("The next anchor is targeted mid-zip"), Grapple->GetTargetAnchor() == Second);
	TestFalse(TEXT("At 35% a second arrow is refused"), Grapple->TryFire());
	TestEqual(TEXT("And costs nothing"), Grapple->GetGrappleArrows(), 99);
	TestTrue(TEXT("Still zipping to the first anchor"), Grapple->GetZipAnchor() == First);

	Grapple->AdvanceZip(0.15f * Length / Grapple->ZipSpeed);
	AimAtSecond();
	const FVector OldDirection = Grapple->GetZipDirection();
	const FVector AtPress = Kate->GetActorLocation();
	TestTrue(TEXT("At 50% the chain fires"), Grapple->TryFire());
	TestEqual(TEXT("One arrow spent"), Grapple->GetGrappleArrows(), 98);
	TestFalse(TEXT("The chain arrow is already there"), Grapple->IsArrowInFlight());
	TestEqual(TEXT("It stays in the new anchor"), Second->GetStuckArrowCount(), 1);
	TestTrue(TEXT("The zip target changed before landing"), Grapple->IsZipping() && Grapple->GetZipAnchor() == Second);
	TestTrue(TEXT("She is still in the air, nowhere near the first roof"),
		FVector::Dist(Kate->GetActorLocation(), FirstEnd) > 0.4f * Length);
	TestEqual(TEXT("Flying, not landed"), ModeOf(Kate), static_cast<int32>(MOVE_Flying));
	TestEqual(TEXT("From the start of its new line"), Grapple->GetZipProgress(), 0.f);
	TestTrue(TEXT("From where she was, with no hop"), Grapple->GetZipLaunch().Equals(AtPress, 0.5f));
	TestTrue(TEXT("Turning onto the new line"), Grapple->IsRedirecting());
	TestEqual(TEXT("Counted as a mid-air redirect"), Grapple->GetRedirectCount(), 1);
	TestFalse(TEXT("A new line starts the 40% rule over"), Grapple->TryFire());

	// Half the blend: the direction is between the old line and the new one, not snapped.
	const FVector NewDirection = (Grapple->ComputeZipEnd(Second) - AtPress).GetSafeNormal();
	Grapple->AdvanceZip(0.05f);
	const FVector Mid = Grapple->GetZipDirection();
	const float ToOld = FVector::DotProduct(Mid, OldDirection);
	const float ToNew = FVector::DotProduct(Mid, NewDirection);
	TestTrue(TEXT("Half way through the blend it has turned part of the way"),
		ToNew > FVector::DotProduct(OldDirection, NewDirection) + 0.05f && ToNew < 0.999f && ToOld < 0.999f);
	Grapple->AdvanceZip(0.06f);
	TestFalse(TEXT("After 0.1 s the blend is done"), Grapple->IsRedirecting());
	TestTrue(TEXT("And she heads straight for the new landing point"),
		FVector::DotProduct(Grapple->GetZipDirection(),
			(Grapple->ComputeZipEnd(Second) - Kate->GetActorLocation()).GetSafeNormal()) > 0.999f);

	RunZip(Grapple, 0.05f, 5.f);
	TestTrue(TEXT("She lands on the second anchor's roof"),
		Kate->GetActorLocation().Equals(Grapple->ComputeZipEnd(Second), 0.5f));
	TestEqual(TEXT("Walking"), ModeOf(Kate), static_cast<int32>(MOVE_Walking));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleGrappleArrowCount, "Castle.Grapple.ArrowCountAndRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleGrappleArrowCount::RunTest(const FString& Parameters)
{
	using namespace CastleGrappleTest;
	const FCastleTestWorld TestWorld;
	ACastleAimTestCharacter* Kate = SpawnKate(TestWorld, FVector(0.f, 0.f, 200.f));
	AGrappleAnchor* Anchor = SpawnAnchor(TestWorld, FVector(1500.f, 0.f, 200.f));
	AGrappleAnchor* Beyond = SpawnAnchor(TestWorld, FVector(3000.f, 0.f, 200.f));
	if (!Kate || !Anchor || !Beyond)
	{
		AddError(TEXT("Could not spawn the scene."));
		return false;
	}
	UGrappleComponent* Grapple = Kate->GetGrappleComponent();
	const FVector Forward = FVector::ForwardVector;

	TestFalse(TEXT("No target, no shot"), Grapple->TryFire());
	TestEqual(TEXT("A refused shot costs nothing"), Grapple->GetGrappleArrows(), 99);

	Grapple->SetGrappleArrows(1);
	Grapple->UpdateTarget(Kate->GetActorLocation() + FVector(0.f, 0.f, 20.f), Forward);
	TestTrue(TEXT("The near anchor is the target"), Grapple->GetTargetAnchor() == Anchor);
	TestTrue(TEXT("The last arrow fires"), Grapple->TryFire());
	TestEqual(TEXT("It is spent"), Grapple->GetGrappleArrows(), 0);
	TestEqual(TEXT("The hint counts the use"), Grapple->GetUseCount(), 1);
	TestFalse(TEXT("A second press while it flies is refused"), Grapple->TryFire());

	AGrappleArrowProjectile* Arrow = FindArrowInFlight(TestWorld.Get());
	if (!TestNotNull(TEXT("A projectile was spawned"), Arrow))
	{
		return false;
	}
	Arrow->Advance(1.f);
	RunZip(Grapple, 0.05f, 5.f);
	TestFalse(TEXT("Landed"), Grapple->IsZipping());

	Grapple->UpdateTarget(Kate->GetActorLocation(), Forward);
	TestTrue(TEXT("The next anchor along is targeted"), Grapple->GetTargetAnchor() == Beyond);
	TestFalse(TEXT("With no arrows left the shot is refused"), Grapple->TryFire());
	TestEqual(TEXT("Still none"), Grapple->GetGrappleArrows(), 0);
	TestNull(TEXT("And nothing was fired"), FindArrowInFlight(TestWorld.Get()));

	TestEqual(TEXT("Standing by the anchor pulls the arrow back"), Grapple->RecoverNearbyArrows(), 1);
	TestEqual(TEXT("Back in the quiver"), Grapple->GetGrappleArrows(), 1);
	TestEqual(TEXT("Gone from the anchor"), Anchor->GetStuckArrowCount(), 0);
	TestTrue(TEXT("The stuck projectile is removed"), !IsValid(Arrow) || Arrow->IsActorBeingDestroyed());
	TestEqual(TEXT("Nothing more to recover"), Grapple->RecoverNearbyArrows(), 0);
	TestTrue(TEXT("And the recovered arrow can be fired"), Grapple->TryFire());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleGrappleQSpendsFromQuiver, "Castle.Grapple.QSpendsFromQuiver",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleGrappleQSpendsFromQuiver::RunTest(const FString& Parameters)
{
	using namespace CastleGrappleTest;
	const FCastleTestWorld TestWorld;
	ACastleAimTestCharacter* Kate = SpawnKate(TestWorld, FVector(0.f, 0.f, 200.f), 0);
	AGrappleAnchor* Anchor = SpawnAnchor(TestWorld, FVector(1500.f, 0.f, 200.f));
	UInventoryComponent* Inventory = Kate ? Kate->GetInventoryComponent() : nullptr;
	if (!Inventory || !Anchor)
	{
		AddError(TEXT("Could not spawn the scene."));
		return false;
	}
	UGrappleComponent* Grapple = Kate->GetGrappleComponent();
	Inventory->SetArrowCount(2, 6);
	Inventory->SetArrowCount(1, 30);
	TestEqual(TEXT("Standard arrows are nocked"), Inventory->GetActiveArrowSlot(), 1);

	Grapple->UpdateTarget(Kate->GetActorLocation() + FVector(0.f, 0.f, 20.f), FVector::ForwardVector);
	TestTrue(TEXT("Q fires at the marked anchor whatever is nocked"), Grapple->TryFire());
	TestEqual(TEXT("One grapple arrow spent from slot 2"), Inventory->GetArrowCount(2), 5);
	TestEqual(TEXT("The grapple reads the same count"), Grapple->GetGrappleArrows(), 5);
	TestEqual(TEXT("Standard arrows untouched"), Inventory->GetArrowCount(1), 30);
	TestEqual(TEXT("And still nocked"), Inventory->GetActiveArrowSlot(), 1);
	AGrappleArrowProjectile* Arrow = FindArrowInFlight(TestWorld.Get());
	TestNotNull(TEXT("A grapple projectile left the bow"), Arrow);
	TestTrue(TEXT("Carrying the quiver's grapple definition"),
		Arrow && Arrow->GetArrowDefinition() == Inventory->GetArrowSlot(2).Arrow);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleGrappleBlocked, "Castle.Grapple.BlockedZipDrops",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleGrappleBlocked::RunTest(const FString& Parameters)
{
	using namespace CastleGrappleTest;
	const FCastleTestWorld TestWorld;
	ACastleAimTestCharacter* Kate = SpawnKate(TestWorld, FVector(0.f, 0.f, 200.f));
	AGrappleAnchor* Anchor = SpawnAnchor(TestWorld, FVector(2000.f, 0.f, 200.f));
	ACastleTestBlocker* Wall = SpawnBlocker(TestWorld, FVector(1000.f, 0.f, 250.f), FVector(20.f, 200.f, 300.f));
	if (!Kate || !Anchor || !Wall)
	{
		AddError(TEXT("Could not spawn the scene."));
		return false;
	}
	UGrappleComponent* Grapple = Kate->GetGrappleComponent();
	UCastleTestListener* Listener = NewObject<UCastleTestListener>();
	Grapple->OnGrappleLanded.AddDynamic(Listener, &UCastleTestListener::HandleGrappleLanded);
	Grapple->OnGrappleCancelled.AddDynamic(Listener, &UCastleTestListener::HandleGrappleCancelled);

	Grapple->StartZip(Anchor);
	RunZip(Grapple, 0.05f, 5.f);
	TestFalse(TEXT("The zip stopped"), Grapple->IsZipping());
	TestTrue(TEXT("At the wall, not through it"), Kate->GetActorLocation().X < 1000.f);
	TestEqual(TEXT("And she falls"), ModeOf(Kate), static_cast<int32>(MOVE_Falling));
	TestEqual(TEXT("Gravity is back"), Kate->GetCharacterMovement()->GravityScale, 1.f);
	TestEqual(TEXT("OnGrappleCancelled fired"), Listener->GrappleCancelledCount, 1);
	TestEqual(TEXT("OnGrappleLanded did not"), Listener->GrappleLandedCount, 0);
	return true;
}

#endif

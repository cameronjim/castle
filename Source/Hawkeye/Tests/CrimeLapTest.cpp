// Copyright Epic Games, Inc. All Rights Reserved.

#include "Camera/CameraComponent.h"
#include "CollisionQueryParams.h"
#include "Combat/ArrowDefinition.h"
#include "Combat/BowDefinition.h"
#include "Combat/FinisherComponent.h"
#include "Combat/HealthComponent.h"
#include "Combat/MeleeComponent.h"
#include "Combat/MeleeRules.h"
#include "Components/CapsuleComponent.h"
#include "Crime/Civilian.h"
#include "Crime/CrimeDefinition.h"
#include "Crime/CrimeSpot.h"
#include "Crime/CrimeSubsystem.h"
#include "Crime/CrimeTracker.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformFileManager.h"
#include "HawkeyePlayerController.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Player/HawkeyeCharacter.h"
#include "Player/InventoryComponent.h"
#include "Save/HawkeyeCampaignState.h"
#include "Save/HawkeyeSaveSubsystem.h"
#include "Tests/AutomationCommon.h"
#include "Tests/HawkeyeShots.h"
#include "Tests/PartnerScreenshots.h"
#include "World/ThugAIController.h"
#include "World/ThugCharacter.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * A street crime played by a script on the district, in the standalone game like the other laps:
 *
 *   UnrealEditor-Cmd.exe Hawkeye.uproject -game -windowed -ResX=1280 -ResY=720 -unattended
 *       -nosplash -log -ExecCmds="Automation RunTests Hawkeye.Lap.CrimeMugging; Quit"
 *
 * Hawkeye.Lap.CrimeMugging: the crime spot nearest the PlayerStart; Kate put on the street 30 m from it
 * with 20 standard arrows. hawkeye.CrimeInterval 1 and hawkeye.CrimeType mugging make the next crime a
 * mugging within a second, started by the subsystem's own rules (so it must pick that spot, the only one
 * 25 to 40 m out). She runs in, looses arrows from 5 to 15 m, then fights with strikes (parries, finishers
 * and dodges as a player who has learnt them, like the other melee laps). The district's placed thugs
 * stop thinking; the crime's do not. Must complete: both thugs down with the victim alive, 5 arrows
 * added, the completion counted and in the campaign state. Reports the time, hits taken and the
 * victim's hits and writes Saved/Automation/crime_mugging.json.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHawkeyeLapCrimeMugging, "Hawkeye.Lap.CrimeMugging",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

namespace HawkeyeCrimeLap
{
	static const TCHAR* MovePath = TEXT("/Game/Input/IA_Move.IA_Move");
	static const TCHAR* SprintPath = TEXT("/Game/Input/IA_Sprint.IA_Sprint");
	static const TCHAR* FirePath = TEXT("/Game/Input/IA_Fire.IA_Fire");
	static const TCHAR* MeleePath = TEXT("/Game/Input/IA_Melee.IA_Melee");
	static const TCHAR* TakedownPath = TEXT("/Game/Input/IA_Takedown.IA_Takedown");
	static const TCHAR* BowAssetPath = TEXT("/Game/Blueprints/Weapons/DA_Bow_Kate.DA_Bow_Kate");
	static const TCHAR* StandardAssetPath = TEXT("/Game/Blueprints/Weapons/DA_Arrow_Standard.DA_Arrow_Standard");
	static const TCHAR* GrappleAssetPath = TEXT("/Game/Blueprints/Weapons/DA_Arrow_Grapple.DA_Arrow_Grapple");

	/** Where she is put, from the spot, and how many arrows she starts with. */
	static constexpr float StartDistance = 3000.f;
	static constexpr int32 StartArrows = 20;

	static UWorld* FindWorld()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.World() && (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE))
			{
				return Context.World();
			}
		}
		return nullptr;
	}

	static UEnhancedInputLocalPlayerSubsystem* InputOf(APlayerController* PC)
	{
		ULocalPlayer* Player = PC ? PC->GetLocalPlayer() : nullptr;
		return Player ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Player) : nullptr;
	}

	static void Tap(APlayerController* PC, const TCHAR* Path)
	{
		UEnhancedInputLocalPlayerSubsystem* Input = InputOf(PC);
		const UInputAction* A = LoadObject<UInputAction>(nullptr, Path);
		if (Input && A)
		{
			Input->InjectInputForAction(A, FInputActionValue(true), {}, {});
		}
	}

	static void Hold(APlayerController* PC, const TCHAR* Path, bool bHold)
	{
		UEnhancedInputLocalPlayerSubsystem* Input = InputOf(PC);
		const UInputAction* A = LoadObject<UInputAction>(nullptr, Path);
		if (!Input || !A)
		{
			return;
		}
		if (bHold)
		{
			Input->StartContinuousInputInjectionForAction(A, FInputActionValue(true), {}, {});
		}
		else
		{
			Input->StopContinuousInputInjectionForAction(A);
		}
	}

	static FVector2D MoveTowards(const FVector& WorldDirection, float CameraYaw)
	{
		const float Angle = FMath::DegreesToRadians(WorldDirection.Rotation().Yaw - CameraYaw);
		return FVector2D(FMath::Sin(Angle), FMath::Cos(Angle));
	}

	static void AimAt(APlayerController* PC, const AHawkeyeCharacter* Kate, const FVector& Target)
	{
		const FVector Lens = Kate->GetFollowCamera()->GetComponentLocation();
		const FRotator Look = (Target - Lens).Rotation();
		PC->SetControlRotation(FRotator(Kate->ClampCameraPitch(Look.Pitch), Look.Yaw, 0.f));
	}

	/** A crime thug in front of her within 240 cm whose telegraph she has seen for 0.2 s. */
	static AThugCharacter* ParryChance(const TArray<TWeakObjectPtr<AThugCharacter>>& Thugs, const AHawkeyeCharacter* Kate,
		const APlayerController* PC)
	{
		const FVector Forward = FRotator(0.f, PC->GetControlRotation().Yaw, 0.f).Vector();
		for (const TWeakObjectPtr<AThugCharacter>& Weak : Thugs)
		{
			AThugCharacter* Thug = Weak.Get();
			const EHawkeyeParryKind Kind = Thug ? UHawkeyeMeleeRules::ClassifyParry(Thug) : EHawkeyeParryKind::None;
			if (Kind == EHawkeyeParryKind::None
				|| !UHawkeyeMeleeRules::IsInFrontWithin(Kate->GetActorLocation(), Forward, Thug->GetActorLocation(), 240.f, 65.f))
			{
				continue;
			}
			const UMeleeComponent* Melee = Thug->GetMeleeComponent();
			if (Kind == EHawkeyeParryKind::Burst || (Melee && Melee->GetCurrentAttack().WindupSeconds - Melee->GetPhaseRemaining() >= 0.2f))
			{
				return Thug;
			}
		}
		return nullptr;
	}

	static bool CanParryNow(const AHawkeyeCharacter* Kate)
	{
		return Kate->GetMeleeComponent() && !Kate->GetMeleeComponent()->IsWindingUp() && !Kate->IsDodging()
			&& !Kate->IsStaggered() && !Kate->IsDrawingBow() && !Kate->IsLockedOutByTakedown();
	}

	/** The way to walk from Kate toward Goal: along the navmesh path when there is one, else straight. */
	static FVector SteerTowards(UWorld* World, const AHawkeyeCharacter* Kate, const FVector& Goal)
	{
		const FVector Here = Kate->GetActorLocation();
		UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		const UNavigationPath* Path = Nav ? Nav->FindPathToLocationSynchronously(World, Here, Goal, const_cast<AHawkeyeCharacter*>(Kate)) : nullptr;
		if (Path && Path->IsValid())
		{
			for (const FVector& Point : Path->PathPoints)
			{
				if (FVector::Dist2D(Point, Here) > 150.f)
				{
					return (Point - Here).GetSafeNormal2D();
				}
			}
		}
		return (Goal - Here).GetSafeNormal2D();
	}

	static bool IsDown(const AThugCharacter* Thug)
	{
		return !Thug || Thug->IsLimp() || Thug->GetHealthComponent()->IsDead();
	}

	static void WriteText(const FString& FileName, const FString& Text)
	{
		const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation") / FileName);
		FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(Path));
		FFileHelper::SaveStringToFile(Text, *Path);
	}

	static void SetCVar(const TCHAR* Name, const FString& Value)
	{
		if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			Var->Set(*Value, ECVF_SetByCode);
		}
	}

	/** The saved count of Type in the campaign state, or -1 without one. */
	static int32 SavedCount(const UObject* WorldContext, FName TypeKey)
	{
		const UHawkeyeSaveSubsystem* Save = UHawkeyeSaveSubsystem::Get(WorldContext);
		const UHawkeyeCampaignState* Campaign = Save ? Save->GetCampaignState() : nullptr;
		if (!Campaign)
		{
			return -1;
		}
		const int32 Row = Campaign->CrimeTypes.IndexOfByKey(TypeKey);
		return Row != INDEX_NONE && Campaign->CrimeCompletions.IsValidIndex(Row) ? Campaign->CrimeCompletions[Row] : 0;
	}
}

class FHawkeyeCrimeMuggingRunner : public IAutomationLatentCommand
{
public:
	explicit FHawkeyeCrimeMuggingRunner(FAutomationTestBase* InTest) : Test(InTest) {}

	virtual bool Update() override;

private:
	bool SetUp(UWorld* World, APlayerController* PC, AHawkeyeCharacter* Kate);
	bool PlaceKate(UWorld* World, APlayerController* PC, AHawkeyeCharacter* Kate, const FVector& Spot);
	void Fight(UWorld* World, APlayerController* PC, AHawkeyeCharacter* Kate, double Now);
	void Finish(UWorld* World, APlayerController* PC, AHawkeyeCharacter* Kate, const FString& Why);
	void Move(APlayerController* PC, const FVector2D& Value);

	FAutomationTestBase* Test;
	bool bSetUp = false;
	bool bStarted = false;
	bool bDone = false;
	double SetUpAt = 0.0;
	double StartedAt = 0.0;
	double DoneAt = 0.0;
	double NextActionAt = 0.0;
	double DrawStart = 0.0;
	double MeleeHeldSince = 0.0;
	double LastParryTap = -10.0;
	double NextReportAt = 0.0;
	bool bDrawing = false;
	bool bMeleeHeld = false;
	bool bMoveInjecting = false;
	bool bSprinting = false;
	float LastHealth = 0.f;
	int32 HitsTaken = 0;
	float DamageTaken = 0.f;
	int32 ArrowsLoosed = 0;
	int32 Strikes = 0;
	int32 Parries = 0;
	int32 Finishers = 0;
	int32 Dodges = 0;
	int32 ArrowsBeforeEnd = 0;
	int32 CountBefore = 0;
	int32 SavedBefore = 0;
	FString OldInterval;
	FString OldType;
	TWeakObjectPtr<ACrimeSpot> Spot;
	TWeakObjectPtr<ACivilian> Victim;
	TArray<TWeakObjectPtr<AThugCharacter>> Thugs;
};

void FHawkeyeCrimeMuggingRunner::Move(APlayerController* PC, const FVector2D& Value)
{
	UEnhancedInputLocalPlayerSubsystem* Input = HawkeyeCrimeLap::InputOf(PC);
	const UInputAction* A = LoadObject<UInputAction>(nullptr, HawkeyeCrimeLap::MovePath);
	if (!Input || !A)
	{
		return;
	}
	if (Value.IsNearlyZero())
	{
		if (bMoveInjecting)
		{
			Input->StopContinuousInputInjectionForAction(A);
			bMoveInjecting = false;
		}
		return;
	}
	if (!bMoveInjecting)
	{
		Input->StartContinuousInputInjectionForAction(A, FInputActionValue(Value), {}, {});
		bMoveInjecting = true;
		return;
	}
	Input->UpdateValueOfContinuousInputInjectionForAction(A, FInputActionValue(Value));
}

bool FHawkeyeCrimeMuggingRunner::PlaceKate(UWorld* World, APlayerController* PC, AHawkeyeCharacter* Kate, const FVector& At)
{
	// Open street 30 m out with a walk in not much longer than the straight line, toward the PlayerStart
	// first and then round the spot; failing that any street 30 m out with a path in at all.
	FVector Toward = FVector(1.f, 0.f, 0.f);
	if (TActorIterator<APlayerStart> Start(World); Start)
	{
		Toward = (Start->GetActorLocation() - At).GetSafeNormal2D();
	}
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	const UCrimeSubsystem* Crimes = UCrimeSubsystem::Get(World);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CrimeLapGround), false, Kate);
	for (const float Detour : { 1.3f, 3.f })
	{
		for (int32 Step = 0; Step < 24; ++Step)
		{
			const float Turn = (Step % 2 == 0 ? 1.f : -1.f) * 15.f * ((Step + 1) / 2);
			const FVector Out = Toward.RotateAngleAxis(Turn, FVector::UpVector);
			const FVector Probe = At + Out * HawkeyeCrimeLap::StartDistance;
			// From high above, so a probe inside a building's footprint finds its roof, not the slab under it.
			FHitResult Hit;
			if (!World->LineTraceSingleByChannel(Hit, Probe + FVector(0.f, 0.f, 20000.f), Probe - FVector(0.f, 0.f, 500.f), ECC_Visibility, Params)
				|| Hit.ImpactPoint.Z > At.Z + 60.f || (Hit.GetActor() && Hit.GetActor()->ActorHasTag(TEXT("CityBuilding"))))
			{
				continue;
			}
			const UNavigationPath* Path = Nav ? Nav->FindPathToLocationSynchronously(World, Hit.ImpactPoint + FVector(0.f, 0.f, 50.f), At, Kate) : nullptr;
			if (!Path || !Path->IsValid() || Path->IsPartial() || Path->GetPathLength() > HawkeyeCrimeLap::StartDistance * Detour)
			{
				continue;
			}
			// Only this spot may be 25 to 40 m out, so the rules can only pick it.
			const bool bOthersInBand = Crimes && Crimes->GetSpots().ContainsByPredicate([&](const ACrimeSpot* Other)
			{
				const float D = FVector::Dist2D(Other->GetActorLocation(), Hit.ImpactPoint);
				return Other != Spot.Get() && Other->FindCrime(ECrimeType::Mugging) && D >= Crimes->SpawnMinDistance - 100.f
					&& D <= Crimes->SpawnMaxDistance + 100.f;
			});
			if (bOthersInBand)
			{
				continue;
			}
			const float HalfHeight = Kate->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			const float Yaw = (-Out).Rotation().Yaw;
			Kate->TeleportTo(Hit.ImpactPoint + FVector(0.f, 0.f, HalfHeight + 2.f), FRotator(0.f, Yaw, 0.f));
			PC->SetControlRotation(FRotator(-8.f, Yaw, 0.f));
			PC->SetViewTarget(Kate);
			Test->AddInfo(FString::Printf(TEXT("Crime lap: Kate at %s, %.0f m from %s, %.0f m to walk."), *Hit.ImpactPoint.ToCompactString(),
				FVector::Dist2D(Hit.ImpactPoint, At) / 100.f, *GetNameSafe(Spot.Get()), Path->GetPathLength() / 100.f));
			return true;
		}
	}
	return false;
}

bool FHawkeyeCrimeMuggingRunner::SetUp(UWorld* World, APlayerController* PC, AHawkeyeCharacter* Kate)
{
	using namespace HawkeyeCrimeLap;
	UCrimeSubsystem* Crimes = UCrimeSubsystem::Get(World);
	FVector StartLocation = Kate->GetActorLocation();
	if (TActorIterator<APlayerStart> Start(World); Start)
	{
		StartLocation = Start->GetActorLocation();
	}
	ACrimeSpot* Nearest = nullptr;
	for (ACrimeSpot* Candidate : Crimes ? Crimes->GetSpots() : TArray<ACrimeSpot*>())
	{
		if (!Candidate->bRooftop && Candidate->FindCrime(ECrimeType::Mugging)
			&& (!Nearest || FVector::Dist2D(Candidate->GetActorLocation(), StartLocation) < FVector::Dist2D(Nearest->GetActorLocation(), StartLocation)))
		{
			Nearest = Candidate;
		}
	}
	if (!Nearest)
	{
		Test->AddError(TEXT("Crime lap: no street crime spot with a mugging on the district."));
		return false;
	}
	Spot = Nearest;
	HawkeyeFreezePartner(World);
	for (TActorIterator<AThugCharacter> It(World); It; ++It)
	{
		if (AThugAIController* Brain = Cast<AThugAIController>(It->GetController()))
		{
			Brain->SetThinkingEnabled(false);
		}
	}
	if (!PlaceKate(World, PC, Kate, Nearest->GetActorLocation()))
	{
		Test->AddError(TEXT("Crime lap: no open street 30 m from the spot to put Kate on."));
		return false;
	}
	UInventoryComponent* Inventory = Kate->GetInventoryComponent();
	if (!Inventory->HasBow())
	{
		FHawkeyeQuiverSlot Standard;
		Standard.Arrow = LoadObject<UArrowDefinition>(nullptr, StandardAssetPath);
		Standard.Count = StartArrows;
		FHawkeyeQuiverSlot Grapple;
		Grapple.Arrow = LoadObject<UArrowDefinition>(nullptr, GrappleAssetPath);
		Grapple.Count = 6;
		Inventory->ApplyStartingQuiver(LoadObject<UBowDefinition>(nullptr, BowAssetPath), { Standard, Grapple });
	}
	Inventory->SetArrowCount(1, StartArrows);
	Inventory->SelectArrowSlot(1);
	Kate->GetHealthComponent()->SetInvulnerable(false);
	Kate->GetHealthComponent()->Heal(1000.f);
	LastHealth = Kate->GetHealthComponent()->GetCurrentHealth();
	CountBefore = Crimes->GetCompletions(ECrimeType::Mugging);
	SavedBefore = FMath::Max(SavedCount(World, TEXT("Mugging")), 0);
	IConsoleVariable* Interval = IConsoleManager::Get().FindConsoleVariable(TEXT("hawkeye.CrimeInterval"));
	IConsoleVariable* Type = IConsoleManager::Get().FindConsoleVariable(TEXT("hawkeye.CrimeType"));
	OldInterval = Interval ? Interval->GetString() : TEXT("0");
	OldType = Type ? Type->GetString() : TEXT("");
	SetCVar(TEXT("hawkeye.CrimeInterval"), TEXT("1"));
	SetCVar(TEXT("hawkeye.CrimeType"), TEXT("mugging"));
	return true;
}

void FHawkeyeCrimeMuggingRunner::Finish(UWorld* World, APlayerController* PC, AHawkeyeCharacter* Kate, const FString& Why)
{
	using namespace HawkeyeCrimeLap;
	Move(PC, FVector2D::ZeroVector);
	Hold(PC, FirePath, false);
	Hold(PC, MeleePath, false);
	Hold(PC, SprintPath, false);
	SetCVar(TEXT("hawkeye.CrimeInterval"), OldInterval);
	SetCVar(TEXT("hawkeye.CrimeType"), OldType);
	bDone = true;
	DoneAt = World->GetTimeSeconds();
	UCrimeSubsystem* Crimes = UCrimeSubsystem::Get(World);
	const UCrimeTracker* Tracker = Crimes ? Crimes->GetTracker() : nullptr;
	const bool bHasResult = Tracker && Tracker->HasLastResult() && bStarted;
	const FCrimeResult Result = bHasResult ? Tracker->GetLastResult() : FCrimeResult();
	const UInventoryComponent* Inventory = Kate->GetInventoryComponent();
	const int32 ArrowsAfter = Inventory->GetArrowCount(1);
	const int32 Cap = Inventory->GetCap(Inventory->GetArrowSlot(1).Arrow);
	const int32 WantArrows = FMath::Min(5, FMath::Max(Cap - ArrowsBeforeEnd, 0));
	const int32 CountAfter = Crimes ? Crimes->GetCompletions(ECrimeType::Mugging) : 0;
	const int32 SavedAfter = SavedCount(World, TEXT("Mugging"));
	const ACivilian* Civilian = Victim.Get();
	const bool bWon = bHasResult && Result.IsCompleted() && Result.Type == ECrimeType::Mugging;
	const FString Json = FString::Printf(TEXT(
		"{\n  \"test\": \"Hawkeye.Lap.CrimeMugging\",\n  \"completed\": %s,\n  \"end\": \"%s\",\n  \"reason\": \"%s\",\n"
		"  \"spot\": \"%s\",\n  \"seconds\": %.2f,\n  \"seconds_to_start\": %.2f,\n  \"hits_taken\": %d,\n  \"damage_taken\": %.1f,\n"
		"  \"kate_health\": %.1f,\n  \"victim_hits\": %d,\n  \"victim_freed\": %s,\n  \"thanks_shown\": %s,\n"
		"  \"arrows_loosed\": %d,\n  \"strikes\": %d,\n  \"parries\": %d,\n  \"finishers\": %d,\n  \"dodges\": %d,\n"
		"  \"arrows_before_end\": %d,\n  \"arrows_after\": %d,\n  \"arrows_given\": %d,\n"
		"  \"muggings_before\": %d,\n  \"muggings_after\": %d,\n  \"saved_muggings\": %d\n}\n"),
		bWon ? TEXT("true") : TEXT("false"), *Why, bHasResult ? *UEnum::GetValueAsString(Result.Reason) : TEXT("none"),
		*GetNameSafe(Spot.Get()), bStarted ? DoneAt - StartedAt : 0.0, bStarted ? StartedAt - SetUpAt : 0.0, HitsTaken, DamageTaken,
		Kate->GetHealthComponent()->GetCurrentHealth(), Result.VictimHits, Civilian && Civilian->IsFreed() ? TEXT("true") : TEXT("false"),
		Civilian && Civilian->IsThanksShowing() ? TEXT("true") : TEXT("false"), ArrowsLoosed, Strikes, Parries, Finishers, Dodges,
		ArrowsBeforeEnd, ArrowsAfter, Result.ArrowsGiven, CountBefore, CountAfter, SavedAfter);
	WriteText(TEXT("crime_mugging.json"), Json);
	Test->AddInfo(TEXT("crime_mugging.json:\n") + Json);
	UE_LOG(LogTemp, Display, TEXT("[Hawkeye] crime mugging: %s"), *Json);
	if (!bWon)
	{
		Test->AddError(FString::Printf(TEXT("The mugging was not stopped (%s)."), *Why));
		return;
	}
	Test->TestEqual(TEXT("Stopping it added 5 standard arrows (or up to the cap)"), Result.ArrowsGiven, WantArrows);
	Test->TestEqual(TEXT("The quiver shows them"), ArrowsAfter - ArrowsBeforeEnd, WantArrows);
	Test->TestTrue(TEXT("There was room for them"), WantArrows > 0);
	Test->TestEqual(TEXT("One more mugging counted"), CountAfter, CountBefore + 1);
	Test->TestEqual(TEXT("And in the campaign save"), SavedAfter, SavedBefore + 1);
	Test->TestTrue(TEXT("The victim was freed"), Civilian && Civilian->IsFreed());
	Test->TestTrue(TEXT("With fewer than three hits on him"), Result.VictimHits < 3);
}

bool FHawkeyeCrimeMuggingRunner::Update()
{
	using namespace HawkeyeCrimeLap;
	UWorld* World = FindWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	AHawkeyeCharacter* Kate = PC ? Cast<AHawkeyeCharacter>(PC->GetPawn()) : nullptr;
	UCrimeSubsystem* Crimes = UCrimeSubsystem::Get(World);
	if (!World || !Kate || !Crimes)
	{
		Test->AddError(TEXT("No game world, no Kate, or no crime subsystem for the crime lap."));
		return true;
	}
	const double Now = World->GetTimeSeconds();
	if (bDone)
	{
		return Now - DoneAt > 1.5;
	}
	if (!bSetUp)
	{
		if (!SetUp(World, PC, Kate))
		{
			return true;
		}
		bSetUp = true;
		SetUpAt = Now;
		return false;
	}
	if (!bStarted)
	{
		if (Crimes->IsCrimeActive())
		{
			bStarted = true;
			StartedAt = Now;
			Victim = Crimes->GetVictim();
			Thugs = Crimes->GetThugs();
			const UCrimeDefinition* Definition = Crimes->GetActiveDefinition();
			Test->AddInfo(FString::Printf(TEXT("Crime lap: %s started at %s after %.1f s, %d thugs."), *GetNameSafe(Definition),
				*GetNameSafe(Crimes->GetActiveSpot()), Now - SetUpAt, Thugs.Num()));
			Test->TestTrue(TEXT("The due crime is a mugging"), Definition && Definition->Type == ECrimeType::Mugging);
			Test->TestTrue(TEXT("At the spot 30 m from her"), Crimes->GetActiveSpot() == Spot.Get());
		}
		else if (Now - SetUpAt > 10.0)
		{
			Finish(World, PC, Kate, TEXT("no crime started within 10 s"));
		}
		return false;
	}
	if (!Crimes->IsCrimeActive())
	{
		Finish(World, PC, Kate, TEXT("the crime ended"));
		return false;
	}
	ArrowsBeforeEnd = Kate->GetInventoryComponent()->GetArrowCount(1);
	const float Health = Kate->GetHealthComponent()->GetCurrentHealth();
	if (Health < LastHealth - 0.01f)
	{
		++HitsTaken;
		DamageTaken += LastHealth - Health;
		Test->AddInfo(FString::Printf(TEXT("Crime lap %.1f s: Kate hit for %.0f, health %.0f"), Now - StartedAt, LastHealth - Health, Health));
	}
	LastHealth = Health;
	if (!Kate->GetHealthComponent()->IsAlive() || Now - StartedAt > 90.0)
	{
		Finish(World, PC, Kate, Kate->GetHealthComponent()->IsAlive() ? TEXT("timed out") : TEXT("Kate died"));
		return false;
	}
	Fight(World, PC, Kate, Now);
	return false;
}

void FHawkeyeCrimeMuggingRunner::Fight(UWorld* World, APlayerController* PC, AHawkeyeCharacter* Kate, double Now)
{
	using namespace HawkeyeCrimeLap;
	AThugCharacter* Target = nullptr;
	float Distance = BIG_NUMBER;
	for (const TWeakObjectPtr<AThugCharacter>& Thug : Thugs)
	{
		const float D = IsDown(Thug.Get()) ? BIG_NUMBER : FVector::Dist2D(Thug->GetActorLocation(), Kate->GetActorLocation());
		if (D < Distance)
		{
			Distance = D;
			Target = Thug.Get();
		}
	}
	if (!Target)
	{
		Move(PC, FVector2D::ZeroVector);
		return;
	}
	AimAt(PC, Kate, Target->GetActorLocation() + FVector(0.f, 0.f, 30.f));
	if (bMeleeHeld)
	{
		if (Now - MeleeHeldSince >= 0.45)
		{
			Hold(PC, MeleePath, false);
			bMeleeHeld = false;
			NextActionAt = Now + 0.1;
		}
		return;
	}
	if (bDrawing)
	{
		if (Now - DrawStart >= 0.85 || Distance < 300.f)
		{
			Hold(PC, FirePath, false);
			bDrawing = false;
			++ArrowsLoosed;
			NextActionAt = Now + 0.2;
		}
		return;
	}
	if (Now - LastParryTap > 0.3 && CanParryNow(Kate) && ParryChance(Thugs, Kate, PC))
	{
		Move(PC, FVector2D::ZeroVector);
		Tap(PC, MeleePath);
		LastParryTap = Now;
		++Parries;
		NextActionAt = Now + 0.25;
		return;
	}
	const bool bBusy = Kate->IsMeleeAttacking() || Kate->IsDodging() || Kate->IsStaggered() || Kate->IsLockedOutByTakedown();
	if (Now < NextActionAt || bBusy)
	{
		return;
	}
	if (Kate->GetFinisherComponent() && Kate->GetFinisherComponent()->FindTarget())
	{
		Move(PC, FVector2D::ZeroVector);
		Tap(PC, TakedownPath);
		++Finishers;
		NextActionAt = Now + 0.3;
		return;
	}
	// A swing about to land beside her: step out of it (a direct call, as in the other melee laps).
	for (const TWeakObjectPtr<AThugCharacter>& Weak : Thugs)
	{
		const AThugCharacter* Thug = Weak.Get();
		const UMeleeComponent* Melee = Thug ? Thug->GetMeleeComponent() : nullptr;
		if (!IsDown(Thug) && Melee && Melee->IsWindingUp() && Melee->GetPhaseRemaining() < 0.3f && !Thug->IsIncapacitated()
			&& FVector::Dist2D(Thug->GetActorLocation(), Kate->GetActorLocation()) <= 250.f && Kate->GetDodgeCooldownRemaining() <= 0.f
			&& Thug != Target)
		{
			const FVector Away = (Kate->GetActorLocation() - Thug->GetActorLocation()).GetSafeNormal2D();
			if (Kate->TryDodge((Away + FVector::CrossProduct(FVector::UpVector, Away)).GetSafeNormal2D()))
			{
				++Dodges;
				NextActionAt = Now + 0.3;
				return;
			}
		}
	}
	const FVector To = SteerTowards(World, Kate, Target->GetActorLocation());
	if (Now >= NextReportAt)
	{
		NextReportAt = Now + 3.0;
		FHitResult Block;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(CrimeLapBlock), false, Kate);
		const FVector Chest = Kate->GetActorLocation();
		const bool bBlocked = World->SweepSingleByChannel(Block, Chest, Chest + To * 80.f, FQuat::Identity, ECC_Pawn,
			FCollisionShape::MakeCapsule(30.f, 70.f), Params);
		Test->AddInfo(FString::Printf(TEXT("Crime lap %.1f s: Kate at %s, %.0f m from %s, speed %.0f, heading %.0f, mode %d, ahead %s."),
			Now - StartedAt, *Kate->GetActorLocation().ToCompactString(), Distance / 100.f, *Target->GetName(), Kate->GetVelocity().Size2D(),
			To.Rotation().Yaw, static_cast<int32>(Kate->GetCharacterMovement()->MovementMode.GetValue()),
			bBlocked && Block.GetActor() ? *Block.GetActor()->GetActorNameOrLabel() : TEXT("clear")));
	}
	if (Distance > 1500.f)
	{
		// Run in, sprinting; she is 30 m out and the victim's clock is running.
		if (!bSprinting)
		{
			Hold(PC, SprintPath, true);
			bSprinting = true;
		}
		Move(PC, MoveTowards(To, PC->GetControlRotation().Yaw));
		return;
	}
	if (bSprinting)
	{
		Hold(PC, SprintPath, false);
		bSprinting = false;
	}
	if (Distance <= 190.f)
	{
		Move(PC, FVector2D::ZeroVector);
		const UMeleeComponent* ThugMelee = Target->GetMeleeComponent();
		const bool bLight = (ThugMelee && ThugMelee->IsWindingUp()) || Target->IsKnockedDown() || Target->IsStaggered() || Strikes % 3 != 2;
		++Strikes;
		if (bLight)
		{
			Tap(PC, MeleePath);
			NextActionAt = Now + 0.32;
			return;
		}
		Hold(PC, MeleePath, true);
		bMeleeHeld = true;
		MeleeHeldSince = Now;
		return;
	}
	// Two arrows on the way in, from 5 to 15 m; after that she closes for the strikes.
	if (Distance >= 500.f && ArrowsLoosed < 2)
	{
		Move(PC, FVector2D::ZeroVector);
		Hold(PC, FirePath, true);
		bDrawing = true;
		DrawStart = Now;
		return;
	}
	Move(PC, MoveTowards(To, PC->GetControlRotation().Yaw));
}

bool FHawkeyeLapCrimeMugging::RunTest(const FString& Parameters)
{
	if (!FApp::CanEverRender())
	{
		AddInfo(TEXT("No RHI: skipping the crime lap. Run it from the standalone game (-game, no -nullrhi)."));
		return true;
	}
	AutomationOpenMap(TEXT("/Game/Maps/L_District_EastVillage"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(4.f));
	ADD_LATENT_AUTOMATION_COMMAND(FHawkeyeCrimeMuggingRunner(this));
	return true;
}

#endif

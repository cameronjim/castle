// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/CrimeScreenshots.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "CollisionQueryParams.h"
#include "Combat/HealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Crime/Civilian.h"
#include "Crime/CrimeDefinition.h"
#include "Crime/CrimeSpot.h"
#include "Crime/CrimeSubsystem.h"
#include "Crime/CrimeTracker.h"
#include "Engine/Engine.h"
#include "Engine/PointLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "HawkeyePlayerController.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Mission/MissionSubsystem.h"
#include "Player/HawkeyeCharacter.h"
#include "Player/InventoryComponent.h"
#include "Tests/AutomationCommon.h"
#include "Tests/HawkeyeShots.h"
#include "Tests/PartnerScreenshots.h"
#include "UI/CrimePanelWidget.h"
#include "UI/HawkeyeHudWidget.h"
#include "UI/HawkeyeObjectiveWidget.h"
#include "World/ThugAIController.h"
#include "World/ThugCharacter.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * The street crime shots (Saved/Screenshots/Kate/), run in Hawkeye.Screenshot.Kate after the challenge
 * pass and on their own as Hawkeye.Screenshot.Crime:
 *
 *   crime_marker.png   Kate's own camera 30 m from the crime spot nearest the PlayerStart, a mugging just
 *                      started there: the "[Crime: mugging] 30 m" HUD line top right and the purple
 *                      secondary marker over the victim
 *   crime_mugging.png  a camera 5 m from the victim: the two thugs over the cowering grey civilian
 *   crime_results.png  both thugs put down by script: the "[Crime stopped]" toast with the arrows, the
 *                      freed civilian's "[thank you]" over his head
 *
 * The crime's thugs are frozen for the frames (they stand where they spawned); every other thug is
 * frozen and Kate is invulnerable. She goes back where she was afterwards and the crime is cleaned up.
 */
namespace HawkeyeCrimeShots
{
	enum class EShot : uint8
	{
		Setup,
		Marker,
		MarkerReport,
		Mugging,
		Results,
		ResultsReport,
		Cleanup,
	};

	static TWeakObjectPtr<ACameraActor> ShotCamera;
	static TWeakObjectPtr<APointLight> FillLight;
	static TWeakObjectPtr<ACrimeSpot> Spot;
	static FTransform KateStart;
	static bool bKateWasInvulnerable = false;

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

	static FString ShotPath(const FString& FileName)
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Screenshots") / TEXT("Kate") / FileName);
	}

	static void Frame(UWorld* World, APlayerController* PC, const FVector& Eye, const FVector& Target, float Fov, float Fill)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ACameraActor* Camera = ShotCamera.Get();
		if (!Camera)
		{
			Camera = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, Params);
			ShotCamera = Camera;
		}
		APointLight* Light = FillLight.Get();
		if (!Light)
		{
			Light = World->SpawnActor<APointLight>(APointLight::StaticClass(), FTransform::Identity, Params);
			FillLight = Light;
			if (UPointLightComponent* Lamp = Light ? Light->PointLightComponent.Get() : nullptr)
			{
				Lamp->SetMobility(EComponentMobility::Movable);
				Lamp->SetIntensityUnits(ELightUnits::Lumens);
				Lamp->SetAttenuationRadius(1200.f);
				Lamp->SetCastShadows(false);
			}
		}
		if (!Camera)
		{
			return;
		}
		Camera->SetActorLocationAndRotation(Eye, (Target - Eye).Rotation());
		Camera->GetCameraComponent()->SetFieldOfView(Fov);
		Camera->GetCameraComponent()->bConstrainAspectRatio = false;
		PC->SetViewTarget(Camera);
		if (Light)
		{
			Light->SetActorLocation(Eye + FVector(0.f, 0.f, 80.f));
			Light->PointLightComponent->SetIntensity(Fill);
		}
	}

	/** The street spot with a mugging nearest the PlayerStart. */
	static ACrimeSpot* NearestMuggingSpot(UWorld* World, const UCrimeSubsystem* Crimes)
	{
		FVector From = FVector::ZeroVector;
		if (TActorIterator<APlayerStart> Start(World); Start)
		{
			From = Start->GetActorLocation();
		}
		ACrimeSpot* Best = nullptr;
		for (ACrimeSpot* Candidate : Crimes->GetSpots())
		{
			if (!Candidate->bRooftop && Candidate->FindCrime(ECrimeType::Mugging)
				&& (!Best || FVector::Dist2D(Candidate->GetActorLocation(), From) < FVector::Dist2D(Best->GetActorLocation(), From)))
			{
				Best = Candidate;
			}
		}
		return Best;
	}

	/**
	 * Kate on open street Distance from At, facing it; false when no such ground was found. Best a spot
	 * she can see it from, then one where only props (a car, a lamp) are in the way, then any street.
	 */
	static bool StandOff(UWorld* World, APlayerController* PC, AHawkeyeCharacter* Kate, const FVector& At, float Distance)
	{
		FVector Toward(1.f, 0.f, 0.f);
		if (TActorIterator<APlayerStart> Start(World); Start)
		{
			Toward = (Start->GetActorLocation() - At).GetSafeNormal2D();
		}
		FCollisionQueryParams Params(SCENE_QUERY_STAT(CrimeShotGround), false, Kate);
		for (int32 Pass = 0; Pass < 3; ++Pass)
		{
			for (int32 Step = 0; Step < 24; ++Step)
			{
				const FVector Out = Toward.RotateAngleAxis((Step % 2 == 0 ? 1.f : -1.f) * 15.f * ((Step + 1) / 2), FVector::UpVector);
				const FVector Probe = At + Out * Distance;
				FHitResult Hit;
				// From high above, so a probe inside a building's footprint finds its roof, not the slab under it.
				if (!World->LineTraceSingleByChannel(Hit, Probe + FVector(0.f, 0.f, 20000.f), Probe - FVector(0.f, 0.f, 500.f), ECC_Visibility,
					Params) || Hit.ImpactPoint.Z > At.Z + 60.f || (Hit.GetActor() && Hit.GetActor()->ActorHasTag(TEXT("CityBuilding"))))
				{
					continue;
				}
				FHitResult Line;
				const bool bBlocked = World->LineTraceSingleByChannel(Line, Hit.ImpactPoint + FVector(0.f, 0.f, 170.f),
					At + FVector(0.f, 0.f, 120.f), ECC_Visibility, Params);
				const bool bByBuilding = bBlocked && Line.GetActor() && Line.GetActor()->ActorHasTag(TEXT("CityBuilding"));
				if ((Pass == 0 && bBlocked) || (Pass == 1 && bByBuilding))
				{
					continue;
				}
				const float Yaw = (-Out).Rotation().Yaw;
				const float HalfHeight = Kate->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
				Kate->TeleportTo(Hit.ImpactPoint + FVector(0.f, 0.f, HalfHeight + 2.f), FRotator(0.f, Yaw, 0.f), false, true);
				Kate->SetActorRotation(FRotator(0.f, Yaw, 0.f));
				PC->SetControlRotation(FRotator(-4.f, Yaw, 0.f));
				return true;
			}
		}
		return false;
	}

	static void FreezeCrimeThugs(const UCrimeSubsystem* Crimes)
	{
		for (const TWeakObjectPtr<AThugCharacter>& Thug : Crimes->GetThugs())
		{
			if (AThugAIController* Brain = Thug.IsValid() ? Cast<AThugAIController>(Thug->GetController()) : nullptr)
			{
				Brain->SetThinkingEnabled(false);
			}
		}
	}
}

DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FHawkeyeCrimeShot, FAutomationTestBase*, Test, uint8, Shot);

bool FHawkeyeCrimeShot::Update()
{
	using namespace HawkeyeCrimeShots;
	UWorld* World = FindWorld();
	AHawkeyePlayerController* PC = World ? Cast<AHawkeyePlayerController>(World->GetFirstPlayerController()) : nullptr;
	AHawkeyeCharacter* Kate = PC ? Cast<AHawkeyeCharacter>(PC->GetPawn()) : nullptr;
	UCrimeSubsystem* Crimes = UCrimeSubsystem::Get(World);
	if (!World || !Kate || !Crimes)
	{
		Test->AddError(TEXT("Crime shots: no world, no player, or no crime subsystem."));
		return true;
	}
	ACrimeSpot* Here = Spot.Get();
	const UHawkeyeHudWidget* Hud = AHawkeyePlayerController::GetHawkeyeHudFor(Kate);

	switch (static_cast<EShot>(Shot))
	{
	case EShot::Setup:
	{
		if (Kate->GetCharacterName().ToString() != TEXT("Kate"))
		{
			PC->bAllowSwitchingOverride = true;
			PC->SwitchCharacter();
			Kate = Cast<AHawkeyeCharacter>(PC->GetPawn());
		}
		HawkeyeFreezePartner(World);
		for (TActorIterator<AThugCharacter> It(World); It; ++It)
		{
			if (AThugAIController* Brain = Cast<AThugAIController>(It->GetController()))
			{
				Brain->SetThinkingEnabled(false);
			}
		}
		KateStart = Kate->GetActorTransform();
		bKateWasInvulnerable = Kate->GetHealthComponent()->IsInvulnerable();
		Kate->GetHealthComponent()->SetInvulnerable(true);
		Kate->StopAim();
		Kate->GetInventoryComponent()->SetArrowCount(1, 20);
		Crimes->AbortCrime();
		Crimes->DespawnLeftoversNow();
		Spot = NearestMuggingSpot(World, Crimes);
		if (!Spot.IsValid())
		{
			Test->AddError(TEXT("Crime shots: no City_CrimeSpot_ with a mugging on the district."));
		}
		break;
	}

	case EShot::Marker:
	{
		UCrimeDefinition* Mugging = Here ? Here->FindCrime(ECrimeType::Mugging) : nullptr;
		if (!Mugging || !StandOff(World, PC, Kate, Here->GetActorLocation(), 3000.f))
		{
			Test->AddError(TEXT("crime_marker.png: no mugging, or no open street 30 m from the spot."));
			break;
		}
		PC->SetViewTarget(Kate);
		if (!Crimes->StartCrimeAt(Here, Mugging, Kate))
		{
			Test->AddError(TEXT("crime_marker.png: the mugging did not start."));
			break;
		}
		FreezeCrimeThugs(Crimes);
		break;
	}

	case EShot::MarkerReport:
	{
		const UCrimePanelWidget* Panel = Hud ? Hud->GetCrimePanel() : nullptr;
		const UHawkeyeObjectiveWidget* Markers = Hud ? Hud->GetObjectiveMarker() : nullptr;
		Test->AddInfo(FString::Printf(TEXT("crime_marker.png: panel %s (%s | %s); %d secondary marker(s); %.0f m to the crime."),
			Panel && Panel->IsPanelVisible() ? TEXT("up") : TEXT("down"), Panel ? *Panel->GetTitleText().ToString() : TEXT("-"),
			Panel ? *Panel->GetStatusText().ToString() : TEXT("-"), Markers ? Markers->GetSecondaryMarkerCount() : 0,
			FVector::Dist(Kate->GetActorLocation(), Crimes->GetCrimePoint()) / 100.f));
		if (!Panel || !Panel->IsPanelVisible() || !Panel->GetTitleText().ToString().StartsWith(TEXT("[Crime: mugging]")))
		{
			Test->AddError(TEXT("crime_marker.png: the crime's HUD line is not up."));
		}
		if (!Markers || Markers->GetSecondaryMarkerCount() != 1)
		{
			Test->AddError(TEXT("crime_marker.png: no secondary marker on the crime."));
		}
		break;
	}

	case EShot::Mugging:
	{
		const ACivilian* Victim = Crimes->GetVictim();
		if (!Victim)
		{
			Test->AddError(TEXT("crime_mugging.png: no victim."));
			break;
		}
		// From the open street side, a little above head height, so both thugs and the hunched victim read.
		const FVector Centre = Victim->GetActorLocation();
		FVector Side = (Kate->GetActorLocation() - Centre).GetSafeNormal2D();
		Side = Side.IsNearlyZero() ? FVector(1.f, 0.f, 0.f) : Side;
		Frame(World, PC, Centre + Side.RotateAngleAxis(25.f, FVector::UpVector) * 480.f + FVector(0.f, 0.f, 90.f), Centre - FVector(0.f, 0.f, 20.f),
			55.f, 260.f);
		Test->AddInfo(FString::Printf(TEXT("crime_mugging.png: %d thugs, %d on the victim, victim cowering %s, hit %d time(s)."),
			Crimes->GetThugs().Num(), Crimes->CountThugsOnVictim(), Victim->IsCowering() ? TEXT("yes") : TEXT("no"), Victim->GetHitsTaken()));
		if (Crimes->CountThugsOnVictim() != 2 || !Victim->IsCowering())
		{
			Test->AddError(TEXT("crime_mugging.png: the two thugs are not over a cowering victim."));
		}
		break;
	}

	case EShot::Results:
	{
		// Both put down, as if she had fought them; she stands over the victim on her own camera.
		for (const TWeakObjectPtr<AThugCharacter>& Thug : Crimes->GetThugs())
		{
			if (Thug.IsValid())
			{
				Thug->GetHealthComponent()->ApplyDamage(10000.f, Kate);
			}
		}
		if (const ACivilian* Victim = Crimes->GetVictim())
		{
			const FVector Centre = Victim->GetActorLocation();
			FVector Out = (Kate->GetActorLocation() - Centre).GetSafeNormal2D();
			Out = Out.IsNearlyZero() ? FVector(1.f, 0.f, 0.f) : Out;
			const float HalfHeight = Kate->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			const FVector Feet = Centre - FVector(0.f, 0.f, Victim->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()) + Out * 420.f;
			Kate->TeleportTo(Feet + FVector(0.f, 0.f, HalfHeight + 2.f), (-Out).Rotation(), false, true);
			PC->SetControlRotation(FRotator(-6.f, (-Out).Rotation().Yaw, 0.f));
		}
		PC->SetViewTarget(Kate);
		if (APointLight* Light = FillLight.Get())
		{
			Light->SetActorLocation(Kate->GetActorLocation() + FVector(0.f, 0.f, 200.f));
		}
		break;
	}

	case EShot::ResultsReport:
	{
		const UCrimeTracker* Tracker = Crimes->GetTracker();
		const FCrimeResult Result = Tracker->HasLastResult() ? Tracker->GetLastResult() : FCrimeResult();
		const UHawkeyeObjectiveWidget* Toasts = Hud ? Hud->GetObjectiveMarker() : nullptr;
		const ACivilian* Victim = nullptr;
		for (TActorIterator<ACivilian> It(World); It; ++It)
		{
			Victim = *It;
		}
		Test->AddInfo(FString::Printf(TEXT("crime_results.png: %s, +%d arrows, %d stopped; toast \"%s / %s\"; thanks %s."),
			*UEnum::GetValueAsString(Result.Reason), Result.ArrowsGiven, Result.Completions,
			Toasts ? *Toasts->GetToastHeading().ToString() : TEXT("-"), Toasts ? *Toasts->GetToastTitle().ToString() : TEXT("-"),
			Victim && Victim->IsThanksShowing() ? TEXT("showing") : TEXT("not showing")));
		if (!Result.IsCompleted() || Crimes->IsCrimeActive())
		{
			Test->AddError(TEXT("crime_results.png: the mugging did not complete."));
		}
		break;
	}

	case EShot::Cleanup:
		Crimes->AbortCrime();
		Crimes->DespawnLeftoversNow();
		PC->SetViewTarget(Kate);
		if (ACameraActor* Camera = ShotCamera.Get())
		{
			Camera->Destroy();
		}
		if (APointLight* Light = FillLight.Get())
		{
			Light->Destroy();
		}
		Kate->TeleportTo(KateStart.GetLocation(), KateStart.Rotator(), false, true);
		Kate->GetHealthComponent()->Heal(1000.f);
		Kate->GetHealthComponent()->SetInvulnerable(bKateWasInvulnerable);
		break;
	}
	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FHawkeyeCrimeTakeShot, FAutomationTestBase*, Test, FString, FileName);

bool FHawkeyeCrimeTakeShot::Update()
{
	HawkeyeShots::Request(Test, HawkeyeCrimeShots::ShotPath(FileName), /*bShowUI=*/true);
	return true;
}

void HawkeyeAddCrimeShots(FAutomationTestBase* Test)
{
	using EShot = HawkeyeCrimeShots::EShot;
	auto Shot = [Test](EShot Which, float Wait)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FHawkeyeCrimeShot(Test, static_cast<uint8>(Which)));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(Wait));
	};
	auto Take = [Test](const TCHAR* File, float Wait)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FHawkeyeCrimeTakeShot(Test, FString(File)));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(Wait));
	};

	Shot(EShot::Setup, 0.5f);
	// Long enough for the spawned thugs and the victim to settle on the pavement and the marker to project.
	Shot(EShot::Marker, 1.5f);
	Shot(EShot::MarkerReport, 0.1f);
	Take(TEXT("crime_marker.png"), 0.5f);

	Shot(EShot::Mugging, 1.2f);
	Take(TEXT("crime_mugging.png"), 0.5f);

	// The toast and the thanks both last longer than this.
	Shot(EShot::Results, 0.7f);
	Shot(EShot::ResultsReport, 0.1f);
	Take(TEXT("crime_results.png"), 0.5f);

	Shot(EShot::Cleanup, 0.5f);
}

/** The crime shots on their own, for tuning without the whole Kate pass. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHawkeyeScreenshotCrime, "Hawkeye.Screenshot.Crime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FHawkeyeScreenshotCrime::RunTest(const FString& Parameters)
{
	if (!FApp::CanEverRender())
	{
		AddInfo(TEXT("No RHI: skipping the crime screenshots."));
		return true;
	}
	AutomationOpenMap(TEXT("/Game/Maps/L_District_EastVillage"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(6.f));
	HawkeyeAddCrimeShots(this);
	// Fails the test for any capture that did not reach the disk.
	ADD_LATENT_AUTOMATION_COMMAND(FHawkeyeWaitForShots(this));
	return true;
}

#endif

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/MeleeScreenshots.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Combat/BowComponent.h"
#include "Combat/BowIKAnimInstance.h"
#include "Combat/FinisherComponent.h"
#include "Combat/HealthComponent.h"
#include "Combat/MeleeComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/PointLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Partner/HawkeyePartnerController.h"
#include "Player/HawkeyeCharacter.h"
#include "Tests/AutomationCommon.h"
#include "Tests/PartnerScreenshots.h"
#include "World/ThugAIController.h"
#include "World/ThugCharacter.h"

#include "Tests/HawkeyeShots.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * The melee shots (Saved/Screenshots/Kate/), run in Hawkeye.Screenshot.Kate after the fair-fight shots
 * and on their own as Hawkeye.Screenshot.Melee. On the street by the PlayerStart, against a bat thug
 * the pass spawns (BP_Thug) and removes, every other thug frozen, Kate invulnerable:
 *
 *   combo_x3.png      her own camera, three lights chained into him: the counter at x3 just after the third
 *   parry_flash.png   side on, a tap of V into his bat's wind-up: the purple ring between them, him staggering
 *   finisher_mid.png  her own camera 0.3 s into a finisher on him (staggered): time at 0.5, the lens pushed in
 *   hit_lean.png      from in front of him, 0.1 s after she hits him from his left: his upper body leant right
 *   strike_pose.png   side on, a light at the peak of the punch: her right hand out 70 cm at chest height
 *
 * The world is slowed to a crawl for each capture that is about a moment (all but the finisher, whose
 * own slow motion is the subject) and put back after.
 */
namespace HawkeyeMeleeShots
{
	static const TCHAR* ThugClassPath = TEXT("/Game/Blueprints/AI/BP_Thug.BP_Thug_C");
	static constexpr float CrawlDilation = 0.02f;

	enum class EShot : uint8
	{
		Setup,
		ComboSetup,
		ComboHit,
		ParrySetup,
		ParrySwing,
		Parry,
		FinisherSetup,
		Finisher,
		LeanSetup,
		LeanHit,
		StrikeSetup,
		Strike,
		Crawl,
		Uncrawl,
		Cleanup,
	};

	static TWeakObjectPtr<AThugCharacter> Foe;
	static TArray<TWeakObjectPtr<AThugCharacter>> Spawned;
	static TWeakObjectPtr<ACameraActor> ShotCamera;
	static TWeakObjectPtr<APointLight> FillLight;
	static FVector StreetFeet = FVector::ZeroVector;
	static FVector Along = FVector::ForwardVector;
	static FTransform KateStart;
	static bool bKateWasInvulnerable = false;
	static bool bHaveStreet = false;

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

	static bool Ground(UWorld* World, const FVector& XY, float FromZ, const TArray<const AActor*>& Ignore, FVector& Out)
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(MeleeShotGround), false);
		Params.AddIgnoredActors(Ignore);
		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, FVector(XY.X, XY.Y, FromZ), FVector(XY.X, XY.Y, FromZ - 6000.f), ECC_Visibility, Params))
		{
			return false;
		}
		Out = Hit.ImpactPoint;
		return true;
	}

	static void Stand(ACharacter* Character, const FVector& Feet, float Yaw)
	{
		const float HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		Character->TeleportTo(Feet + FVector(0.f, 0.f, HalfHeight + 2.f), FRotator(0.f, Yaw, 0.f), false, true);
		Character->SetActorRotation(FRotator(0.f, Yaw, 0.f));
	}

	/** A camera at Eye looking at Target, with a soft fill light over the lens (the street is at night). */
	static void Frame(UWorld* World, APlayerController* PC, const FVector& Eye, const FVector& Target, float Fov)
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
				Lamp->SetAttenuationRadius(900.f);
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
			Light->PointLightComponent->SetIntensity(400.f);
		}
	}

	/** Back to her own camera, looking from YawOffset degrees round her facing, a little down. */
	static void OwnCamera(APlayerController* PC, AHawkeyeCharacter* Kate, float YawOffset)
	{
		PC->SetViewTarget(Kate);
		PC->SetControlRotation(FRotator(-10.f, Kate->GetActorRotation().Yaw + YawOffset, 0.f));
		if (APointLight* Light = FillLight.Get())
		{
			// Her own camera sits in the street lamps' light; only a little fill.
			Light->SetActorLocation(Kate->GetActorLocation() + FVector(0.f, 0.f, 250.f));
			Light->PointLightComponent->SetIntensity(80.f);
		}
	}

	/** A fresh bat thug from BP_Thug, not thinking, at Feet facing Yaw. */
	static AThugCharacter* SpawnFoe(UWorld* World, const FVector& Feet, float Yaw)
	{
		UClass* ThugClass = LoadClass<AThugCharacter>(nullptr, ThugClassPath);
		if (!ThugClass)
		{
			ThugClass = AThugCharacter::StaticClass();
		}
		const FTransform At(FRotator(0.f, Yaw, 0.f), Feet + FVector(0.f, 0.f, 100.f));
		AThugCharacter* Thug = World->SpawnActorDeferred<AThugCharacter>(ThugClass, At, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
		if (!Thug)
		{
			return nullptr;
		}
		Thug->Weapon = EThugWeapon::Bat;
		UGameplayStatics::FinishSpawningActor(Thug, At);
		Thug->RefreshHeldWeapon();
		if (AThugAIController* Brain = Cast<AThugAIController>(Thug->GetController()))
		{
			Brain->SetThinkingEnabled(false);
		}
		Stand(Thug, Feet, Yaw);
		Spawned.Add(Thug);
		return Thug;
	}

	/** Kate on the street spot facing along it, and a fresh foe Distance in front of her facing her. */
	static bool FaceOff(UWorld* World, AHawkeyeCharacter* Kate, float Distance, float Yaw = 0.f)
	{
		if (!bHaveStreet)
		{
			return false;
		}
		const FVector Facing = FRotator(0.f, Along.Rotation().Yaw + Yaw, 0.f).Vector();
		Stand(Kate, StreetFeet, Facing.Rotation().Yaw);
		if (AThugCharacter* Old = Foe.Get())
		{
			Old->Destroy();
		}
		FVector FoeFeet;
		if (!Ground(World, StreetFeet + Facing * Distance, StreetFeet.Z + 200.f, { Kate }, FoeFeet))
		{
			FoeFeet = StreetFeet + Facing * Distance;
		}
		Foe = SpawnFoe(World, FoeFeet, (-Facing).Rotation().Yaw);
		return Foe.IsValid();
	}

	static void SetCrawl(UWorld* World, bool bCrawl)
	{
		UGameplayStatics::SetGlobalTimeDilation(World, bCrawl ? CrawlDilation : 1.f);
	}

	static FString Vec(const FVector& V) { return V.ToCompactString(); }
}

DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FHawkeyeMeleeShot, FAutomationTestBase*, Test, uint8, Shot);

bool FHawkeyeMeleeShot::Update()
{
	using namespace HawkeyeMeleeShots;
	UWorld* World = FindWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	AHawkeyeCharacter* Kate = PC ? Cast<AHawkeyeCharacter>(PC->GetPawn()) : nullptr;
	if (!World || !Kate)
	{
		Test->AddError(TEXT("Melee shots: no world or no Kate."));
		return true;
	}
	AThugCharacter* Thug = Foe.Get();

	switch (static_cast<EShot>(Shot))
	{
	case EShot::Setup:
	{
		Spawned.Reset();
		Foe = nullptr;
		KateStart = Kate->GetActorTransform();
		bKateWasInvulnerable = Kate->GetHealthComponent()->IsInvulnerable();
		Kate->GetHealthComponent()->SetInvulnerable(true);
		Kate->StopAim();
		for (TActorIterator<AThugCharacter> It(World); It; ++It)
		{
			if (AThugAIController* Brain = Cast<AThugAIController>(It->GetController()))
			{
				Brain->SetThinkingEnabled(false);
			}
		}
		HawkeyeFreezePartner(World);
		bHaveStreet = false;
		TActorIterator<APlayerStart> Start(World);
		if (Start)
		{
			const FVector Away = Start->GetActorForwardVector().GetSafeNormal2D();
			Along = FVector::CrossProduct(FVector::UpVector, Away);
			bHaveStreet = Ground(World, Start->GetActorLocation() - Away * 400.f, Start->GetActorLocation().Z + 3000.f, { Kate },
				StreetFeet);
			// Clint well out of the way.
			for (TActorIterator<AHawkeyePartnerController> It(World); It; ++It)
			{
				if (AHawkeyeCharacter* Clint = It->GetPartner())
				{
					Stand(Clint, StreetFeet - Along * 1500.f, Along.Rotation().Yaw);
				}
			}
		}
		if (!bHaveStreet)
		{
			Test->AddWarning(TEXT("Melee shots: no street by the PlayerStart."));
		}
		break;
	}

	case EShot::ComboSetup:
		if (!FaceOff(World, Kate, 110.f))
		{
			Test->AddWarning(TEXT("combo_x3.png: could not stand a thug in front of her."));
			break;
		}
		OwnCamera(PC, Kate, 35.f);
		break;

	case EShot::ComboHit:
		Kate->StartLightAttack();
		break;

	case EShot::ParrySetup:
	{
		if (!FaceOff(World, Kate, 130.f))
		{
			Test->AddWarning(TEXT("parry_flash.png: could not stand a thug in front of her."));
			break;
		}
		// "In front" is her view: the control rotation, whatever camera the shot looks through.
		PC->SetControlRotation(FRotator(-10.f, Kate->GetActorRotation().Yaw, 0.f));
		// Three-quarters from behind her, as she sees it: the ring faces along the line between them.
		const FVector Mid = (Foe->GetActorLocation() + Kate->GetActorLocation()) * 0.5f;
		const FVector Side = FVector::CrossProduct(FVector::UpVector, Along);
		Frame(World, PC, Mid - Along * 300.f + Side * 260.f + FVector(0.f, 0.f, 70.f), Mid + FVector(0.f, 0.f, 25.f), 55.f);
		break;
	}

	case EShot::ParrySwing:
		if (Thug)
		{
			Thug->GetMeleeComponent()->StartAttack(Thug->GetMeleeAttack());
		}
		break;

	case EShot::Parry:
	{
		const bool bParried = Kate->TryParry();
		Test->AddInfo(FString::Printf(TEXT("parry_flash.png: parried %d (%d in all); %s staggered %.2f s, swinging %d."),
			bParried ? 1 : 0, Kate->GetParryCount(), *GetNameSafe(Thug), Thug ? Thug->GetStaggerRemaining() : 0.f,
			Thug && Thug->GetMeleeComponent()->IsAttacking() ? 1 : 0));
		if (!bParried)
		{
			Test->AddWarning(TEXT("parry_flash.png: the parry did not take."));
		}
		break;
	}

	case EShot::FinisherSetup:
		if (!FaceOff(World, Kate, 140.f, 0.f) || !Foe.IsValid())
		{
			Test->AddWarning(TEXT("finisher_mid.png: could not stand a thug in front of her."));
			break;
		}
		Foe->StaggerFor(Kate, 2.f);
		OwnCamera(PC, Kate, 40.f);
		break;

	case EShot::Finisher:
		if (!Kate->TryFinisher())
		{
			Test->AddWarning(TEXT("finisher_mid.png: the finisher did not start."));
		}
		break;

	case EShot::LeanSetup:
	{
		// She stands at his left; he faces along the street; the camera is in front of him.
		if (!FaceOff(World, Kate, 110.f, 90.f) || !Foe.IsValid())
		{
			Test->AddWarning(TEXT("hit_lean.png: could not stand a thug beside her."));
			break;
		}
		AThugCharacter* Him = Foe.Get();
		Him->SetActorRotation(FRotator(0.f, Kate->GetActorRotation().Yaw - 90.f, 0.f));
		PC->SetControlRotation(FRotator(-10.f, Kate->GetActorRotation().Yaw, 0.f));
		const FVector Front = Him->GetActorForwardVector().GetSafeNormal2D();
		// Square on from in front of him, far enough back for both of them.
		const FVector Between = (Him->GetActorLocation() + Kate->GetActorLocation()) * 0.5f;
		Frame(World, PC, Between + Front * 420.f + FVector(0.f, 0.f, 20.f), Between + FVector(0.f, 0.f, 10.f), 45.f);
		break;
	}

	case EShot::LeanHit:
		if (Thug)
		{
			Thug->GetHealthComponent()->ApplyDamage(1.f, Kate);
		}
		break;

	case EShot::StrikeSetup:
	{
		if (!FaceOff(World, Kate, 220.f))
		{
			Test->AddWarning(TEXT("strike_pose.png: no street spot."));
			break;
		}
		PC->SetControlRotation(FRotator(-10.f, Kate->GetActorRotation().Yaw, 0.f));
		const FVector Side = FVector::CrossProduct(FVector::UpVector, Along);
		const FVector Chest = Kate->GetActorLocation() + Along * 35.f + FVector(0.f, 0.f, 35.f);
		Frame(World, PC, Chest + Side * 380.f + FVector(0.f, 0.f, 10.f), Chest - FVector(0.f, 0.f, 15.f), 50.f);
		break;
	}

	case EShot::Strike:
		Kate->StartLightAttack();
		break;

	case EShot::Crawl:
		SetCrawl(World, true);
		break;

	case EShot::Uncrawl:
		SetCrawl(World, false);
		break;

	case EShot::Cleanup:
		SetCrawl(World, false);
		PC->SetViewTarget(Kate);
		if (ACameraActor* Camera = ShotCamera.Get())
		{
			Camera->Destroy();
		}
		if (APointLight* Light = FillLight.Get())
		{
			Light->Destroy();
		}
		for (const TWeakObjectPtr<AThugCharacter>& Weak : Spawned)
		{
			if (AThugCharacter* Made = Weak.Get())
			{
				Made->Destroy();
			}
		}
		Spawned.Reset();
		Foe = nullptr;
		Kate->TeleportTo(KateStart.GetLocation(), KateStart.Rotator(), false, true);
		Kate->GetHealthComponent()->Heal(1000.f);
		Kate->GetHealthComponent()->SetInvulnerable(bKateWasInvulnerable);
		break;
	}
	return true;
}

/** Waits (up to Timeout) for the moment a shot is about, then reports it. */
class FHawkeyeMeleeWait : public IAutomationLatentCommand
{
public:
	FHawkeyeMeleeWait(FAutomationTestBase* InTest, FString InLabel, float InTimeout)
		: Test(InTest), Label(MoveTemp(InLabel)), Timeout(InTimeout) {}

	virtual bool Update() override
	{
		using namespace HawkeyeMeleeShots;
		UWorld* World = FindWorld();
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		AHawkeyeCharacter* Kate = PC ? Cast<AHawkeyeCharacter>(PC->GetPawn()) : nullptr;
		if (!World || !Kate)
		{
			return true;
		}
		const double Now = FPlatformTime::Seconds();
		if (Start < 0.0)
		{
			Start = Now;
		}
		const bool bTimedOut = Now - Start >= Timeout;
		const AThugCharacter* Thug = Foe.Get();
		const USkeletalMeshComponent* Body = Kate->GetMesh();
		const UHawkeyeBowIKAnimInstance* Hands = Kate->GetBowComponent() ? Kate->GetBowComponent()->GetHandsIKInstance() : nullptr;
		const USpringArmComponent* Arm = Kate->GetCameraBoom();

		if (Label == TEXT("finisher_mid.png"))
		{
			const UFinisherComponent* Finisher = Kate->GetFinisherComponent();
			const bool bReady = Finisher && Finisher->IsPerformingFinisher() && Finisher->IsSlowing() && Finisher->GetElapsed() >= 0.3f;
			if (!bReady && !bTimedOut)
			{
				return false;
			}
			Test->AddInfo(FString::Printf(TEXT("finisher_mid.png: %.2f s in, slowing %d, time dilation %.2f, camera push %.2f (arm %.0f cm), ")
				TEXT("invulnerable %d, locked %d, thug alive %d."),
				Finisher ? Finisher->GetElapsed() : 0.f, Finisher && Finisher->IsSlowing() ? 1 : 0,
				World->GetWorldSettings()->TimeDilation, Finisher ? Finisher->GetCameraPushAlpha() : 0.f,
				Arm ? Arm->TargetArmLength : 0.f, Kate->GetHealthComponent()->IsInvulnerable() ? 1 : 0,
				Kate->IsLockedOutByTakedown() ? 1 : 0, Thug && Thug->GetHealthComponent()->IsAlive() ? 1 : 0));
			if (!bReady)
			{
				Test->AddWarning(TEXT("finisher_mid.png: never caught the finisher in its slow motion."));
			}
			return true;
		}
		if (Label == TEXT("combo_x3.png"))
		{
			Test->AddInfo(FString::Printf(TEXT("combo_x3.png: counter x%d, chain next step %d, swing %s, thug health %.0f."),
				Kate->GetComboCount(), Kate->GetCombo().GetNextStep(), *Kate->GetMeleeComponent()->GetCurrentAttack().Name.ToString(),
				Thug ? Thug->GetHealthComponent()->GetCurrentHealth() : -1.f));
			if (Kate->GetComboCount() != 3)
			{
				Test->AddWarning(FString::Printf(TEXT("combo_x3.png: the counter reads x%d, not x3."), Kate->GetComboCount()));
			}
			return true;
		}
		if (Label == TEXT("hit_lean.png"))
		{
			const UHawkeyeBowIKAnimInstance* ThugHands = Thug && Thug->GetBowComponent() ? Thug->GetBowComponent()->GetHandsIKInstance() : nullptr;
			Test->AddInfo(FString::Printf(TEXT("hit_lean.png: hit from %s, lean alpha %.2f toward %s; thug graph %s, HitLean %s."),
				Thug ? *UEnum::GetValueAsString(Thug->GetLastHitDirection()) : TEXT("-"), Thug ? Thug->GetHitLean().GetAlpha() : 0.f,
				Thug ? *Vec(Thug->GetHitLean().GetDirection()) : TEXT("-"), ThugHands ? *ThugHands->GetClass()->GetName() : TEXT("none"),
				ThugHands ? *ThugHands->HitLean.ToCompactString() : TEXT("-")));
			if (!ThugHands)
			{
				Test->AddWarning(TEXT("hit_lean.png: the thug runs no bow-IK graph, so nothing leans him."));
			}
			return true;
		}
		if (Label == TEXT("strike_pose.png"))
		{
			const FVector Hand = Body ? Body->GetSocketLocation(TEXT("hand_r")) : FVector::ZeroVector;
			const FVector Target = Kate->GetActorTransform().TransformPosition(Kate->GetStrikePose().Settings.LightHand);
			Test->AddInfo(FString::Printf(TEXT("strike_pose.png: pose %s alpha %.2f; arm alpha %.2f; hand_r %.0f cm in front, %.0f cm up, ")
				TEXT("%.1f cm from the target."),
				*UEnum::GetValueAsString(Kate->GetStrikePose().GetPose()), Kate->GetStrikePose().GetAlpha(), Hands ? Hands->RightArmAlpha : -1.f,
				FVector::DotProduct(Hand - Kate->GetActorLocation(), Kate->GetActorForwardVector()), Hand.Z - Kate->GetActorLocation().Z,
				FVector::Dist(Hand, Target)));
			if (!Hands)
			{
				Test->AddWarning(TEXT("strike_pose.png: Kate runs no bow-IK graph."));
			}
			return true;
		}
		return true;
	}

private:
	FAutomationTestBase* Test;
	FString Label;
	float Timeout;
	double Start = -1.0;
};

DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FHawkeyeMeleeTakeShot, FAutomationTestBase*, Test, FString, FileName);

bool FHawkeyeMeleeTakeShot::Update()
{
	HawkeyeShots::Request(Test, HawkeyeMeleeShots::ShotPath(FileName), /*bShowUI=*/true);
	return true;
}

void HawkeyeAddMeleeShots(FAutomationTestBase* Test)
{
	using EShot = HawkeyeMeleeShots::EShot;
	auto Shot = [Test](EShot Which, float Wait)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FHawkeyeMeleeShot(Test, static_cast<uint8>(Which)));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(Wait));
	};
	auto Wait = [Test](const TCHAR* Label, float Timeout)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FHawkeyeMeleeWait(Test, FString(Label), Timeout));
	};
	auto Take = [Test](const TCHAR* File)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FHawkeyeMeleeTakeShot(Test, FString(File)));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.4f));
	};

	Shot(EShot::Setup, 0.3f);

	// Three lights, each pressed in the last one's recovery: the third lands at about 0.72 s.
	Shot(EShot::ComboSetup, 1.2f);
	Shot(EShot::ComboHit, 0.25f);
	Shot(EShot::ComboHit, 0.25f);
	// Caught just after the third, once his hit flash has gone: the counter at x3, him shoved back.
	Shot(EShot::ComboHit, 0.5f);
	Shot(EShot::Crawl, 0.05f);
	Wait(TEXT("combo_x3.png"), 0.f);
	Take(TEXT("combo_x3.png"));
	Shot(EShot::Uncrawl, 2.5f);

	// A tap of V 0.3 s into his 0.6 s bat wind-up; the ring 0.17 s later (past the parry's hit stop).
	Shot(EShot::ParrySetup, 1.2f);
	Shot(EShot::ParrySwing, 0.3f);
	Shot(EShot::Parry, 0.17f);
	Shot(EShot::Crawl, 0.05f);
	Take(TEXT("parry_flash.png"));
	Shot(EShot::Uncrawl, 2.f);

	// The finisher on a staggered thug, caught in its slow motion.
	Shot(EShot::FinisherSetup, 1.f);
	Shot(EShot::Finisher, 0.f);
	Wait(TEXT("finisher_mid.png"), 2.f);
	Take(TEXT("finisher_mid.png"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.f));

	// A hit from his left: 0.1 s in, the lean at its height.
	Shot(EShot::LeanSetup, 1.2f);
	Shot(EShot::LeanHit, 0.1f);
	Shot(EShot::Crawl, 0.05f);
	Wait(TEXT("hit_lean.png"), 0.f);
	Take(TEXT("hit_lean.png"));
	Shot(EShot::Uncrawl, 0.8f);

	// A light at the peak of the punch (0.1 s in), nobody in reach so no hit stop.
	Shot(EShot::StrikeSetup, 1.2f);
	Shot(EShot::Strike, 0.1f);
	Shot(EShot::Crawl, 0.05f);
	Wait(TEXT("strike_pose.png"), 0.f);
	Take(TEXT("strike_pose.png"));
	Shot(EShot::Uncrawl, 0.5f);

	Shot(EShot::Cleanup, 0.5f);
}

/** The melee shots on their own, for tuning without the whole Kate pass. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHawkeyeScreenshotMelee, "Hawkeye.Screenshot.Melee",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FHawkeyeScreenshotMelee::RunTest(const FString& Parameters)
{
	if (!FApp::CanEverRender())
	{
		AddInfo(TEXT("No RHI: skipping the melee screenshots."));
		return true;
	}
	AutomationOpenMap(TEXT("/Game/Maps/L_District_EastVillage"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(6.f));
	HawkeyeAddMeleeShots(this);
	// Fails the test for any capture that did not reach the disk.
	ADD_LATENT_AUTOMATION_COMMAND(FHawkeyeWaitForShots(this));
	return true;
}

#endif

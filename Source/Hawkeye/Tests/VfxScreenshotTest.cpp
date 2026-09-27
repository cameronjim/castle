// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/VfxScreenshots.h"

#include "Combat/ArrowDefinition.h"
#include "Combat/ArrowEffects/ArrowEffect.h"
#include "Combat/ArrowProjectile.h"
#include "Combat/BowComponent.h"
#include "Combat/BowDefinition.h"
#include "Combat/HealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/PlatformFileManager.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Player/HawkeyeCharacter.h"
#include "Tests/AutomationCommon.h"
#include "UnrealClient.h"
#include "Vfx/HawkeyeVfxSubsystem.h"
#include "World/ThugAIController.h"
#include "World/ThugCharacter.h"

#include "Tests/HawkeyeShots.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * The effect shots, queued onto Hawkeye.Screenshot.Kate after the street frame time (Kate
 * invulnerable, every placed thug frozen). All of them 30 m along East 7th Street from the
 * PlayerStart, Kate looking down the street, the effects set off through the real game path
 * (AArrowEffect::SpawnForHit with the chapter's data assets, a real arrow from her bow):
 *
 *   vfx_snowfall.png   standing still, the light snow falling round the camera
 *   vfx_footstep.png   0.9 s into a walk away from the camera: snow kicked back from her heels
 *   vfx_explosion.png  0.15 s after an explosive arrow 9 m ahead: fireball, core flash, sparks
 *   vfx_emp.png        0.25 s after an EMP 7 m ahead: the ring racing out, arcs, the screen split
 *   vfx_hit_spark.png  a standard arrow into a thug 4.5 m ahead: the purple-white spark, no blood
 *   vfx_smoke.png      2 s after a smoke arrow 9 m ahead: the cloud billowing across the street
 *
 * Each logs how many effects the VFX subsystem was asked for and actually spawned.
 */
namespace HawkeyeVfxShots
{
	enum class EShot : uint8
	{
		Setup,
		Stand,
		Walk,
		StopWalk,
		Explosion,
		Emp,
		HitSparkSetup,
		HitSparkFire,
		Smoke,
		Cleanup,
	};

	static const TCHAR* MoveActionPath = TEXT("/Game/Input/IA_Move.IA_Move");
	static const TCHAR* WeaponsPath = TEXT("/Game/Blueprints/Weapons/");
	static const TCHAR* ThugClassPath = TEXT("/Game/Blueprints/AI/BP_Thug.BP_Thug_C");

	static FVector Spot = FVector::ZeroVector;
	static float Yaw = 0.f;
	static bool bKateWasInvulnerable = false;
	static TWeakObjectPtr<AThugCharacter> Target;
	static TWeakObjectPtr<AArrowProjectile> Arrow;

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

	static AHawkeyeCharacter* FindKate(APlayerController*& OutPC)
	{
		UWorld* World = FindWorld();
		OutPC = World ? World->GetFirstPlayerController() : nullptr;
		return OutPC ? Cast<AHawkeyeCharacter>(OutPC->GetPawn()) : nullptr;
	}

	static FString ShotPath(const FString& FileName)
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Screenshots") / TEXT("Kate") / FileName);
	}

	static bool Ground(UWorld* World, const FVector& XY, const AActor* Ignore, FVector& Out)
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(VfxShotGround), false, Ignore);
		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, FVector(XY.X, XY.Y, XY.Z + 800.f), FVector(XY.X, XY.Y, XY.Z - 3000.f),
				ECC_Visibility, Params))
		{
			return false;
		}
		Out = Hit.ImpactPoint;
		return true;
	}

	static FVector Along() { return FRotator(0.f, Yaw, 0.f).Vector(); }

	static void HoldMove(APlayerController* PC, const FVector2D& Value, bool bHold)
	{
		const UInputAction* Move = LoadObject<UInputAction>(nullptr, MoveActionPath);
		ULocalPlayer* Player = PC ? PC->GetLocalPlayer() : nullptr;
		UEnhancedInputLocalPlayerSubsystem* Input =
			Player ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Player) : nullptr;
		if (!Move || !Input)
		{
			return;
		}
		if (bHold)
		{
			Input->StartContinuousInputInjectionForAction(Move, FInputActionValue(Value), {}, {});
		}
		else
		{
			Input->StopContinuousInputInjectionForAction(Move);
		}
	}

	static void PlaceKate(AHawkeyeCharacter* Kate, APlayerController* PC, float Pitch)
	{
		const float HalfHeight = Kate->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		Kate->StopAim();
		Kate->TeleportTo(Spot + FVector(0.f, 0.f, HalfHeight + 2.f), FRotator(0.f, Yaw, 0.f), false, true);
		PC->SetControlRotation(FRotator(Pitch, Yaw, 0.f));
		PC->SetViewTarget(Kate);
	}

	static UArrowDefinition* LoadArrow(const TCHAR* Name)
	{
		return LoadObject<UArrowDefinition>(nullptr, *FString::Printf(TEXT("%s%s.%s"), WeaponsPath, Name, Name));
	}

	/** Sets the arrow Name off on the ground Ahead cm down the street, as if it had landed there. */
	static bool SetOff(FAutomationTestBase* Test, AHawkeyeCharacter* Kate, const TCHAR* Name, float Ahead)
	{
		UWorld* World = Kate->GetWorld();
		UArrowDefinition* Definition = LoadArrow(Name);
		FVector Point;
		if (!Definition || !Ground(World, Spot + Along() * Ahead, Kate, Point))
		{
			Test->AddError(FString::Printf(TEXT("vfx: no %s or no ground %.0f cm ahead."), Name, Ahead));
			return false;
		}
		FHitResult Hit;
		Hit.bBlockingHit = true;
		Hit.ImpactPoint = Point;
		Hit.Location = Point;
		Hit.ImpactNormal = FVector::UpVector;
		Hit.Normal = FVector::UpVector;
		Hit.TraceStart = Point + FVector(0.f, 0.f, 100.f);
		Hit.TraceEnd = Point - FVector(0.f, 0.f, 100.f);
		const AArrowEffect* Effect = AArrowEffect::SpawnForHit(World, Definition, Kate, Hit);
		Test->AddInfo(FString::Printf(TEXT("vfx: %s set off at %s (%s, niagara %s)."), Name, *Point.ToCompactString(),
			*GetNameSafe(Effect), Effect && Effect->HasEffectVfx() ? TEXT("drawn") : TEXT("NOT drawn")));
		return Effect != nullptr;
	}

	static void SetThugsThinking(UWorld* World, bool bThinking)
	{
		for (TActorIterator<AThugCharacter> It(World); It; ++It)
		{
			if (AThugAIController* Brain = Cast<AThugAIController>(It->GetController()))
			{
				Brain->SetThinkingEnabled(bThinking);
			}
		}
	}

	static bool Setup(FAutomationTestBase* Test, AHawkeyeCharacter* Kate, APlayerController* PC)
	{
		UWorld* World = Kate->GetWorld();
		const APlayerStart* Start = nullptr;
		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			Start = *It;
			break;
		}
		if (!Start)
		{
			Test->AddError(TEXT("vfx: no PlayerStart."));
			return false;
		}
		// The PlayerStart faces the tenement row across East 7th; the street runs a quarter turn round.
		Yaw = Start->GetActorRotation().Yaw + 90.f;
		if (!Ground(World, Start->GetActorLocation() + Along() * 3000.f, Kate, Spot))
		{
			Test->AddError(TEXT("vfx: no ground 30 m down the street."));
			return false;
		}
		SetThugsThinking(World, false);
		if (UHealthComponent* Health = Kate->GetHealthComponent())
		{
			bKateWasInvulnerable = Health->IsInvulnerable();
			Health->SetInvulnerable(true);
		}
		Test->AddInfo(FString::Printf(TEXT("vfx: shots at %s looking along yaw %.0f."), *Spot.ToCompactString(), Yaw));
		return true;
	}

	static void SetUpHitSpark(FAutomationTestBase* Test, AHawkeyeCharacter* Kate, APlayerController* PC)
	{
		UWorld* World = Kate->GetWorld();
		PlaceKate(Kate, PC, -4.f);
		UClass* ThugClass = LoadClass<AThugCharacter>(nullptr, ThugClassPath);
		FVector Point;
		if (!ThugClass || !Ground(World, Spot + Along() * 450.f, Kate, Point))
		{
			Test->AddError(TEXT("vfx: no BP_Thug or no ground for the hit spark."));
			return;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		AThugCharacter* Thug = World->SpawnActor<AThugCharacter>(ThugClass, Point + FVector(0.f, 0.f, 100.f),
			FRotator(0.f, Yaw + 180.f, 0.f), Params);
		if (Thug && !Thug->GetController())
		{
			Thug->SpawnDefaultController();
		}
		if (AThugAIController* Brain = Thug ? Cast<AThugAIController>(Thug->GetController()) : nullptr)
		{
			Brain->SetThinkingEnabled(false);
		}
		Target = Thug;
	}

	static void FireAtTarget(FAutomationTestBase* Test, AHawkeyeCharacter* Kate)
	{
		AThugCharacter* Thug = Target.Get();
		UBowComponent* Bow = Kate->FindComponentByClass<UBowComponent>();
		UArrowDefinition* Standard = LoadArrow(TEXT("DA_Arrow_Standard"));
		if (!Thug || !Bow || !Standard)
		{
			Test->AddError(TEXT("vfx: no thug, bow or standard arrow for the hit spark."));
			return;
		}
		const FVector Chest = Thug->GetActorLocation() + FVector(0.f, 0.f, 35.f);
		const FVector From = Chest - Along() * 250.f;
		TSubclassOf<AArrowProjectile> Class = Standard->ProjectileClass ? Standard->ProjectileClass
			: TSubclassOf<AArrowProjectile>(AArrowProjectile::StaticClass());
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.Owner = Kate;
		AArrowProjectile* Shot = Kate->GetWorld()->SpawnActor<AArrowProjectile>(Class, From, Along().Rotation(), Params);
		if (Shot)
		{
			Shot->InitArrow(Standard, Bow->GetBow(), 40.f, Kate, Bow);
			Shot->LaunchWithVelocity((Chest - From).GetSafeNormal() * 6000.f);
		}
		Arrow = Shot;
	}

	static void Cleanup(FAutomationTestBase* Test, AHawkeyeCharacter* Kate, APlayerController* PC)
	{
		HoldMove(PC, FVector2D::ZeroVector, false);
		if (AThugCharacter* Thug = Target.Get())
		{
			if (AController* Brain = Thug->GetController())
			{
				Brain->Destroy();
			}
			Thug->Destroy();
		}
		if (AArrowProjectile* Shot = Arrow.Get())
		{
			Shot->Destroy();
		}
		if (UHealthComponent* Health = Kate->GetHealthComponent())
		{
			Health->SetInvulnerable(bKateWasInvulnerable);
		}
		if (const UHawkeyeVfxSubsystem* Vfx = UHawkeyeVfxSubsystem::Find(Kate))
		{
			Test->AddInfo(FString::Printf(TEXT("vfx: %d effect requests, %d spawned, this run."), Vfx->GetTotalRequests(),
				Vfx->GetSpawnCount()));
		}
	}
}

DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FHawkeyeVfxShot, FAutomationTestBase*, Test, uint8, Shot);

bool FHawkeyeVfxShot::Update()
{
	using namespace HawkeyeVfxShots;
	APlayerController* PC = nullptr;
	AHawkeyeCharacter* Kate = FindKate(PC);
	if (!Kate)
	{
		Test->AddError(TEXT("vfx: no Kate."));
		return true;
	}
	switch (static_cast<EShot>(Shot))
	{
	case EShot::Setup:
		Setup(Test, Kate, PC);
		break;
	case EShot::Stand:
		PlaceKate(Kate, PC, -6.f);
		break;
	case EShot::Walk:
		PlaceKate(Kate, PC, -12.f);
		HoldMove(PC, FVector2D(0.f, 1.f), true);
		break;
	case EShot::StopWalk:
		HoldMove(PC, FVector2D::ZeroVector, false);
		break;
	case EShot::Explosion:
		PlaceKate(Kate, PC, -4.f);
		SetOff(Test, Kate, TEXT("DA_Arrow_Explosive"), 900.f);
		break;
	case EShot::Emp:
		PlaceKate(Kate, PC, -10.f);
		SetOff(Test, Kate, TEXT("DA_Arrow_EMP"), 700.f);
		break;
	case EShot::HitSparkSetup:
		SetUpHitSpark(Test, Kate, PC);
		break;
	case EShot::HitSparkFire:
		FireAtTarget(Test, Kate);
		break;
	case EShot::Smoke:
		PlaceKate(Kate, PC, -4.f);
		SetOff(Test, Kate, TEXT("DA_Arrow_Smoke"), 900.f);
		break;
	case EShot::Cleanup:
		Cleanup(Test, Kate, PC);
		break;
	}
	return true;
}

/** Waits until the hit spark's arrow has landed (or MaxSeconds). */
DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FHawkeyeVfxWaitArrow, FAutomationTestBase*, Test, float, MaxSeconds);

bool FHawkeyeVfxWaitArrow::Update()
{
	const AArrowProjectile* Shot = HawkeyeVfxShots::Arrow.Get();
	if (Shot && Shot->IsStuck())
	{
		Test->AddInfo(FString::Printf(TEXT("vfx: the arrow landed in %s."), *GetNameSafe(Shot->GetStuckInActor())));
		return true;
	}
	if (!Shot || FPlatformTime::Seconds() - StartTime > MaxSeconds)
	{
		Test->AddWarning(TEXT("vfx: the hit spark's arrow never landed."));
		return true;
	}
	return false;
}

DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FHawkeyeVfxTakeShot, FAutomationTestBase*, Test, FString, FileName);

bool FHawkeyeVfxTakeShot::Update()
{
	const FString FullPath = HawkeyeVfxShots::ShotPath(FileName);
	HawkeyeShots::Request(Test, FullPath, /*bShowUI=*/true);
	return true;
}

void HawkeyeAddVfxShots(FAutomationTestBase* Test)
{
	using EShot = HawkeyeVfxShots::EShot;
	auto Step = [Test](EShot Shot) { ADD_LATENT_AUTOMATION_COMMAND(FHawkeyeVfxShot(Test, static_cast<uint8>(Shot))); };
	auto Wait = [](float Seconds) { ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(Seconds)); };
	auto Take = [Test](const TCHAR* File) { ADD_LATENT_AUTOMATION_COMMAND(FHawkeyeVfxTakeShot(Test, File)); };

	Step(EShot::Setup);
	Step(EShot::Stand);
	Wait(3.f);
	Take(TEXT("vfx_snowfall.png"));
	Wait(0.5f);

	Step(EShot::Walk);
	Wait(0.9f);
	Take(TEXT("vfx_footstep.png"));
	Wait(0.3f);
	Step(EShot::StopWalk);
	Wait(1.f);

	Step(EShot::Explosion);
	Wait(0.15f);
	Take(TEXT("vfx_explosion.png"));
	Wait(4.5f);

	Step(EShot::Emp);
	Wait(0.25f);
	Take(TEXT("vfx_emp.png"));
	Wait(1.5f);

	Step(EShot::HitSparkSetup);
	Wait(1.f);
	Step(EShot::HitSparkFire);
	ADD_LATENT_AUTOMATION_COMMAND(FHawkeyeVfxWaitArrow(Test, 1.f));
	Take(TEXT("vfx_hit_spark.png"));
	Wait(0.5f);

	Step(EShot::Smoke);
	Wait(2.f);
	Take(TEXT("vfx_smoke.png"));
	Wait(0.5f);
	Step(EShot::Cleanup);
}

#endif

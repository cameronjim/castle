// Copyright Epic Games, Inc. All Rights Reserved.

#include "HawkeyeGameMode.h"

#include "Hawkeye.h"
#include "HawkeyePlayerController.h"
#include "GameFramework/Pawn.h"
#include "Mission/MissionDefinition.h"
#include "Player/InventoryComponent.h"
#include "NavigationSystem.h"
#include "Mission/MissionSubsystem.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Player/HawkeyeCharacter.h"
#include "Camera/PlayerCameraManager.h"
#include "Combat/HealthComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Mission/MissionObjective.h"
#include "Partner/HawkeyePartnerController.h"
#include "Save/HawkeyeSaveSubsystem.h"
#include "TimerManager.h"
#include "World/ThugCharacter.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UnrealClient.h"

namespace HawkeyeGameModeClock
{
	/** When the last level change began (the outgoing game mode's EndPlay), wall seconds; 0 before any. */
	static double LevelChangeStartSeconds = 0.0;
}

AHawkeyeGameMode::AHawkeyeGameMode()
{
	DefaultPawnClass = AHawkeyeCharacter::StaticClass();
	PlayerControllerClass = AHawkeyePlayerController::StaticClass();
	// Only the playable mark and the frame watch after it; the main menu pauses the world under it.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

bool AHawkeyeGameMode::WantsOutdoorWeather(const UWorld* World)
{
	const AHawkeyeGameMode* GameMode = World ? World->GetAuthGameMode<AHawkeyeGameMode>() : nullptr;
	return GameMode && GameMode->bOutdoorWeather;
}

void AHawkeyeGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double Now = FPlatformTime::Seconds();
	if (PlayableSeconds < 0.f)
	{
		const UWorld* World = GetWorld();
		const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		if (PC && PC->GetPawn())
		{
			NotePlayable();
		}
		return;
	}
	WatchFrame(Now);
}

void AHawkeyeGameMode::NotePlayable()
{
	const double Now = FPlatformTime::Seconds();
	PlayableWallSeconds = Now;
	LastFrameWallSeconds = Now;
	FrameWatch = FHawkeyeFrameWatch();
	FrameWatch.ThresholdSeconds = HitchLogMs / 1000.0;
	const bool bFirstMap = HawkeyeGameModeClock::LevelChangeStartSeconds <= 0.0;
	const double From = bFirstMap ? GStartTime : HawkeyeGameModeClock::LevelChangeStartSeconds;
	PlayableSeconds = static_cast<float>(Now - From);
	UE_LOG(LogHawkeye, Log, TEXT("Playable after %.2f s (%s to the first tick with a possessed pawn; BeginPlay to it %.2f s, map %s)."),
		PlayableSeconds, bFirstMap ? TEXT("process start") : TEXT("level change"), Now - BeginPlayWallSeconds,
		*GetNameSafe(GetWorld()));

	ScreenshotProcessedHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(
		this, &AHawkeyeGameMode::HandleScreenshotProcessed);
}

void AHawkeyeGameMode::HandleScreenshotProcessed()
{
	bScreenshotThisFrame = true;
}

void AHawkeyeGameMode::WatchFrame(double Now)
{
	const double FrameSeconds = Now - LastFrameWallSeconds;
	LastFrameWallSeconds = Now;
	const double Since = Now - PlayableWallSeconds;
	// The capture ran in the previous frame's draw, so it lengthens the frame that ends now.
	const bool bScreenshot = bScreenshotThisFrame;
	bScreenshotThisFrame = false;
	if (FrameWatch.AddFrame(FrameSeconds, Since, bScreenshot))
	{
		UE_LOG(LogHawkeye, Log, TEXT("Hitch: a %.0f ms frame %.2f s after the playable mark (frame %d)%s."),
			FrameSeconds * 1000.0, Since, FrameWatch.Frames - 1, bScreenshot ? TEXT(", a screenshot capture") : TEXT(""));
	}
	if (!bFrameSummaryLogged && Since >= FrameSummarySeconds)
	{
		bFrameSummaryLogged = true;
		UE_LOG(LogHawkeye, Log, TEXT("First %.0f s after the playable mark: %d frames, worst %.0f ms at %.2f s%s; worst without captures %.0f ms at %.2f s; %d frame(s) of %.0f ms or more (%d of them screenshot captures)."),
			FrameSummarySeconds, FrameWatch.Frames, FrameWatch.WorstSeconds * 1000.0, FrameWatch.WorstAtSeconds,
			FrameWatch.bWorstWasScreenshot ? TEXT(" (a capture)") : TEXT(""), FrameWatch.WorstGameSeconds * 1000.0,
			FrameWatch.WorstGameAtSeconds, FrameWatch.HitchFrames, HitchLogMs, FrameWatch.ScreenshotHitchFrames);
	}
	// Wall time, not a timer: the main menu pauses the world on a fresh boot.
	if (QuitAfterPlayableSeconds >= 0.f && !bQuitRequested && Since >= QuitAfterPlayableSeconds)
	{
		bQuitRequested = true;
		UE_LOG(LogHawkeye, Log, TEXT("%s: -HawkeyeQuitAfterPlayable=%.1f: quitting."), *GetName(), QuitAfterPlayableSeconds);
		FPlatformMisc::RequestExit(/*bForce=*/false, TEXT("HawkeyeQuitAfterPlayable"));
	}
}

void AHawkeyeGameMode::BeginPlay()
{
	Super::BeginPlay();
	BeginPlayWallSeconds = FPlatformTime::Seconds();
	FParse::Value(FCommandLine::Get(), TEXT("HawkeyeQuitAfterPlayable="), QuitAfterPlayableSeconds);

	BuildNavigationIfEmpty();

	UMissionSubsystem* MissionSubsystem = UMissionSubsystem::Get(this);
	if (!MissionSubsystem)
	{
		return;
	}

	MissionSubsystem->OnMissionComplete.AddDynamic(this, &AHawkeyeGameMode::HandleMissionComplete);
	MissionSubsystem->OnMissionStarted.AddDynamic(this, &AHawkeyeGameMode::HandleMissionStarted);
	MissionSubsystem->OnObjectiveUpdated.AddDynamic(this, &AHawkeyeGameMode::HandleObjectiveUpdated);

	if (StartingMission)
	{
		MissionSubsystem->StartMission(StartingMission);
	}
	else
	{
		UE_LOG(LogHawkeye, Log, TEXT("%s has no StartingMission set."), *GetName());
	}

	// A load in progress: the mission has just started from the top; the save says how far it got.
	// Actors (Kate, Clint, thugs) are restored by SPUD right after BeginPlay.
	UHawkeyeSaveSubsystem* Save = UHawkeyeSaveSubsystem::Get(this);
	if (!Save)
	{
		return;
	}
	Save->ApplyPendingRestore(GetWorld());
	// Back from a playable scene without a save to load: put the player at the return point now.
	Save->ApplyPendingSceneReturn(GetWorld());
	GetWorldTimerManager().SetTimer(AutosaveTimerHandle, this, &AHawkeyeGameMode::TickAutosave, AutosaveTickSeconds, true);
	if (Save->ConsumeNewGameAutosave())
	{
		GetWorldTimerManager().SetTimer(NewGameSaveTimerHandle, this, &AHawkeyeGameMode::SaveNewGame, 1.5f, false);
	}
}

void AHawkeyeGameMode::SaveNewGame()
{
	if (UHawkeyeSaveSubsystem* Save = UHawkeyeSaveSubsystem::Get(this))
	{
		Save->SaveCampaign(TEXT("new game"));
	}
}

void AHawkeyeGameMode::HandleObjectiveUpdated(UMissionObjective* Objective, int32 /*ObjectiveIndex*/)
{
	UHawkeyeSaveSubsystem* Save = UHawkeyeSaveSubsystem::Get(this);
	if (Save && Objective && Objective->IsCompleted())
	{
		// Before OnMissionComplete, which clears the quiver for the end card.
		Save->SaveCampaign(FString::Printf(TEXT("objective %s"), *Objective->ObjectiveId.ToString()));
	}
}

bool AHawkeyeGameMode::IsWorldInCombat(const UWorld* World)
{
	if (!World)
	{
		return false;
	}
	for (TActorIterator<AThugCharacter> It(World); It; ++It)
	{
		const UHealthComponent* Health = It->GetHealthComponent();
		if (Health && Health->IsAlive() && It->GetAlertState() == EThugAlertState::Alerted)
		{
			return true;
		}
	}
	for (TActorIterator<AHawkeyePartnerController> It(World); It; ++It)
	{
		if (It->IsInFight())
		{
			return true;
		}
	}
	return false;
}

bool AHawkeyeGameMode::CanAutosaveNow() const
{
	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	const AHawkeyeCharacter* Player = PC ? Cast<AHawkeyeCharacter>(PC->GetPawn()) : nullptr;
	const UHealthComponent* Health = Player ? Player->GetHealthComponent() : nullptr;
	const UCharacterMovementComponent* Movement = Player ? Player->GetCharacterMovement() : nullptr;
	if (!Health || !Health->IsAlive() || Player->IsDowned() || !Movement || !Movement->IsMovingOnGround())
	{
		return false;
	}
	return !bRestartPending && !IsInCombat();
}

void AHawkeyeGameMode::TickAutosave()
{
	if (UHawkeyeSaveSubsystem* Save = UHawkeyeSaveSubsystem::Get(this))
	{
		Save->AdvanceAutosave(AutosaveTickSeconds, CanAutosaveNow());
	}
}

void AHawkeyeGameMode::ReloadLastCheckpoint(float FadeSeconds)
{
	UWorld* World = GetWorld();
	if (bRestartPending || !World)
	{
		return;
	}
	bRestartPending = true;
	OnMissionRestarting();

	const float Seconds = FadeSeconds < 0.f ? DeathFadeSeconds : FadeSeconds;
	APlayerController* PC = World->GetFirstPlayerController();
	if (PC && PC->PlayerCameraManager)
	{
		// Held at black: the load that follows swaps the world out underneath it.
		PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, Seconds, FLinearColor::Black, false, true);
	}
	UE_LOG(LogHawkeye, Log, TEXT("%s: fading out for %.1f s, then loading the last save."), *GetName(), Seconds);

	if (Seconds <= 0.f)
	{
		LoadCheckpointNow();
		return;
	}
	World->GetTimerManager().SetTimer(RestartTimerHandle, this, &AHawkeyeGameMode::LoadCheckpointNow, Seconds, false);
}

void AHawkeyeGameMode::LoadCheckpointNow()
{
	UHawkeyeSaveSubsystem* Save = UHawkeyeSaveSubsystem::Get(this);
	if (Save && Save->HasSave())
	{
		// Either the save loads, or LoadCampaign decides it is stale and starts a new game; both
		// travel. Only a refusal (SPUD busy) leaves nothing pending, and then the level reload runs.
		Save->LoadCampaign();
		if (Save->IsTravelPending())
		{
			return;
		}
	}
	UE_LOG(LogHawkeye, Log, TEXT("%s: no save to go back to; reloading the level."), *GetName());
	ReopenCurrentLevel();
}

void AHawkeyeGameMode::BuildNavigationIfEmpty()
{
	if (!bBuildNavigationAtStart)
	{
		return;
	}

	UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!NavSystem)
	{
		UE_LOG(LogHawkeye, Warning,
			TEXT("%s: no navigation system; nothing will patrol in this level."), *GetName());
		return;
	}

	UE_LOG(LogHawkeye, Log, TEXT("%s: building navigation."), *GetName());
	NavSystem->Build();
}

void AHawkeyeGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (EndPlayReason == EEndPlayReason::LevelTransition)
	{
		HawkeyeGameModeClock::LevelChangeStartSeconds = FPlatformTime::Seconds();
	}
	if (ScreenshotProcessedHandle.IsValid())
	{
		FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotProcessedHandle);
		ScreenshotProcessedHandle.Reset();
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RestartTimerHandle);
		World->GetTimerManager().ClearTimer(AutosaveTimerHandle);
		World->GetTimerManager().ClearTimer(NewGameSaveTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void AHawkeyeGameMode::RestartMission(float Delay)
{
	if (bRestartPending)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float WaitSeconds = Delay < 0.f ? RestartDelaySeconds : Delay;

	bRestartPending = true;

	// Starting over means starting over: the quiver goes back to what the mission grants.
	if (UInventoryComponent* Inventory = FindPlayerInventory())
	{
		Inventory->Clear();
	}

	OnMissionRestarting();

	UE_LOG(LogHawkeye, Log, TEXT("%s: restarting the mission in %.1f s."), *GetName(), WaitSeconds);

	if (WaitSeconds <= 0.f)
	{
		ReopenCurrentLevel();
		return;
	}

	World->GetTimerManager().SetTimer(
		RestartTimerHandle, this, &AHawkeyeGameMode::ReopenCurrentLevel, WaitSeconds, false);
}

void AHawkeyeGameMode::ReopenCurrentLevel()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FName CurrentLevel(*UGameplayStatics::GetCurrentLevelName(World, /*bRemovePrefixString=*/true));
	UGameplayStatics::OpenLevel(World, CurrentLevel);
}

UInventoryComponent* AHawkeyeGameMode::FindPlayerInventory() const
{
	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	return Pawn ? Pawn->FindComponentByClass<UInventoryComponent>() : nullptr;
}

void AHawkeyeGameMode::HandleMissionStarted(UMissionDefinition* Mission)
{
	UInventoryComponent* Inventory = FindPlayerInventory();
	if (!Inventory || !Mission)
	{
		return;
	}

	// Nothing carries between chapters: the chapter's own data asset says which bow and arrows
	// she starts with, and everything else she has to find again.
	Inventory->ApplyMissionStart(Mission);
}

void AHawkeyeGameMode::HandleMissionComplete(UMissionDefinition* Mission)
{
	UE_LOG(LogHawkeye, Log, TEXT("Mission complete: %s"),
		Mission ? *Mission->MissionName.ToString() : TEXT("<none>"));

	// Before the end card, so the last frame of gameplay is not a quiver the player keeps. A chapter
	// that hands the player back to the district keeps it: she goes on roaming with it.
	UInventoryComponent* Inventory = FindPlayerInventory();
	if (Inventory && !(Mission && Mission->bReturnToRoamingAtEnd))
	{
		Inventory->Clear();
	}

	OnMissionCompleted(Mission);
}

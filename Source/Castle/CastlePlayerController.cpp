// Copyright Epic Games, Inc. All Rights Reserved.

#include "CastlePlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Castle.h"
#include "CastleGameMode.h"
#include "Combat/HealthComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Flashback/FlashbackDefinition.h"
#include "Flashback/FlashbackWidget.h"
#include "GameFramework/Pawn.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Mission/MissionDefinition.h"
#include "Mission/MissionSubsystem.h"
#include "UI/CastleHudWidget.h"
#include "UI/CastlePauseWidget.h"

void ACastlePlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (UMissionSubsystem* MissionSubsystem = UMissionSubsystem::Get(this))
	{
		MissionSubsystem->OnFlashbackRequested.AddDynamic(this, &ACastlePlayerController::HandleFlashbackRequested);
	}

	AddPauseMappingContext();
	CreateHud();
}

void ACastlePlayerController::AddPauseMappingContext()
{
	if (!PauseMappingContext || !IsLocalController())
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Subsystem->AddMappingContext(PauseMappingContext, PauseMappingPriority);
	}
}

void ACastlePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!PauseAction)
	{
		return;
	}

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInput)
	{
		UE_LOG(LogCastle, Error,
			TEXT("%s: expected an EnhancedInputComponent; the pause key will not work."), *GetName());
		return;
	}

	EnhancedInput->BindAction(PauseAction, ETriggerEvent::Started, this, &ACastlePlayerController::Input_Pause);
}

void ACastlePlayerController::Input_Pause(const FInputActionValue& /*Value*/)
{
	TogglePause();
}

bool ACastlePlayerController::CanTogglePause() const
{
	// The slideshow pauses the game itself and restores the previous state on finish; letting
	// Escape unpause underneath it would leave the flashback running over live gameplay.
	if (bFlashbackActive)
	{
		return false;
	}

	// Dead: the mission is already reloading behind the death screen, and a pause menu whose
	// Resume button gives you a corpse back is worse than no menu.
	const APawn* ControlledPawn = GetPawn();
	const UHealthComponent* Health =
		ControlledPawn ? ControlledPawn->FindComponentByClass<UHealthComponent>() : nullptr;

	return !Health || Health->IsAlive();
}

void ACastlePlayerController::TogglePause()
{
	if (!CanTogglePause())
	{
		UE_LOG(LogCastle, Verbose, TEXT("%s: pause ignored (a flashback is playing, or the player is dead)."), *GetName());
		return;
	}

	SetPauseMenuOpen(!bPauseMenuOpen);
}

void ACastlePlayerController::SetPauseMenuOpen(bool bOpen)
{
	if (bPauseMenuOpen == bOpen)
	{
		return;
	}

	bPauseMenuOpen = bOpen;

	if (bOpen)
	{
		ShowPauseWidget();
	}
	else
	{
		HidePauseWidget();
	}

	SetPause(bOpen);
	ApplyPauseInputMode(bOpen);
}

UCastlePauseWidget* ACastlePlayerController::ShowPauseWidget()
{
	if (!PauseWidgetClass || !IsLocalController())
	{
		// Pausing still works; there is simply no menu drawn over it.
		UE_LOG(LogCastle, Warning, TEXT("%s has no PauseWidgetClass set."), *GetName());
		return nullptr;
	}

	if (!PauseWidget)
	{
		PauseWidget = CreateWidget<UCastlePauseWidget>(this, PauseWidgetClass);
		if (!PauseWidget)
		{
			UE_LOG(LogCastle, Warning, TEXT("%s: could not create the pause widget."), *GetName());
			return nullptr;
		}

		PauseWidget->OnResumeClicked.AddDynamic(this, &ACastlePlayerController::HandlePauseResumeClicked);
		PauseWidget->OnRestartMissionClicked.AddDynamic(this, &ACastlePlayerController::HandlePauseRestartClicked);
		PauseWidget->OnQuitToDesktopClicked.AddDynamic(this, &ACastlePlayerController::HandlePauseQuitClicked);
	}

	if (!PauseWidget->IsInViewport())
	{
		PauseWidget->AddToViewport(10);
	}
	PauseWidget->SetVisibility(ESlateVisibility::Visible);

	return PauseWidget;
}

void ACastlePlayerController::HidePauseWidget()
{
	if (PauseWidget)
	{
		PauseWidget->RemoveFromParent();
	}
}

void ACastlePlayerController::ApplyPauseInputMode(bool bPaused)
{
	if (!IsLocalController())
	{
		return;
	}

	if (bPaused)
	{
		// UI and Game so the buttons take clicks but the world is still shown behind them.
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		if (PauseWidget)
		{
			Mode.SetWidgetToFocus(PauseWidget->TakeWidget());
		}
		SetInputMode(Mode);
		bShowMouseCursor = true;
		return;
	}

	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = false;
}

void ACastlePlayerController::HandlePauseResumeClicked()
{
	SetPauseMenuOpen(false);
}

void ACastlePlayerController::HandlePauseRestartClicked()
{
	RestartMissionFromPause();
}

void ACastlePlayerController::HandlePauseQuitClicked()
{
	QuitToDesktop();
}

void ACastlePlayerController::RestartMissionFromPause()
{
	// Unpause first: the level travel and anything it triggers should run on a live world.
	SetPauseMenuOpen(false);

	if (ACastleGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ACastleGameMode>() : nullptr)
	{
		GameMode->RestartMission(0.f);
	}
}

void ACastlePlayerController::QuitToDesktop()
{
	SetPauseMenuOpen(false);
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, /*bIgnorePlatformRestrictions=*/false);
}

void ACastlePlayerController::CreateHud()
{
	if (HudWidget || !HudWidgetClass || !IsLocalController())
	{
		return;
	}

	HudWidget = CreateWidget<UCastleHudWidget>(this, HudWidgetClass);
	if (!HudWidget)
	{
		UE_LOG(LogCastle, Warning, TEXT("%s: could not create the HUD widget."), *GetName());
		return;
	}

	HudWidget->AddToViewport(0);
}

void ACastlePlayerController::SetHudVisible(bool bVisible)
{
	if (HudWidget)
	{
		HudWidget->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

UCastleHudWidget* ACastlePlayerController::GetCastleHudFor(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const ACastlePlayerController* PC = World ? Cast<ACastlePlayerController>(World->GetFirstPlayerController()) : nullptr;
	return PC ? PC->GetCastleHud() : nullptr;
}

void ACastlePlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UMissionSubsystem* MissionSubsystem = UMissionSubsystem::Get(this))
	{
		MissionSubsystem->OnFlashbackRequested.RemoveDynamic(this, &ACastlePlayerController::HandleFlashbackRequested);
	}

	if (ActiveFlashbackWidget)
	{
		ActiveFlashbackWidget->OnFlashbackFinished.RemoveDynamic(this, &ACastlePlayerController::HandleFlashbackFinished);
		ActiveFlashbackWidget = nullptr;
	}

	if (PauseWidget)
	{
		PauseWidget->OnResumeClicked.RemoveDynamic(this, &ACastlePlayerController::HandlePauseResumeClicked);
		PauseWidget->OnRestartMissionClicked.RemoveDynamic(this, &ACastlePlayerController::HandlePauseRestartClicked);
		PauseWidget->OnQuitToDesktopClicked.RemoveDynamic(this, &ACastlePlayerController::HandlePauseQuitClicked);
		PauseWidget->RemoveFromParent();
		PauseWidget = nullptr;
	}
	bPauseMenuOpen = false;

	if (HudWidget)
	{
		HudWidget->RemoveFromParent();
		HudWidget = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void ACastlePlayerController::HandleFlashbackRequested(UFlashbackDefinition* Flashback)
{
	PlayFlashback(Flashback);
}

UFlashbackWidget* ACastlePlayerController::PlayFlashback(UFlashbackDefinition* Flashback)
{
	if (!Flashback)
	{
		return nullptr;
	}

	// Escape belongs to the slideshow from here until OnFlashbackFinished.
	bFlashbackActive = true;

	if (!FlashbackWidgetClass)
	{
		UE_LOG(LogCastle, Warning,
			TEXT("%s has no FlashbackWidgetClass set; skipping the flashback."), *GetName());
		HandleFlashbackFinished(Flashback);
		return nullptr;
	}

	if (ActiveFlashbackWidget && ActiveFlashbackWidget->IsPlaying())
	{
		return ActiveFlashbackWidget;
	}

	// A flashback and the pause menu cannot share the screen; the slideshow wins.
	SetPauseMenuOpen(false);

	ActiveFlashbackWidget = CreateWidget<UFlashbackWidget>(this, FlashbackWidgetClass);
	if (!ActiveFlashbackWidget)
	{
		HandleFlashbackFinished(Flashback);
		return nullptr;
	}

	// The slideshow owns the screen while it plays.
	SetHudVisible(false);

	ActiveFlashbackWidget->OnFlashbackFinished.AddDynamic(this, &ACastlePlayerController::HandleFlashbackFinished);
	ActiveFlashbackWidget->Play(Flashback);

	return ActiveFlashbackWidget;
}

void ACastlePlayerController::HandleFlashbackFinished(UFlashbackDefinition* /*Flashback*/)
{
	if (ActiveFlashbackWidget)
	{
		ActiveFlashbackWidget->OnFlashbackFinished.RemoveDynamic(this, &ACastlePlayerController::HandleFlashbackFinished);
		ActiveFlashbackWidget = nullptr;
	}

	bFlashbackActive = false;

	SetHudVisible(true);

	if (bOpenNextLevelAfterFlashback)
	{
		TryOpenNextLevel();
	}
}

bool ACastlePlayerController::TryOpenNextLevel()
{
	const UMissionSubsystem* MissionSubsystem = UMissionSubsystem::Get(this);
	const UMissionDefinition* Mission = MissionSubsystem ? MissionSubsystem->GetCurrentMission() : nullptr;

	if (!Mission || Mission->NextLevel.IsNull())
	{
		return false;
	}

	UE_LOG(LogCastle, Log, TEXT("Travelling to next level: %s"), *Mission->NextLevel.ToString());
	UGameplayStatics::OpenLevelBySoftObjectPtr(this, Mission->NextLevel);
	return true;
}

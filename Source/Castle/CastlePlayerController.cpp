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
#include "Misc/PackageName.h"
#include "Mission/MissionDefinition.h"
#include "Mission/MissionFlowController.h"
#include "Mission/MissionSubsystem.h"
#include "UI/CastleHudWidget.h"
#include "UI/CastleInventoryWidget.h"
#include "UI/CastlePauseWidget.h"
#include "UI/CastleSettingsWidget.h"
#include "UI/MissionEndCardWidget.h"

void ACastlePlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (FinalCardPrompt.IsEmpty())
	{
		FinalCardPrompt = NSLOCTEXT("Castle", "EndCardPressAnyKey", "Press any key");
	}

	MissionFlow = NewObject<UMissionFlowController>(this, TEXT("MissionFlow"));

	if (UMissionSubsystem* MissionSubsystem = UMissionSubsystem::Get(this))
	{
		MissionSubsystem->OnMissionComplete.AddDynamic(this, &ACastlePlayerController::HandleMissionComplete);
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
	// Escape inside Settings is Back, not unpause: the player came from the pause menu and
	// that is where one press should put them.
	if (bSettingsOpen)
	{
		CloseSettings();
		return;
	}

	// Escape closes the inventory rather than opening a second menu over it.
	if (bInventoryOpen)
	{
		SetInventoryOpen(false);
		return;
	}

	TogglePause();
}

void ACastlePlayerController::ToggleInventory()
{
	SetInventoryOpen(!bInventoryOpen);
}

void ACastlePlayerController::SetInventoryOpen(bool bOpen)
{
	if (bInventoryOpen == bOpen)
	{
		return;
	}

	// One thing owns the pause at a time; the slideshow and the pause menu both outrank Tab.
	if (bOpen && (bPauseMenuOpen || bFlashbackActive))
	{
		return;
	}

	if (bOpen)
	{
		if (!InventoryWidgetClass || !IsLocalController())
		{
			UE_LOG(LogCastle, Warning, TEXT("%s has no InventoryWidgetClass set."), *GetName());
			return;
		}

		if (!InventoryWidget)
		{
			InventoryWidget = CreateWidget<UCastleInventoryWidget>(this, InventoryWidgetClass);
			if (!InventoryWidget)
			{
				UE_LOG(LogCastle, Warning, TEXT("%s: could not create the inventory widget."), *GetName());
				return;
			}
		}

		if (!InventoryWidget->IsInViewport())
		{
			InventoryWidget->AddToViewport(10);
		}
		InventoryWidget->SetVisibility(ESlateVisibility::Visible);
		InventoryWidget->BindToOwningPawn();
		InventoryWidget->RefreshRows();
	}
	else if (InventoryWidget)
	{
		InventoryWidget->RemoveFromParent();
	}

	bInventoryOpen = bOpen;
	SetPause(bOpen);
	ApplyPauseInputMode(bOpen);
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
		// Leaving the pause state at all takes the settings screen with it.
		bSettingsOpen = false;
		HideSettingsWidget();
		HidePauseWidget();
	}

	SetPause(bOpen);
	ApplyPauseInputMode(bOpen);
}

void ACastlePlayerController::OpenSettings()
{
	if (!bPauseMenuOpen || bSettingsOpen)
	{
		return;
	}

	if (!ShowSettingsWidget())
	{
		return;
	}

	// One menu at a time; the game stays paused underneath both.
	HidePauseWidget();
	bSettingsOpen = true;
	ApplyPauseInputMode(true);
}

void ACastlePlayerController::CloseSettings()
{
	if (!bSettingsOpen)
	{
		return;
	}

	bSettingsOpen = false;
	HideSettingsWidget();
	ShowPauseWidget();
	ApplyPauseInputMode(true);
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
		PauseWidget->OnSettingsClicked.AddDynamic(this, &ACastlePlayerController::HandlePauseSettingsClicked);
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

UCastleSettingsWidget* ACastlePlayerController::ShowSettingsWidget()
{
	if (!SettingsWidgetClass || !IsLocalController())
	{
		UE_LOG(LogCastle, Warning, TEXT("%s has no SettingsWidgetClass set."), *GetName());
		return nullptr;
	}

	if (!SettingsWidget)
	{
		SettingsWidget = CreateWidget<UCastleSettingsWidget>(this, SettingsWidgetClass);
		if (!SettingsWidget)
		{
			UE_LOG(LogCastle, Warning, TEXT("%s: could not create the settings widget."), *GetName());
			return nullptr;
		}

		SettingsWidget->OnBackRequested.AddDynamic(this, &ACastlePlayerController::HandleSettingsBackRequested);
	}

	if (!SettingsWidget->IsInViewport())
	{
		SettingsWidget->AddToViewport(11);
	}
	SettingsWidget->SetVisibility(ESlateVisibility::Visible);
	SettingsWidget->RefreshFromSettings();

	return SettingsWidget;
}

void ACastlePlayerController::HideSettingsWidget()
{
	if (SettingsWidget)
	{
		SettingsWidget->RemoveFromParent();
	}
}

TSharedPtr<SWidget> ACastlePlayerController::GetFocusedMenuWidget() const
{
	if (bInventoryOpen && InventoryWidget)
	{
		return InventoryWidget->TakeWidget();
	}
	if (bSettingsOpen && SettingsWidget)
	{
		return SettingsWidget->TakeWidget();
	}
	if (PauseWidget)
	{
		return PauseWidget->TakeWidget();
	}
	return nullptr;
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
		// Game and UI, so the Escape mapping still reaches this controller while a slider has
		// keyboard focus.
		if (const TSharedPtr<SWidget> Focus = GetFocusedMenuWidget())
		{
			Mode.SetWidgetToFocus(Focus);
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

void ACastlePlayerController::HandlePauseSettingsClicked()
{
	OpenSettings();
}

void ACastlePlayerController::HandleSettingsBackRequested()
{
	CloseSettings();
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
		MissionSubsystem->OnMissionComplete.RemoveDynamic(this, &ACastlePlayerController::HandleMissionComplete);
		MissionSubsystem->OnFlashbackRequested.RemoveDynamic(this, &ACastlePlayerController::HandleFlashbackRequested);
	}

	if (EndCardWidget)
	{
		EndCardWidget->OnEndCardFinished.RemoveDynamic(this, &ACastlePlayerController::HandleEndCardFinished);
		EndCardWidget->RemoveFromParent();
		EndCardWidget = nullptr;
	}

	if (ActiveFlashbackWidget)
	{
		ActiveFlashbackWidget->OnFlashbackFinished.RemoveDynamic(this, &ACastlePlayerController::HandleFlashbackFinished);
		ActiveFlashbackWidget = nullptr;
	}

	if (SettingsWidget)
	{
		SettingsWidget->OnBackRequested.RemoveDynamic(this, &ACastlePlayerController::HandleSettingsBackRequested);
		SettingsWidget->RemoveFromParent();
		SettingsWidget = nullptr;
	}
	bSettingsOpen = false;

	if (InventoryWidget)
	{
		InventoryWidget->RemoveFromParent();
		InventoryWidget = nullptr;
	}
	bInventoryOpen = false;

	if (PauseWidget)
	{
		PauseWidget->OnResumeClicked.RemoveDynamic(this, &ACastlePlayerController::HandlePauseResumeClicked);
		PauseWidget->OnSettingsClicked.RemoveDynamic(this, &ACastlePlayerController::HandlePauseSettingsClicked);
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
	// This arrives in the same frame as OnMissionComplete, while the end card is still up, so
	// the definition is only cached here; the flow plays it when it reaches the Flashback beat.
	PendingFlashback = Flashback;
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
	PendingFlashback = nullptr;

	SetHudVisible(true);

	// A flashback played outside a mission end (a debug key, a scripted beat) just ends.
	if (MissionFlow && MissionFlow->GetStep() == EMissionFlowStep::Flashback)
	{
		MissionFlow->Advance();
		PerformCurrentFlowStep();
		return;
	}

	if (bOpenNextLevelAfterFlashback)
	{
		TryOpenNextLevel();
	}
}

void ACastlePlayerController::HandleMissionComplete(UMissionDefinition* Mission)
{
	if (!MissionFlow)
	{
		return;
	}

	if (MissionFlow->IsRunning())
	{
		UE_LOG(LogCastle, Warning,
			TEXT("%s: mission completed while an end sequence was already running; ignoring."), *GetName());
		return;
	}

	CompletedMission = Mission;

	// The end card owns the screen from here; the HUD comes back only if we stay in the level.
	SetHudVisible(false);
	SetPauseMenuOpen(false);

	const bool bHasFlashback = Mission && !Mission->FlashbackToPlay.IsNull();
	const bool bHasNextLevel = Mission && !Mission->NextLevel.IsNull();

	MissionFlow->Begin(bHasFlashback, bHasNextLevel);
	PerformCurrentFlowStep();
}

void ACastlePlayerController::PerformCurrentFlowStep()
{
	if (!MissionFlow)
	{
		return;
	}

	switch (MissionFlow->GetStep())
	{
	case EMissionFlowStep::EndCard:
		ShowEndCard(CompletedMission, /*bWaitForInput=*/false);
		break;

	case EMissionFlowStep::Flashback:
	{
		HideEndCard();

		// PendingFlashback is what OnFlashbackRequested handed us; fall back to a synchronous
		// load so a mission that completes without the delegate still shows its slideshow.
		UFlashbackDefinition* Flashback = PendingFlashback;
		if (!Flashback && CompletedMission)
		{
			Flashback = CompletedMission->FlashbackToPlay.LoadSynchronous();
		}

		if (!Flashback)
		{
			UE_LOG(LogCastle, Warning, TEXT("%s: the mission's flashback could not be loaded."), *GetName());
			MissionFlow->Advance();
			PerformCurrentFlowStep();
			return;
		}

		PlayFlashback(Flashback);
		break;
	}

	case EMissionFlowStep::FinalCard:
		// No next level: the campaign is over, so hold the card until the player says go.
		ShowEndCard(CompletedMission, /*bWaitForInput=*/true);
		break;

	case EMissionFlowStep::OpenNextLevel:
		HideEndCard();
		if (!TryOpenNextLevel())
		{
			UE_LOG(LogCastle, Warning, TEXT("%s: no next level to travel to after all."), *GetName());
		}
		MissionFlow->Advance();
		break;

	case EMissionFlowStep::OpenMenuLevel:
		HideEndCard();
		OpenMenuLevel();
		MissionFlow->Advance();
		break;

	default:
		break;
	}
}

UMissionEndCardWidget* ACastlePlayerController::ShowEndCard(UMissionDefinition* Mission, bool bWaitForInput)
{
	if (!EndCardWidgetClass || !IsLocalController())
	{
		UE_LOG(LogCastle, Warning,
			TEXT("%s has no EndCardWidgetClass set; skipping the end card."), *GetName());
		HandleEndCardFinished(Mission);
		return nullptr;
	}

	if (!EndCardWidget)
	{
		EndCardWidget = CreateWidget<UMissionEndCardWidget>(this, EndCardWidgetClass);
		if (!EndCardWidget)
		{
			UE_LOG(LogCastle, Warning, TEXT("%s: could not create the end card widget."), *GetName());
			HandleEndCardFinished(Mission);
			return nullptr;
		}

		EndCardWidget->OnEndCardFinished.AddDynamic(this, &ACastlePlayerController::HandleEndCardFinished);
	}

	if (!EndCardWidget->IsInViewport())
	{
		EndCardWidget->AddToViewport(5);
	}
	EndCardWidget->SetVisibility(ESlateVisibility::Visible);

	if (bWaitForInput)
	{
		EndCardWidget->PlayAndWaitForInput(Mission, FinalCardPrompt);
	}
	else
	{
		EndCardWidget->Play(Mission);
	}

	return EndCardWidget;
}

void ACastlePlayerController::HideEndCard()
{
	if (EndCardWidget)
	{
		EndCardWidget->RemoveFromParent();
	}
}

void ACastlePlayerController::HandleEndCardFinished(UMissionDefinition* /*Mission*/)
{
	if (!MissionFlow)
	{
		return;
	}

	MissionFlow->Advance();
	PerformCurrentFlowStep();
}

void ACastlePlayerController::OpenMenuLevel()
{
	// L_MainMenu is a stage-5 asset; until it exists, land somewhere that loads rather than
	// throwing the player at a map name the engine cannot resolve.
	FName Target = MenuLevelName;
	if (Target.IsNone() || !FPackageName::DoesPackageExist(Target.ToString()))
	{
		UE_LOG(LogCastle, Log, TEXT("%s: %s does not exist; falling back to %s."),
			*GetName(), *Target.ToString(), *FallbackMenuLevelName.ToString());
		Target = FallbackMenuLevelName;
	}

	UE_LOG(LogCastle, Log, TEXT("Campaign over; returning to %s."), *Target.ToString());
	UGameplayStatics::OpenLevel(this, Target);
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

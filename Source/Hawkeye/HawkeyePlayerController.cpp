// Copyright Epic Games, Inc. All Rights Reserved.

#include "HawkeyePlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Hawkeye.h"
#include "HawkeyeGameMode.h"
#include "Combat/HealthComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Flashback/FlashbackDefinition.h"
#include "Flashback/FlashbackWidget.h"
#include "GameFramework/Pawn.h"
#include "InputActionValue.h"
#include "InputKeyEventArgs.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/PackageName.h"
#include "Mission/MissionDefinition.h"
#include "Mission/MissionFlowController.h"
#include "Mission/MissionSubsystem.h"
#include "UI/HawkeyeHudWidget.h"
#include "UI/HawkeyeInventoryWidget.h"
#include "UI/HawkeyePauseWidget.h"
#include "UI/HawkeyeSettingsWidget.h"
#include "UI/MissionEndCardWidget.h"
#include "Dialogue/BanterComponent.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "Partner/HawkeyePartnerController.h"
#include "Player/HawkeyeCharacter.h"

AHawkeyePlayerController::AHawkeyePlayerController()
{
	Banter = CreateDefaultSubobject<UBanterComponent>(TEXT("Banter"));
}

void AHawkeyePlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (FinalCardPrompt.IsEmpty())
	{
		FinalCardPrompt = NSLOCTEXT("Hawkeye", "EndCardPressAnyKey", "Press any key");
	}

	MissionFlow = NewObject<UMissionFlowController>(this, TEXT("MissionFlow"));

	if (UMissionSubsystem* MissionSubsystem = UMissionSubsystem::Get(this))
	{
		MissionSubsystem->OnMissionComplete.AddDynamic(this, &AHawkeyePlayerController::HandleMissionComplete);
		MissionSubsystem->OnFlashbackRequested.AddDynamic(this, &AHawkeyePlayerController::HandleFlashbackRequested);
	}

	AddPauseMappingContext();
	CreateHud();
}

void AHawkeyePlayerController::AddPauseMappingContext()
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

void AHawkeyePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!PauseAction && !SwitchCharacterAction && !PartnerMarkAction)
	{
		return;
	}

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInput)
	{
		UE_LOG(LogHawkeye, Error,
			TEXT("%s: expected an EnhancedInputComponent; the pause key will not work."), *GetName());
		return;
	}

	if (PauseAction)
	{
		EnhancedInput->BindAction(PauseAction, ETriggerEvent::Started, this, &AHawkeyePlayerController::Input_Pause);
	}
	// On the controller, like pause: switching moves between pawns, so neither pawn can own it.
	if (SwitchCharacterAction)
	{
		EnhancedInput->BindAction(SwitchCharacterAction, ETriggerEvent::Started, this,
			&AHawkeyePlayerController::Input_SwitchCharacter);
	}
	if (PartnerMarkAction)
	{
		EnhancedInput->BindAction(PartnerMarkAction, ETriggerEvent::Started, this, &AHawkeyePlayerController::Input_PartnerMark);
	}
}

bool AHawkeyePlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	// Every key reaches here regardless of what (if anything) it is bound to, so this is the one
	// place that reliably knows which device was touched last.
	bUsingGamepad = Params.Key.IsGamepadKey();

	return Super::InputKey(Params);
}

void AHawkeyePlayerController::Input_Pause(const FInputActionValue& /*Value*/)
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

void AHawkeyePlayerController::ToggleInventory()
{
	SetInventoryOpen(!bInventoryOpen);
}

void AHawkeyePlayerController::SetInventoryOpen(bool bOpen)
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
			UE_LOG(LogHawkeye, Warning, TEXT("%s has no InventoryWidgetClass set."), *GetName());
			return;
		}

		if (!InventoryWidget)
		{
			InventoryWidget = CreateWidget<UHawkeyeInventoryWidget>(this, InventoryWidgetClass);
			if (!InventoryWidget)
			{
				UE_LOG(LogHawkeye, Warning, TEXT("%s: could not create the inventory widget."), *GetName());
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

bool AHawkeyePlayerController::CanTogglePause() const
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

void AHawkeyePlayerController::TogglePause()
{
	if (!CanTogglePause())
	{
		UE_LOG(LogHawkeye, Verbose, TEXT("%s: pause ignored (a flashback is playing, or the player is dead)."), *GetName());
		return;
	}

	SetPauseMenuOpen(!bPauseMenuOpen);
}

void AHawkeyePlayerController::SetPauseMenuOpen(bool bOpen)
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

void AHawkeyePlayerController::OpenSettings()
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

void AHawkeyePlayerController::CloseSettings()
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

UHawkeyePauseWidget* AHawkeyePlayerController::ShowPauseWidget()
{
	if (!PauseWidgetClass || !IsLocalController())
	{
		// Pausing still works; there is simply no menu drawn over it.
		UE_LOG(LogHawkeye, Warning, TEXT("%s has no PauseWidgetClass set."), *GetName());
		return nullptr;
	}

	if (!PauseWidget)
	{
		PauseWidget = CreateWidget<UHawkeyePauseWidget>(this, PauseWidgetClass);
		if (!PauseWidget)
		{
			UE_LOG(LogHawkeye, Warning, TEXT("%s: could not create the pause widget."), *GetName());
			return nullptr;
		}

		PauseWidget->OnResumeClicked.AddDynamic(this, &AHawkeyePlayerController::HandlePauseResumeClicked);
		PauseWidget->OnSettingsClicked.AddDynamic(this, &AHawkeyePlayerController::HandlePauseSettingsClicked);
		PauseWidget->OnRestartMissionClicked.AddDynamic(this, &AHawkeyePlayerController::HandlePauseRestartClicked);
		PauseWidget->OnQuitToDesktopClicked.AddDynamic(this, &AHawkeyePlayerController::HandlePauseQuitClicked);
	}

	if (!PauseWidget->IsInViewport())
	{
		PauseWidget->AddToViewport(10);
	}
	PauseWidget->SetVisibility(ESlateVisibility::Visible);

	return PauseWidget;
}

void AHawkeyePlayerController::HidePauseWidget()
{
	if (PauseWidget)
	{
		PauseWidget->RemoveFromParent();
	}
}

UHawkeyeSettingsWidget* AHawkeyePlayerController::ShowSettingsWidget()
{
	if (!SettingsWidgetClass || !IsLocalController())
	{
		UE_LOG(LogHawkeye, Warning, TEXT("%s has no SettingsWidgetClass set."), *GetName());
		return nullptr;
	}

	if (!SettingsWidget)
	{
		SettingsWidget = CreateWidget<UHawkeyeSettingsWidget>(this, SettingsWidgetClass);
		if (!SettingsWidget)
		{
			UE_LOG(LogHawkeye, Warning, TEXT("%s: could not create the settings widget."), *GetName());
			return nullptr;
		}

		SettingsWidget->OnBackRequested.AddDynamic(this, &AHawkeyePlayerController::HandleSettingsBackRequested);
	}

	if (!SettingsWidget->IsInViewport())
	{
		SettingsWidget->AddToViewport(11);
	}
	SettingsWidget->SetVisibility(ESlateVisibility::Visible);
	SettingsWidget->RefreshFromSettings();

	return SettingsWidget;
}

void AHawkeyePlayerController::HideSettingsWidget()
{
	if (SettingsWidget)
	{
		SettingsWidget->RemoveFromParent();
	}
}

TSharedPtr<SWidget> AHawkeyePlayerController::GetFocusedMenuWidget() const
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

void AHawkeyePlayerController::ApplyPauseInputMode(bool bPaused)
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

void AHawkeyePlayerController::HandlePauseResumeClicked()
{
	SetPauseMenuOpen(false);
}

void AHawkeyePlayerController::HandlePauseSettingsClicked()
{
	OpenSettings();
}

void AHawkeyePlayerController::HandleSettingsBackRequested()
{
	CloseSettings();
}

void AHawkeyePlayerController::HandlePauseRestartClicked()
{
	RestartMissionFromPause();
}

void AHawkeyePlayerController::HandlePauseQuitClicked()
{
	QuitToDesktop();
}

void AHawkeyePlayerController::RestartMissionFromPause()
{
	// Unpause first: the level travel and anything it triggers should run on a live world.
	SetPauseMenuOpen(false);

	if (AHawkeyeGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHawkeyeGameMode>() : nullptr)
	{
		GameMode->RestartMission(0.f);
	}
}

void AHawkeyePlayerController::QuitToDesktop()
{
	SetPauseMenuOpen(false);
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, /*bIgnorePlatformRestrictions=*/false);
}

void AHawkeyePlayerController::CreateHud()
{
	if (HudWidget || !HudWidgetClass || !IsLocalController())
	{
		return;
	}

	HudWidget = CreateWidget<UHawkeyeHudWidget>(this, HudWidgetClass);
	if (!HudWidget)
	{
		UE_LOG(LogHawkeye, Warning, TEXT("%s: could not create the HUD widget."), *GetName());
		return;
	}

	HudWidget->AddToViewport(0);
}

void AHawkeyePlayerController::SetHudVisible(bool bVisible)
{
	if (HudWidget)
	{
		HudWidget->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

UHawkeyeHudWidget* AHawkeyePlayerController::GetHawkeyeHudFor(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const AHawkeyePlayerController* PC = World ? Cast<AHawkeyePlayerController>(World->GetFirstPlayerController()) : nullptr;
	return PC ? PC->GetHawkeyeHud() : nullptr;
}

void AHawkeyePlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UMissionSubsystem* MissionSubsystem = UMissionSubsystem::Get(this))
	{
		MissionSubsystem->OnMissionComplete.RemoveDynamic(this, &AHawkeyePlayerController::HandleMissionComplete);
		MissionSubsystem->OnFlashbackRequested.RemoveDynamic(this, &AHawkeyePlayerController::HandleFlashbackRequested);
	}

	if (EndCardWidget)
	{
		EndCardWidget->OnEndCardFinished.RemoveDynamic(this, &AHawkeyePlayerController::HandleEndCardFinished);
		EndCardWidget->RemoveFromParent();
		EndCardWidget = nullptr;
	}

	if (ActiveFlashbackWidget)
	{
		ActiveFlashbackWidget->OnFlashbackFinished.RemoveDynamic(this, &AHawkeyePlayerController::HandleFlashbackFinished);
		ActiveFlashbackWidget = nullptr;
	}

	if (SettingsWidget)
	{
		SettingsWidget->OnBackRequested.RemoveDynamic(this, &AHawkeyePlayerController::HandleSettingsBackRequested);
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
		PauseWidget->OnResumeClicked.RemoveDynamic(this, &AHawkeyePlayerController::HandlePauseResumeClicked);
		PauseWidget->OnSettingsClicked.RemoveDynamic(this, &AHawkeyePlayerController::HandlePauseSettingsClicked);
		PauseWidget->OnRestartMissionClicked.RemoveDynamic(this, &AHawkeyePlayerController::HandlePauseRestartClicked);
		PauseWidget->OnQuitToDesktopClicked.RemoveDynamic(this, &AHawkeyePlayerController::HandlePauseQuitClicked);
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

void AHawkeyePlayerController::HandleFlashbackRequested(UFlashbackDefinition* Flashback)
{
	// This arrives in the same frame as OnMissionComplete, while the end card is still up, so
	// the definition is only cached here; the flow plays it when it reaches the Flashback beat.
	PendingFlashback = Flashback;
}

UFlashbackWidget* AHawkeyePlayerController::PlayFlashback(UFlashbackDefinition* Flashback)
{
	if (!Flashback)
	{
		return nullptr;
	}

	// Escape belongs to the slideshow from here until OnFlashbackFinished.
	bFlashbackActive = true;

	if (!FlashbackWidgetClass)
	{
		UE_LOG(LogHawkeye, Warning,
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

	ActiveFlashbackWidget->OnFlashbackFinished.AddDynamic(this, &AHawkeyePlayerController::HandleFlashbackFinished);
	ActiveFlashbackWidget->Play(Flashback);

	return ActiveFlashbackWidget;
}

void AHawkeyePlayerController::HandleFlashbackFinished(UFlashbackDefinition* /*Flashback*/)
{
	if (ActiveFlashbackWidget)
	{
		ActiveFlashbackWidget->OnFlashbackFinished.RemoveDynamic(this, &AHawkeyePlayerController::HandleFlashbackFinished);
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

void AHawkeyePlayerController::HandleMissionComplete(UMissionDefinition* Mission)
{
	if (!MissionFlow)
	{
		return;
	}

	if (MissionFlow->IsRunning())
	{
		UE_LOG(LogHawkeye, Warning,
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

void AHawkeyePlayerController::PerformCurrentFlowStep()
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
			UE_LOG(LogHawkeye, Warning, TEXT("%s: the mission's flashback could not be loaded."), *GetName());
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
			UE_LOG(LogHawkeye, Warning, TEXT("%s: no next level to travel to after all."), *GetName());
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

UMissionEndCardWidget* AHawkeyePlayerController::ShowEndCard(UMissionDefinition* Mission, bool bWaitForInput)
{
	if (!EndCardWidgetClass || !IsLocalController())
	{
		UE_LOG(LogHawkeye, Warning,
			TEXT("%s has no EndCardWidgetClass set; skipping the end card."), *GetName());
		HandleEndCardFinished(Mission);
		return nullptr;
	}

	if (!EndCardWidget)
	{
		EndCardWidget = CreateWidget<UMissionEndCardWidget>(this, EndCardWidgetClass);
		if (!EndCardWidget)
		{
			UE_LOG(LogHawkeye, Warning, TEXT("%s: could not create the end card widget."), *GetName());
			HandleEndCardFinished(Mission);
			return nullptr;
		}

		EndCardWidget->OnEndCardFinished.AddDynamic(this, &AHawkeyePlayerController::HandleEndCardFinished);
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

void AHawkeyePlayerController::HideEndCard()
{
	if (EndCardWidget)
	{
		EndCardWidget->RemoveFromParent();
	}
}

void AHawkeyePlayerController::HandleEndCardFinished(UMissionDefinition* /*Mission*/)
{
	if (!MissionFlow)
	{
		return;
	}

	MissionFlow->Advance();
	PerformCurrentFlowStep();
}

void AHawkeyePlayerController::OpenMenuLevel()
{
	// L_MainMenu is a stage-5 asset; until it exists, land somewhere that loads rather than
	// throwing the player at a map name the engine cannot resolve.
	FName Target = MenuLevelName;
	if (Target.IsNone() || !FPackageName::DoesPackageExist(Target.ToString()))
	{
		UE_LOG(LogHawkeye, Log, TEXT("%s: %s does not exist; falling back to %s."),
			*GetName(), *Target.ToString(), *FallbackMenuLevelName.ToString());
		Target = FallbackMenuLevelName;
	}

	UE_LOG(LogHawkeye, Log, TEXT("Campaign over; returning to %s."), *Target.ToString());
	UGameplayStatics::OpenLevel(this, Target);
}

bool AHawkeyePlayerController::TryOpenNextLevel()
{
	const UMissionSubsystem* MissionSubsystem = UMissionSubsystem::Get(this);
	const UMissionDefinition* Mission = MissionSubsystem ? MissionSubsystem->GetCurrentMission() : nullptr;

	if (!Mission || Mission->NextLevel.IsNull())
	{
		return false;
	}

	UE_LOG(LogHawkeye, Log, TEXT("Travelling to next level: %s"), *Mission->NextLevel.ToString());
	UGameplayStatics::OpenLevelBySoftObjectPtr(this, Mission->NextLevel);
	return true;
}

// --- Partner and switching ----------------------------------------------------------------------------

void AHawkeyePlayerController::Input_SwitchCharacter(const FInputActionValue& /*Value*/)
{
	SwitchCharacter();
}

void AHawkeyePlayerController::Input_PartnerMark(const FInputActionValue& /*Value*/)
{
	MarkPoint();
}

bool AHawkeyePlayerController::IsSwitchingAllowed() const
{
	if (bAllowSwitchingOverride)
	{
		return true;
	}
	const UMissionSubsystem* Missions = UMissionSubsystem::Get(this);
	const UMissionDefinition* Mission = Missions ? Missions->GetCurrentMission() : nullptr;
	return Mission && Mission->bAllowSwitching;
}

AHawkeyePartnerController* AHawkeyePlayerController::FindPartnerController() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	AHawkeyePartnerController* Any = nullptr;
	for (TActorIterator<AHawkeyePartnerController> It(World); It; ++It)
	{
		if (!It->GetPartner())
		{
			continue;
		}
		if (It->GetLeader() == GetPawn())
		{
			return *It;
		}
		Any = Any ? Any : *It;
	}
	return Any;
}

FString AHawkeyePlayerController::GetSwitchRefusal() const
{
	if (!IsSwitchingAllowed())
	{
		return TEXT("this chapter does not allow switching");
	}
	const AHawkeyeCharacter* Current = Cast<AHawkeyeCharacter>(GetPawn());
	if (!Current)
	{
		return TEXT("no Hawkeye to switch from");
	}
	const AHawkeyePartnerController* Partner = FindPartnerController();
	const AHawkeyeCharacter* Other = Partner ? Partner->GetPartner() : nullptr;
	if (!Other)
	{
		return TEXT("no partner to switch to");
	}
	const FString Blocker = Current->GetSwitchBlocker();
	if (!Blocker.IsEmpty())
	{
		return FString::Printf(TEXT("%s is %s"), *Current->GetCharacterName().ToString(), *Blocker);
	}
	const FString OtherBlocker = Other->GetSwitchBlocker();
	if (!OtherBlocker.IsEmpty())
	{
		return FString::Printf(TEXT("%s is %s"), *Other->GetCharacterName().ToString(), *OtherBlocker);
	}
	return FString();
}

bool AHawkeyePlayerController::SwitchCharacter()
{
	const FString Refusal = GetSwitchRefusal();
	if (!Refusal.IsEmpty())
	{
		UE_LOG(LogHawkeye, Log, TEXT("%s: switch refused: %s."), *GetName(), *Refusal);
		return false;
	}

	AHawkeyeCharacter* Previous = Cast<AHawkeyeCharacter>(GetPawn());
	AHawkeyePartnerController* Partner = FindPartnerController();
	AHawkeyeCharacter* Next = Partner->GetPartner();
	const FRotator View = GetControlRotation();

	// Whatever the thumbs were holding on the old body is let go: no sprint or draw carries over.
	Previous->ReleaseHeldInputs();
	Partner->UnPossess();
	Possess(Next);
	SetControlRotation(FRotator(View.Pitch, View.Yaw, 0.f));
	Partner->Possess(Previous);
	Partner->SetLeader(Next);

	// Possess snapped the view to the new pawn; start from the old one's camera and blend across.
	if (SwitchBlendSeconds > 0.f && IsLocalController())
	{
		SetViewTarget(Previous);
		SetViewTargetWithBlend(Next, SwitchBlendSeconds, VTBlend_Cubic);
	}

	ActiveCharacterName = Next->GetCharacterName();
	if (HudWidget)
	{
		HudWidget->RebindToPawn();
		HudWidget->SetCharacterName(ActiveCharacterName);
	}
	UE_LOG(LogHawkeye, Log, TEXT("%s: switched from %s to %s; %s now follows."), *GetName(),
		*Previous->GetCharacterName().ToString(), *ActiveCharacterName.ToString(), *Previous->GetCharacterName().ToString());
	return true;
}

FText AHawkeyePlayerController::GetHudCharacterName() const
{
	if (!ActiveCharacterName.IsEmpty())
	{
		return ActiveCharacterName;
	}
	const AHawkeyeCharacter* Current = Cast<AHawkeyeCharacter>(GetPawn());
	return Current ? Current->GetCharacterName() : FText::GetEmpty();
}

bool AHawkeyePlayerController::MarkPoint()
{
	AHawkeyePartnerController* Partner = FindPartnerController();
	UWorld* World = GetWorld();
	if (!Partner || !World)
	{
		return false;
	}
	FVector ViewLocation;
	FRotator ViewRotation;
	GetPlayerViewPoint(ViewLocation, ViewRotation);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HawkeyeMarkPoint), false, GetPawn());
	Params.AddIgnoredActor(Partner->GetPawn());
	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, ViewLocation, ViewLocation + ViewRotation.Vector() * MarkTraceDistance,
			ECC_Visibility, Params))
	{
		UE_LOG(LogHawkeye, Log, TEXT("%s: nothing under the view to mark."), *GetName());
		return false;
	}
	FVector Point = Hit.ImpactPoint;
	FNavLocation OnNav;
	if (const UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(World);
		Nav && Nav->ProjectPointToNavigation(Point, OnNav, FVector(200.f, 200.f, 300.f)))
	{
		Point = OnNav.Location;
	}
	Partner->CommandMoveTo(Point);
	return true;
}

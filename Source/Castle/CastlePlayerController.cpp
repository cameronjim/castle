// Copyright Epic Games, Inc. All Rights Reserved.

#include "CastlePlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Castle.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Flashback/FlashbackDefinition.h"
#include "Flashback/FlashbackWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Mission/MissionDefinition.h"
#include "Mission/MissionSubsystem.h"
#include "UI/CastleHudWidget.h"

void ACastlePlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (UMissionSubsystem* MissionSubsystem = UMissionSubsystem::Get(this))
	{
		MissionSubsystem->OnFlashbackRequested.AddDynamic(this, &ACastlePlayerController::HandleFlashbackRequested);
	}

	CreateHud();
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

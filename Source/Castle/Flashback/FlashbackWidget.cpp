// Copyright Epic Games, Inc. All Rights Reserved.

#include "Flashback/FlashbackWidget.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Castle.h"
#include "Components/AudioComponent.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Flashback/FlashbackDefinition.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

void UFlashbackWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Needed so NativeOnKeyDown receives the skip key.
	SetIsFocusable(true);

	if (SlideImageA)
	{
		SlideImageA->SetRenderOpacity(0.f);
	}
	if (SlideImageB)
	{
		SlideImageB->SetRenderOpacity(0.f);
	}
}

void UFlashbackWidget::NativeDestruct()
{
	if (bIsPlaying)
	{
		Finish();
	}

	Super::NativeDestruct();
}

void UFlashbackWidget::Play(UFlashbackDefinition* InFlashback)
{
	if (!InFlashback || InFlashback->Slides.Num() == 0)
	{
		UE_LOG(LogCastle, Warning, TEXT("FlashbackWidget::Play called with an empty flashback."));
		OnFlashbackFinished.Broadcast(InFlashback);
		return;
	}

	Flashback = InFlashback;
	bIsPlaying = true;
	bSlideBIsActive = false;
	CurrentSlideIndex = INDEX_NONE;
	SlideElapsed = 0.f;

	if (!IsInViewport())
	{
		AddToViewport(100);
	}

	bWasPausedBeforePlay = UGameplayStatics::IsGamePaused(this);
	ApplyPlaybackInputMode(true);
	UGameplayStatics::SetGamePaused(this, true);

	if (USoundBase* Ambient = Flashback->AmbientLoop.LoadSynchronous())
	{
		AmbientAudio = PlaySound2DDuringPause(Ambient, /*bLooping=*/true);
	}

	ShowSlide(0);
}

void UFlashbackWidget::ShowSlide(int32 SlideIndex)
{
	if (!Flashback || !Flashback->Slides.IsValidIndex(SlideIndex))
	{
		Finish();
		return;
	}

	const FFlashbackSlide& Slide = Flashback->Slides[SlideIndex];

	CurrentSlideIndex = SlideIndex;
	SlideElapsed = 0.f;
	CurrentHoldSeconds = FMath::Max(Slide.HoldSeconds, 0.1f);
	CurrentCrossfadeSeconds = FMath::Clamp(Slide.CrossfadeSeconds, 0.f, CurrentHoldSeconds);

	// The incoming slide goes on whichever layer is not currently showing.
	const bool bIncomingIsB = !bSlideBIsActive;
	UImage* IncomingImage = bIncomingIsB ? SlideImageB : SlideImageA;
	UImage* OutgoingImage = bIncomingIsB ? SlideImageA : SlideImageB;

	if (IncomingImage)
	{
		if (UTexture2D* Texture = Slide.Image.LoadSynchronous())
		{
			IncomingImage->SetBrushFromTexture(Texture, /*bMatchSize=*/false);
		}

		// First slide fades up from black; later slides fade over the outgoing layer.
		IncomingImage->SetRenderOpacity(CurrentCrossfadeSeconds > 0.f ? 0.f : 1.f);
	}

	if (OutgoingImage && CurrentCrossfadeSeconds <= 0.f)
	{
		OutgoingImage->SetRenderOpacity(0.f);
	}

	bSlideBIsActive = bIncomingIsB;

	if (CaptionText)
	{
		CaptionText->SetText(Slide.Caption);
	}

	if (USoundBase* VoiceLine = Slide.VoiceLine.LoadSynchronous())
	{
		if (VoiceAudio)
		{
			VoiceAudio->Stop();
		}
		VoiceAudio = PlaySound2DDuringPause(VoiceLine, /*bLooping=*/false);
	}

	OnSlideStarted(SlideIndex);
}

void UFlashbackWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bIsPlaying || !Flashback)
	{
		return;
	}

	SlideElapsed += InDeltaTime;

	// Drive the crossfade between the two image layers.
	if (CurrentCrossfadeSeconds > 0.f && SlideElapsed <= CurrentCrossfadeSeconds)
	{
		const float Alpha = FMath::Clamp(SlideElapsed / CurrentCrossfadeSeconds, 0.f, 1.f);
		UImage* IncomingImage = bSlideBIsActive ? SlideImageB : SlideImageA;
		UImage* OutgoingImage = bSlideBIsActive ? SlideImageA : SlideImageB;

		if (IncomingImage)
		{
			IncomingImage->SetRenderOpacity(Alpha);
		}
		if (OutgoingImage)
		{
			OutgoingImage->SetRenderOpacity(1.f - Alpha);
		}
	}

	if (SlideElapsed >= CurrentHoldSeconds)
	{
		const int32 NextSlide = CurrentSlideIndex + 1;
		if (Flashback->Slides.IsValidIndex(NextSlide))
		{
			ShowSlide(NextSlide);
		}
		else
		{
			Finish();
		}
	}
}

void UFlashbackWidget::Skip()
{
	if (bIsPlaying && Flashback && Flashback->bSkippable)
	{
		Finish();
	}
}

void UFlashbackWidget::Finish()
{
	if (!bIsPlaying)
	{
		return;
	}

	bIsPlaying = false;

	if (AmbientAudio)
	{
		AmbientAudio->Stop();
		AmbientAudio = nullptr;
	}
	if (VoiceAudio)
	{
		VoiceAudio->Stop();
		VoiceAudio = nullptr;
	}

	if (!bWasPausedBeforePlay)
	{
		UGameplayStatics::SetGamePaused(this, false);
	}
	ApplyPlaybackInputMode(false);

	UFlashbackDefinition* FinishedFlashback = Flashback;
	Flashback = nullptr;
	CurrentSlideIndex = INDEX_NONE;

	RemoveFromParent();

	OnFlashbackFinished.Broadcast(FinishedFlashback);
}

FReply UFlashbackWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (bIsPlaying && Flashback && Flashback->bSkippable)
	{
		Skip();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UFlashbackWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bIsPlaying && Flashback && Flashback->bSkippable)
	{
		Skip();
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UFlashbackWidget::ApplyPlaybackInputMode(bool bEnable)
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	if (bEnable)
	{
		bRestoreCursor = PC->bShowMouseCursor;
		PC->bShowMouseCursor = true;

		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);

		SetKeyboardFocus();
	}
	else
	{
		PC->bShowMouseCursor = bRestoreCursor;
		PC->SetInputMode(FInputModeGameOnly());
	}
}

UAudioComponent* UFlashbackWidget::PlaySound2DDuringPause(USoundBase* Sound, bool bLooping)
{
	if (!Sound)
	{
		return nullptr;
	}

	// CreateSound2D does not auto-play, which lets us flag the component as a UI sound first so it
	// keeps playing while the game is paused.
	UAudioComponent* AudioComponent = UGameplayStatics::CreateSound2D(
		this, Sound, 1.f, 1.f, 0.f, nullptr, /*bPersistAcrossLevelTransition=*/false, /*bAutoDestroy=*/false);

	if (AudioComponent)
	{
		AudioComponent->bIsUISound = true;
		AudioComponent->bAutoDestroy = !bLooping;
		AudioComponent->Play();
	}

	return AudioComponent;
}

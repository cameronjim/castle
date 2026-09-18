// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FlashbackWidget.generated.h"

class UImage;
class UTextBlock;
class UAudioComponent;
class UFlashbackDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFlashbackFinishedSignature, UFlashbackDefinition*, Flashback);

/**
 * Full-screen crossfading slideshow.
 *
 * Reparent a UMG widget to this class and (optionally) name widgets SlideImageA, SlideImageB and
 * CaptionText to have them driven automatically. Stack SlideImageA under SlideImageB in an Overlay.
 *
 * Timing is driven from NativeTick rather than FTimerManager because the widget pauses the game
 * while it plays, and paused worlds do not tick their timer manager.
 */
UCLASS(Blueprintable, BlueprintType)
class CASTLE_API UFlashbackWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Adds the widget to the viewport (if needed), pauses the game and starts the slideshow. */
	UFUNCTION(BlueprintCallable, Category = "Flashback")
	void Play(UFlashbackDefinition* InFlashback);

	/** Ends playback early; does nothing if the flashback is not skippable. */
	UFUNCTION(BlueprintCallable, Category = "Flashback")
	void Skip();

	/** Stops playback, restores input/pause state and broadcasts OnFlashbackFinished. */
	UFUNCTION(BlueprintCallable, Category = "Flashback")
	void Finish();

	UFUNCTION(BlueprintPure, Category = "Flashback")
	bool IsPlaying() const { return bIsPlaying; }

	UPROPERTY(BlueprintAssignable, Category = "Flashback")
	FOnFlashbackFinishedSignature OnFlashbackFinished;

	/** Blueprint hook fired each time a new slide starts (for extra VFX, letterboxing, etc.). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Flashback")
	void OnSlideStarted(int32 SlideIndex);

protected:
	//~ Begin UUserWidget interface
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	//~ End UUserWidget interface

	/** Shows the slide at SlideIndex on the inactive image layer and begins the crossfade. */
	void ShowSlide(int32 SlideIndex);

	void ApplyPlaybackInputMode(bool bEnable);

	UAudioComponent* PlaySound2DDuringPause(USoundBase* Sound, bool bLooping);

	/** Bottom image layer. */
	UPROPERTY(BlueprintReadOnly, Category = "Flashback", meta = (BindWidgetOptional))
	TObjectPtr<UImage> SlideImageA = nullptr;

	/** Top image layer; the two swap every slide to produce the crossfade. */
	UPROPERTY(BlueprintReadOnly, Category = "Flashback", meta = (BindWidgetOptional))
	TObjectPtr<UImage> SlideImageB = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Flashback", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CaptionText = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Flashback")
	TObjectPtr<UFlashbackDefinition> Flashback = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> AmbientAudio = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> VoiceAudio = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Flashback")
	int32 CurrentSlideIndex = INDEX_NONE;

private:
	/** Layer currently holding the visible slide: false = SlideImageA, true = SlideImageB. */
	bool bSlideBIsActive = false;

	bool bIsPlaying = false;

	/** Seconds spent on the current slide. */
	float SlideElapsed = 0.f;

	float CurrentHoldSeconds = 0.f;
	float CurrentCrossfadeSeconds = 0.f;

	/** Pause/cursor state captured on Play so Finish can restore it. */
	bool bRestoreCursor = false;
	bool bWasPausedBeforePlay = false;
};

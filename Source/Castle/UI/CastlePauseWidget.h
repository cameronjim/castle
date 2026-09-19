// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CastlePauseWidget.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPauseMenuChoiceSignature);

/**
 * The Escape menu: Resume, Settings, Restart mission, Quit to desktop.
 *
 * Reparent a UMG widget to this class and name four buttons ResumeButton, SettingsButton,
 * RestartMissionButton and QuitToDesktopButton to have them driven automatically. A subclass
 * with no designer layout works too, because RebuildWidget builds a vertical stack itself
 * (the same approach as UCastleHudWidget and UFlashbackWidget).
 *
 * The widget only reports which button was pressed; ACastlePlayerController decides what each
 * one means, so the menu has no opinion about pausing, travel or quitting.
 */
UCLASS(Blueprintable, BlueprintType)
class CASTLE_API UCastlePauseWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Pause")
	FOnPauseMenuChoiceSignature OnResumeClicked;

	UPROPERTY(BlueprintAssignable, Category = "Pause")
	FOnPauseMenuChoiceSignature OnSettingsClicked;

	UPROPERTY(BlueprintAssignable, Category = "Pause")
	FOnPauseMenuChoiceSignature OnRestartMissionClicked;

	UPROPERTY(BlueprintAssignable, Category = "Pause")
	FOnPauseMenuChoiceSignature OnQuitToDesktopClicked;

	/** Label text, so the three buttons can be renamed without touching the layout code. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pause")
	FText TitleLabel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pause")
	FText ResumeLabel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pause")
	FText SettingsLabel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pause")
	FText RestartMissionLabel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pause")
	FText QuitToDesktopLabel;

protected:
	//~ Begin UUserWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End UUserWidget interface

	UFUNCTION()
	void HandleResumeClicked();

	UFUNCTION()
	void HandleSettingsClicked();

	UFUNCTION()
	void HandleRestartMissionClicked();

	UFUNCTION()
	void HandleQuitToDesktopClicked();

	/** Fills in any label the designer left empty. Called before the layout is built. */
	void ApplyDefaultLabels();

	UPROPERTY(BlueprintReadOnly, Category = "Pause", meta = (BindWidgetOptional))
	TObjectPtr<UButton> ResumeButton = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Pause", meta = (BindWidgetOptional))
	TObjectPtr<UButton> SettingsButton = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Pause", meta = (BindWidgetOptional))
	TObjectPtr<UButton> RestartMissionButton = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Pause", meta = (BindWidgetOptional))
	TObjectPtr<UButton> QuitToDesktopButton = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Pause", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText = nullptr;

	/** The stack RebuildWidget builds when the Blueprint has no layout of its own. */
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ButtonStack = nullptr;

private:
	bool bBound = false;
};

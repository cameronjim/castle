// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HawkeyeSettingsWidget.generated.h"

class UButton;
class UCheckBox;
class UHorizontalBox;
class USlider;
class UTextBlock;
class UVerticalBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSettingsBackSignature);

/**
 * The Settings screen: one option today, mouse sensitivity, and the shape every later one
 * copies - a label, a control, a live value, all driven straight into UHawkeyeSettingsSubsystem.
 *
 * Reparent a UMG widget to this class and name the parts SensitivitySlider,
 * SensitivityValueText and BackButton to have them driven automatically. A subclass with no
 * designer layout works too, because RebuildWidget builds one (the same approach as
 * UHawkeyePauseWidget).
 *
 * The widget owns no settings state. It reads the subsystem on open and writes it on change;
 * AHawkeyePlayerController decides what Back means.
 */
UCLASS(Blueprintable, BlueprintType)
class HAWKEYE_API UHawkeyeSettingsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Settings")
	FOnSettingsBackSignature OnBackRequested;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Settings")
	FText TitleLabel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Settings")
	FText SensitivityLabel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Settings")
	FText StickSensitivityLabel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Settings")
	FText BackLabel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Settings")
	FText InvertMouseYLabel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Settings")
	FText InvertStickYLabel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Settings|Audio")
	FText MasterVolumeLabel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Settings|Audio")
	FText SfxVolumeLabel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Settings|Audio")
	FText AmbientVolumeLabel;

	/** Pulls the slider and the number back in line with the subsystem. Called on construct. */
	UFUNCTION(BlueprintCallable, Category = "Settings")
	void RefreshFromSettings();

protected:
	//~ Begin UUserWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End UUserWidget interface

	UFUNCTION()
	void HandleSensitivityChanged(float Value);

	UFUNCTION()
	void HandleStickSensitivityChanged(float Value);

	UFUNCTION()
	void HandleInvertMouseYChanged(bool bIsChecked);

	UFUNCTION()
	void HandleInvertStickYChanged(bool bIsChecked);

	UFUNCTION()
	void HandleBackClicked();

	UFUNCTION()
	void HandleBackHovered();

	UFUNCTION()
	void HandleMasterVolumeChanged(float Value);

	UFUNCTION()
	void HandleSfxVolumeChanged(float Value);

	UFUNCTION()
	void HandleAmbientVolumeChanged(float Value);

	/** Writes Value (0..1) into Text as a whole percentage. */
	static void UpdateVolumeText(UTextBlock* Text, float Value);

	/** Fills in any label the designer left empty. Called before the layout is built. */
	void ApplyDefaultLabels();

	/** Writes Value into SensitivityValueText as two decimals. */
	void UpdateValueText(float Value);

	/** Writes Value into StickSensitivityValueText as two decimals. */
	void UpdateStickValueText(float Value);

	UPROPERTY(BlueprintReadOnly, Category = "Settings", meta = (BindWidgetOptional))
	TObjectPtr<USlider> SensitivitySlider = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Settings", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SensitivityValueText = nullptr;

	/** Second slider, "Controller sensitivity", directly under the mouse one. */
	UPROPERTY(BlueprintReadOnly, Category = "Settings", meta = (BindWidgetOptional))
	TObjectPtr<USlider> StickSensitivitySlider = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Settings", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StickSensitivityValueText = nullptr;

	/** Checkbox under the sliders: "Invert mouse Y". */
	UPROPERTY(BlueprintReadOnly, Category = "Settings", meta = (BindWidgetOptional))
	TObjectPtr<UCheckBox> InvertMouseYCheckBox = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Settings", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> InvertMouseYLabelText = nullptr;

	/** Checkbox under the sliders: "Invert controller Y". */
	UPROPERTY(BlueprintReadOnly, Category = "Settings", meta = (BindWidgetOptional))
	TObjectPtr<UCheckBox> InvertStickYCheckBox = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Settings", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> InvertStickYLabelText = nullptr;

	/** Three volume rows under the checkboxes: master, sound effects, ambience. */
	UPROPERTY(BlueprintReadOnly, Category = "Settings|Audio", meta = (BindWidgetOptional))
	TObjectPtr<USlider> MasterVolumeSlider = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Settings|Audio", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MasterVolumeValueText = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Settings|Audio", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MasterVolumeLabelText = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Settings|Audio", meta = (BindWidgetOptional))
	TObjectPtr<USlider> SfxVolumeSlider = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Settings|Audio", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SfxVolumeValueText = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Settings|Audio", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SfxVolumeLabelText = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Settings|Audio", meta = (BindWidgetOptional))
	TObjectPtr<USlider> AmbientVolumeSlider = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Settings|Audio", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> AmbientVolumeValueText = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Settings|Audio", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> AmbientVolumeLabelText = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Settings", meta = (BindWidgetOptional))
	TObjectPtr<UButton> BackButton = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Settings", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Settings", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SensitivityLabelText = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Settings", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StickSensitivityLabelText = nullptr;

	/** The stack RebuildWidget builds when the Blueprint has no layout of its own. */
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> OptionStack = nullptr;

private:
	bool bBound = false;
};

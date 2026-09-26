// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HawkeyeSafehouseWidget.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSafehouseChoiceSignature);

/**
 * The safehouse menu: Refill arrows, Save, Fast travel, Chapter select, Leave, with a status line
 * under the title for what the last choice did ("Arrows refilled", "No other safehouse yet").
 *
 * Reports choices only; AHawkeyePlayerController acts on them and writes the status line back.
 */
UCLASS(Blueprintable, BlueprintType)
class HAWKEYE_API UHawkeyeSafehouseWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Safehouse")
	FOnSafehouseChoiceSignature OnRefillClicked;

	UPROPERTY(BlueprintAssignable, Category = "Safehouse")
	FOnSafehouseChoiceSignature OnSaveClicked;

	UPROPERTY(BlueprintAssignable, Category = "Safehouse")
	FOnSafehouseChoiceSignature OnFastTravelClicked;

	UPROPERTY(BlueprintAssignable, Category = "Safehouse")
	FOnSafehouseChoiceSignature OnChapterSelectClicked;

	UPROPERTY(BlueprintAssignable, Category = "Safehouse")
	FOnSafehouseChoiceSignature OnLeaveClicked;

	/** The heading: the safehouse's name. */
	UFUNCTION(BlueprintCallable, Category = "Safehouse")
	void SetSafehouseName(const FText& Name);

	/** The line under the heading. */
	UFUNCTION(BlueprintCallable, Category = "Safehouse")
	void SetStatus(const FText& Status);

	UFUNCTION(BlueprintPure, Category = "Safehouse")
	FText GetStatus() const { return StatusLine; }

protected:
	//~ Begin UUserWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End UUserWidget interface

	UFUNCTION()
	void HandleRefillClicked();

	UFUNCTION()
	void HandleSaveClicked();

	UFUNCTION()
	void HandleFastTravelClicked();

	UFUNCTION()
	void HandleChapterSelectClicked();

	UFUNCTION()
	void HandleLeaveClicked();

	UPROPERTY(BlueprintReadOnly, Category = "Safehouse", meta = (BindWidgetOptional))
	TObjectPtr<UButton> RefillButton = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Safehouse", meta = (BindWidgetOptional))
	TObjectPtr<UButton> SaveButton = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Safehouse", meta = (BindWidgetOptional))
	TObjectPtr<UButton> FastTravelButton = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Safehouse", meta = (BindWidgetOptional))
	TObjectPtr<UButton> ChapterSelectButton = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Safehouse", meta = (BindWidgetOptional))
	TObjectPtr<UButton> LeaveButton = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Safehouse", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Safehouse", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText = nullptr;

private:
	FText SafehouseName;
	FText StatusLine;
	bool bBound = false;
};

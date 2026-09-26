// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HawkeyeInventoryWidget.generated.h"

class UBorder;
class UInventoryComponent;
class UTextBlock;
class UVerticalBox;

/** One line of the inventory screen: a section heading, or an item under one. */
USTRUCT(BlueprintType)
struct HAWKEYE_API FHawkeyeInventoryRow
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|UI")
	FText Text;

	/** Headings are drawn dim and unindented; items are white and indented under them. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|UI")
	bool bHeading = false;
};

/**
 * The Tab screen: everything Kate is carrying, read-only. The bow, the quiver with each slot's
 * number and count, and keycards by id.
 *
 * Like the rest of the UI it builds its own layout, because WBP_Inventory is script-generated
 * and has no designer graph. AHawkeyePlayerController owns it and pauses the game while it is
 * up, the same way the pause menu does.
 */
UCLASS(Blueprintable, BlueprintType)
class HAWKEYE_API UHawkeyeInventoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Binds to Inventory and repaints the rows. Passing null unbinds. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|UI")
	void BindToInventory(UInventoryComponent* Inventory);

	/** Finds the owning pawn's inventory and binds to it. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|UI")
	void BindToOwningPawn();

	UFUNCTION(BlueprintPure, Category = "Inventory|UI")
	UInventoryComponent* GetBoundInventory() const { return BoundInventory; }

	/** Rewrites the row list from the bound inventory. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|UI")
	void RefreshRows();

	/**
	 * Every line the screen shows, in order: the bow, then the quiver, then keycards.
	 * Pure, so a test can assert on the contents without building Slate.
	 */
	UFUNCTION(BlueprintPure, Category = "Inventory|UI")
	TArray<FText> BuildInventoryLines() const;

	/** The same list with the section headings marked, which is what the layout needs. */
	UFUNCTION(BlueprintPure, Category = "Inventory|UI")
	TArray<FHawkeyeInventoryRow> BuildInventoryRows() const;

	/** Colour of a section heading. Rows themselves are left at the text block's default. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Inventory|UI")
	FLinearColor HeadingColor = FLinearColor(0.22f, 1.f, 0.08f, 0.8f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|UI")
	FText TitleLabel;

protected:
	//~ Begin UUserWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End UUserWidget interface

	UFUNCTION()
	void HandleInventoryChanged();

	/** Fills in TitleLabel when the designer left it empty. */
	void ApplyDefaultLabels();

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|UI", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText = nullptr;

	/** The rows themselves. Cleared and rebuilt on every refresh. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|UI", meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> RowBox = nullptr;

	/** Full-screen dimmer, so the frozen game reads as background. */
	UPROPERTY(Transient)
	TObjectPtr<UBorder> Dimmer = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Inventory|UI")
	FLinearColor DimmerColor = FLinearColor(0.f, 0.f, 0.f, 0.7f);

	UPROPERTY(Transient)
	TObjectPtr<UInventoryComponent> BoundInventory = nullptr;
};

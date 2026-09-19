// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CastleInventoryWidget.generated.h"

class UBorder;
class UInventoryComponent;
class UTextBlock;
class UVerticalBox;

/**
 * The Tab screen: everything Frank is carrying, read-only. Weapons with their slot number and
 * ammo, keycards by id, spare rounds per weapon.
 *
 * Like the rest of the UI it builds its own layout, because WBP_Inventory is script-generated
 * and has no designer graph. ACastlePlayerController owns it and pauses the game while it is
 * up, the same way the pause menu does.
 */
UCLASS(Blueprintable, BlueprintType)
class CASTLE_API UCastleInventoryWidget : public UUserWidget
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
	 * Every line the screen shows, in order: weapons, then keycards, then spare ammo.
	 * Pure, so a test can assert on the contents without building Slate.
	 */
	UFUNCTION(BlueprintPure, Category = "Inventory|UI")
	TArray<FText> BuildInventoryLines() const;

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

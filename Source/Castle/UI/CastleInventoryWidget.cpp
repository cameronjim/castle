// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/CastleInventoryWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Combat/WeaponDefinition.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Player/InventoryComponent.h"

void UCastleInventoryWidget::ApplyDefaultLabels()
{
	if (TitleLabel.IsEmpty())
	{
		TitleLabel = NSLOCTEXT("Castle", "InventoryTitle", "Inventory");
	}
}

TSharedRef<SWidget> UCastleInventoryWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		ApplyDefaultLabels();

		UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("InventoryRoot"));
		WidgetTree->RootWidget = Root;

		// A colour brush, not the default one: UBorder ships with a brush that has no image
		// resource, so a plain SetBrushColor draws nothing at all.
		Dimmer = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Dimmer"));
		Dimmer->SetBrush(FSlateColorBrush(FLinearColor::White));
		Dimmer->SetBrushColor(DimmerColor);
		if (UOverlaySlot* DimmerSlot = Cast<UOverlaySlot>(Root->AddChild(Dimmer)))
		{
			DimmerSlot->SetHorizontalAlignment(HAlign_Fill);
			DimmerSlot->SetVerticalAlignment(VAlign_Fill);
		}

		UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Stack"));
		if (UOverlaySlot* StackSlot = Cast<UOverlaySlot>(Root->AddChild(Stack)))
		{
			StackSlot->SetHorizontalAlignment(HAlign_Center);
			StackSlot->SetVerticalAlignment(VAlign_Center);
		}

		if (!TitleText)
		{
			TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
		}
		TitleText->SetText(TitleLabel);
		if (UVerticalBoxSlot* TitleSlot = Cast<UVerticalBoxSlot>(Stack->AddChild(TitleText)))
		{
			TitleSlot->SetHorizontalAlignment(HAlign_Center);
			TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 24.f));
		}

		if (!RowBox)
		{
			RowBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RowBox"));
		}
		if (UVerticalBoxSlot* RowSlot = Cast<UVerticalBoxSlot>(Stack->AddChild(RowBox)))
		{
			RowSlot->SetHorizontalAlignment(HAlign_Left);
		}

		RefreshRows();
	}

	return Super::RebuildWidget();
}

void UCastleInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ApplyDefaultLabels();
	BindToOwningPawn();
	RefreshRows();
}

void UCastleInventoryWidget::NativeDestruct()
{
	BindToInventory(nullptr);

	Super::NativeDestruct();
}

void UCastleInventoryWidget::BindToOwningPawn()
{
	const APlayerController* PC = GetOwningPlayer();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	BindToInventory(Pawn ? Pawn->FindComponentByClass<UInventoryComponent>() : nullptr);
}

void UCastleInventoryWidget::BindToInventory(UInventoryComponent* Inventory)
{
	if (BoundInventory == Inventory)
	{
		return;
	}

	if (BoundInventory)
	{
		BoundInventory->OnInventoryChanged.RemoveDynamic(this, &UCastleInventoryWidget::HandleInventoryChanged);
	}

	BoundInventory = Inventory;

	if (BoundInventory)
	{
		BoundInventory->OnInventoryChanged.AddDynamic(this, &UCastleInventoryWidget::HandleInventoryChanged);
	}

	RefreshRows();
}

void UCastleInventoryWidget::HandleInventoryChanged()
{
	RefreshRows();
}

TArray<FText> UCastleInventoryWidget::BuildInventoryLines() const
{
	TArray<FText> Lines;
	if (!BoundInventory)
	{
		return Lines;
	}

	Lines.Add(NSLOCTEXT("Castle", "InventoryWeapons", "Weapons"));

	for (int32 Index = 0; Index < CastleHotbarSlotCount; ++Index)
	{
		const EHotbarSlot HotbarSlot = static_cast<EHotbarSlot>(Index);
		const FCastleInventorySlot Entry = BoundInventory->GetSlot(HotbarSlot);
		if (Entry.IsEmpty())
		{
			continue;
		}

		const FString Name = Entry.Weapon->GetDisplayNameOrAssetName().ToString();
		const FString Ammo = Entry.IsRanged()
			? FString::Printf(TEXT("  %d / %d"), Entry.Magazine, Entry.Reserve)
			: FString();
		Lines.Add(FText::FromString(FString::Printf(TEXT("%d. %s%s"), Index + 1, *Name, *Ammo)));
	}

	Lines.Add(NSLOCTEXT("Castle", "InventoryKeycards", "Keycards"));

	TArray<FName> Keycards = BoundInventory->GetKeycards().Array();
	Keycards.Sort(FNameLexicalLess());
	if (Keycards.Num() == 0)
	{
		Lines.Add(NSLOCTEXT("Castle", "InventoryNoKeycards", "none"));
	}
	for (const FName& Keycard : Keycards)
	{
		Lines.Add(FText::FromName(Keycard));
	}

	Lines.Add(NSLOCTEXT("Castle", "InventorySpareAmmo", "Spare ammo"));

	bool bAnySpare = false;
	for (int32 Index = 0; Index < CastleHotbarSlotCount; ++Index)
	{
		const FCastleInventorySlot Entry = BoundInventory->GetSlot(static_cast<EHotbarSlot>(Index));
		if (!Entry.IsRanged())
		{
			continue;
		}
		bAnySpare = true;
		Lines.Add(FText::FromString(FString::Printf(TEXT("%s  %d"),
			*Entry.Weapon->GetDisplayNameOrAssetName().ToString(), Entry.Reserve)));
	}
	if (!bAnySpare)
	{
		Lines.Add(NSLOCTEXT("Castle", "InventoryNoSpareAmmo", "none"));
	}

	return Lines;
}

void UCastleInventoryWidget::RefreshRows()
{
	if (!RowBox || !WidgetTree)
	{
		return;
	}

	RowBox->ClearChildren();

	int32 RowIndex = 0;
	for (const FText& Line : BuildInventoryLines())
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), *FString::Printf(TEXT("Row%d"), RowIndex++));
		Text->SetText(Line);
		if (UVerticalBoxSlot* LineSlot = Cast<UVerticalBoxSlot>(RowBox->AddChild(Text)))
		{
			LineSlot->SetHorizontalAlignment(HAlign_Left);
			LineSlot->SetPadding(FMargin(0.f, 2.f));
		}
	}
}

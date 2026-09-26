// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/HawkeyeInventoryWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Combat/ArrowDefinition.h"
#include "Combat/BowDefinition.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Player/InventoryComponent.h"

void UHawkeyeInventoryWidget::ApplyDefaultLabels()
{
	if (TitleLabel.IsEmpty())
	{
		TitleLabel = NSLOCTEXT("Hawkeye", "InventoryTitle", "Inventory");
	}
}

TSharedRef<SWidget> UHawkeyeInventoryWidget::RebuildWidget()
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

void UHawkeyeInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ApplyDefaultLabels();
	BindToOwningPawn();
	RefreshRows();
}

void UHawkeyeInventoryWidget::NativeDestruct()
{
	BindToInventory(nullptr);

	Super::NativeDestruct();
}

void UHawkeyeInventoryWidget::BindToOwningPawn()
{
	const APlayerController* PC = GetOwningPlayer();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	BindToInventory(Pawn ? Pawn->FindComponentByClass<UInventoryComponent>() : nullptr);
}

void UHawkeyeInventoryWidget::BindToInventory(UInventoryComponent* Inventory)
{
	if (BoundInventory == Inventory)
	{
		return;
	}

	if (BoundInventory)
	{
		BoundInventory->OnInventoryChanged.RemoveDynamic(this, &UHawkeyeInventoryWidget::HandleInventoryChanged);
	}

	BoundInventory = Inventory;

	if (BoundInventory)
	{
		BoundInventory->OnInventoryChanged.AddDynamic(this, &UHawkeyeInventoryWidget::HandleInventoryChanged);
	}

	RefreshRows();
}

void UHawkeyeInventoryWidget::HandleInventoryChanged()
{
	RefreshRows();
}

TArray<FText> UHawkeyeInventoryWidget::BuildInventoryLines() const
{
	TArray<FText> Lines;
	for (const FHawkeyeInventoryRow& Row : BuildInventoryRows())
	{
		Lines.Add(Row.Text);
	}
	return Lines;
}

TArray<FHawkeyeInventoryRow> UHawkeyeInventoryWidget::BuildInventoryRows() const
{
	TArray<FHawkeyeInventoryRow> Lines;
	if (!BoundInventory)
	{
		return Lines;
	}

	auto AddRow = [&Lines](const FText& Text, bool bHeading)
	{
		FHawkeyeInventoryRow Row;
		Row.Text = Text;
		Row.bHeading = bHeading;
		Lines.Add(Row);
	};

	AddRow(NSLOCTEXT("Hawkeye", "InventoryBow", "Bow"), true);

	const UBowDefinition* Bow = BoundInventory->GetBow();
	AddRow(Bow
		? (Bow->DisplayName.IsEmpty() ? FText::FromName(Bow->GetFName()) : Bow->DisplayName)
		: NSLOCTEXT("Hawkeye", "InventoryNoBow", "none (fists)"), false);

	AddRow(NSLOCTEXT("Hawkeye", "InventoryQuiver", "Quiver"), true);

	for (int32 QuiverSlot = 1; QuiverSlot <= HawkeyeQuiverSlotCount; ++QuiverSlot)
	{
		const FHawkeyeQuiverSlot Entry = BoundInventory->GetArrowSlot(QuiverSlot);
		if (Entry.IsEmpty())
		{
			continue;
		}
		AddRow(FText::FromString(FString::Printf(TEXT("%d. %s  %d / %d"), QuiverSlot,
			*Entry.Arrow->GetDisplayNameOrAssetName().ToString(), Entry.Count, Entry.Arrow->Cap)), false);
	}

	AddRow(NSLOCTEXT("Hawkeye", "InventoryKeycards", "Keycards"), true);

	TArray<FName> Keycards = BoundInventory->GetKeycards().Array();
	Keycards.Sort(FNameLexicalLess());
	if (Keycards.Num() == 0)
	{
		AddRow(NSLOCTEXT("Hawkeye", "InventoryNoKeycards", "none"), false);
	}
	for (const FName& Keycard : Keycards)
	{
		AddRow(FText::FromName(Keycard), false);
	}

	return Lines;
}

void UHawkeyeInventoryWidget::RefreshRows()
{
	if (!RowBox || !WidgetTree)
	{
		return;
	}

	RowBox->ClearChildren();

	int32 RowIndex = 0;
	for (const FHawkeyeInventoryRow& Row : BuildInventoryRows())
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), *FString::Printf(TEXT("Row%d"), RowIndex++));
		Text->SetText(Row.Text);

		// Headings sit flush left in the HUD green; the things you are carrying are indented
		// white underneath them, so the screen reads as three lists rather than one block.
		if (Row.bHeading)
		{
			Text->SetColorAndOpacity(FSlateColor(HeadingColor));
		}

		if (UVerticalBoxSlot* LineSlot = Cast<UVerticalBoxSlot>(RowBox->AddChild(Text)))
		{
			LineSlot->SetHorizontalAlignment(HAlign_Left);
			LineSlot->SetPadding(Row.bHeading
				? FMargin(0.f, RowIndex > 1 ? 16.f : 0.f, 0.f, 4.f)
				: FMargin(20.f, 2.f, 0.f, 2.f));
		}
	}
}

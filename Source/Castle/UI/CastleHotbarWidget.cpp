// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/CastleHotbarWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Combat/ArrowDefinition.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Player/InventoryComponent.h"

TSharedRef<SWidget> UCastleHotbarWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		SlotRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SlotRow"));
		WidgetTree->RootWidget = SlotRow;

		SlotBoxes.Reset();
		SlotKeyTexts.Reset();
		SlotNameTexts.Reset();
		SlotCountTexts.Reset();

		for (int32 QuiverSlot = 1; QuiverSlot <= CastleQuiverSlotCount; ++QuiverSlot)
		{
			const FString Suffix = FString::FromInt(QuiverSlot);

			UBorder* Box = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *(TEXT("SlotBox") + Suffix));
			// UBorder's default brush carries no image resource, so SetBrushColor on its own
			// paints nothing. A colour brush always draws, which is what a slot box needs.
			Box->SetBrush(FSlateColorBrush(FLinearColor::White));
			Box->SetPadding(FMargin(8.f, 5.f));

			// A size box so every slot is the same width whatever is written in it.
			USizeBox* Sizer = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *(TEXT("SlotSizer") + Suffix));
			Sizer->SetMinDesiredWidth(SlotWidthPixels);
			if (UBorderSlot* BoxSlot = Cast<UBorderSlot>(Box->AddChild(Sizer)))
			{
				BoxSlot->SetHorizontalAlignment(HAlign_Fill);
				BoxSlot->SetVerticalAlignment(VAlign_Center);
			}

			UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), *(TEXT("SlotStack") + Suffix));
			if (USizeBoxSlot* StackSlot = Cast<USizeBoxSlot>(Sizer->AddChild(Stack)))
			{
				StackSlot->SetHorizontalAlignment(HAlign_Center);
				StackSlot->SetVerticalAlignment(VAlign_Center);
			}

			auto AddLine = [this, Stack](TArray<TObjectPtr<UTextBlock>>& Into, const FString& Name, int32 FontSize)
			{
				UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *Name);
				FSlateFontInfo Font = Text->GetFont();
				Font.Size = FontSize;
				Text->SetFont(Font);
				if (UVerticalBoxSlot* LineSlot = Cast<UVerticalBoxSlot>(Stack->AddChild(Text)))
				{
					LineSlot->SetHorizontalAlignment(HAlign_Center);
				}
				Into.Add(Text);
			};

			AddLine(SlotKeyTexts, TEXT("SlotKey") + Suffix, 10);
			AddLine(SlotNameTexts, TEXT("SlotName") + Suffix, 12);
			AddLine(SlotCountTexts, TEXT("SlotCount") + Suffix, 14);

			if (UHorizontalBoxSlot* RowSlot = Cast<UHorizontalBoxSlot>(SlotRow->AddChild(Box)))
			{
				RowSlot->SetPadding(FMargin(4.f, 0.f));
				RowSlot->SetVerticalAlignment(VAlign_Bottom);
			}

			SlotBoxes.Add(Box);
		}

		RefreshSlots();
	}

	return Super::RebuildWidget();
}

void UCastleHotbarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BindToOwningPawn();
	RefreshSlots();
}

void UCastleHotbarWidget::NativeDestruct()
{
	BindToInventory(nullptr);

	Super::NativeDestruct();
}

void UCastleHotbarWidget::BindToOwningPawn()
{
	const APlayerController* PC = GetOwningPlayer();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	BindToInventory(Pawn ? Pawn->FindComponentByClass<UInventoryComponent>() : nullptr);
}

void UCastleHotbarWidget::BindToInventory(UInventoryComponent* Inventory)
{
	if (BoundInventory == Inventory)
	{
		return;
	}

	if (BoundInventory)
	{
		BoundInventory->OnInventoryChanged.RemoveDynamic(this, &UCastleHotbarWidget::HandleInventoryChanged);
		BoundInventory->OnActiveArrowSlotChanged.RemoveDynamic(this, &UCastleHotbarWidget::HandleActiveSlotChanged);
	}

	BoundInventory = Inventory;

	if (BoundInventory)
	{
		BoundInventory->OnInventoryChanged.AddDynamic(this, &UCastleHotbarWidget::HandleInventoryChanged);
		BoundInventory->OnActiveArrowSlotChanged.AddDynamic(this, &UCastleHotbarWidget::HandleActiveSlotChanged);
	}

	RefreshSlots();
}

void UCastleHotbarWidget::HandleInventoryChanged()
{
	RefreshSlots();
}

void UCastleHotbarWidget::HandleActiveSlotChanged(int32 /*OldSlot*/, int32 /*NewSlot*/)
{
	RefreshSlots();
}

int32 UCastleHotbarWidget::GetActiveSlot() const
{
	return BoundInventory ? BoundInventory->GetActiveArrowSlot() : 1;
}

bool UCastleHotbarWidget::IsSlotActive(int32 QuiverSlot) const
{
	return BoundInventory != nullptr && BoundInventory->GetActiveArrowSlot() == QuiverSlot;
}

bool UCastleHotbarWidget::IsSlotEmpty(int32 QuiverSlot) const
{
	return !BoundInventory || BoundInventory->IsArrowSlotEmpty(QuiverSlot);
}

FText UCastleHotbarWidget::GetSlotKeyText(int32 QuiverSlot)
{
	return FText::AsNumber(QuiverSlot);
}

FText UCastleHotbarWidget::GetSlotNameText(int32 QuiverSlot) const
{
	if (!BoundInventory)
	{
		return FText::GetEmpty();
	}
	const FCastleQuiverSlot Entry = BoundInventory->GetArrowSlot(QuiverSlot);
	return Entry.IsEmpty() ? NSLOCTEXT("Castle", "HotbarEmptySlot", "--") : Entry.Arrow->GetShortNameOrDisplayName();
}

FText UCastleHotbarWidget::GetSlotCountText(int32 QuiverSlot) const
{
	if (!BoundInventory)
	{
		return FText::GetEmpty();
	}
	const FCastleQuiverSlot Entry = BoundInventory->GetArrowSlot(QuiverSlot);
	if (Entry.IsEmpty())
	{
		return FText::GetEmpty();
	}
	// Standard arrows are the plentiful ones; a trick arrow's cap is small enough to be worth showing.
	return QuiverSlot == 1
		? FText::AsNumber(Entry.Count)
		: FText::FromString(FString::Printf(TEXT("%d/%d"), Entry.Count, Entry.Arrow->Cap));
}

FLinearColor UCastleHotbarWidget::GetSlotColor(int32 QuiverSlot) const
{
	if (IsSlotEmpty(QuiverSlot))
	{
		return EmptySlotColor;
	}
	return IsSlotActive(QuiverSlot) ? ActiveSlotColor : FilledSlotColor;
}

void UCastleHotbarWidget::RefreshSlots()
{
	for (int32 Index = 0; Index < CastleQuiverSlotCount; ++Index)
	{
		const int32 QuiverSlot = Index + 1;
		if (SlotBoxes.IsValidIndex(Index) && SlotBoxes[Index])
		{
			SlotBoxes[Index]->SetBrushColor(GetSlotColor(QuiverSlot));
		}
		if (SlotKeyTexts.IsValidIndex(Index) && SlotKeyTexts[Index])
		{
			SlotKeyTexts[Index]->SetText(GetSlotKeyText(QuiverSlot));
		}
		if (SlotNameTexts.IsValidIndex(Index) && SlotNameTexts[Index])
		{
			SlotNameTexts[Index]->SetText(GetSlotNameText(QuiverSlot));
		}
		if (SlotCountTexts.IsValidIndex(Index) && SlotCountTexts[Index])
		{
			SlotCountTexts[Index]->SetText(GetSlotCountText(QuiverSlot));
		}
	}
}

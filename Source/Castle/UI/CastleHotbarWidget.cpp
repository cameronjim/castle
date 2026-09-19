// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/CastleHotbarWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
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
		SlotAmmoTexts.Reset();

		for (int32 Index = 0; Index < CastleHotbarSlotCount; ++Index)
		{
			const FString Suffix = FString::FromInt(Index + 1);

			UBorder* Box = WidgetTree->ConstructWidget<UBorder>(
				UBorder::StaticClass(), *(TEXT("SlotBox") + Suffix));
			// UBorder's default brush carries no image resource, so SetBrushColor on its own
			// paints nothing. A colour brush always draws, which is what a slot box needs.
			Box->SetBrush(FSlateColorBrush(FLinearColor::White));
			Box->SetPadding(FMargin(10.f, 6.f));

			// A size box so all three slots are the same width whatever is written in them;
			// without it the empty slot shrinks to its dash and the bar looks broken.
			USizeBox* Sizer = WidgetTree->ConstructWidget<USizeBox>(
				USizeBox::StaticClass(), *(TEXT("SlotSizer") + Suffix));
			Sizer->SetMinDesiredWidth(SlotWidthPixels);
			if (UBorderSlot* BoxSlot = Cast<UBorderSlot>(Box->AddChild(Sizer)))
			{
				BoxSlot->SetHorizontalAlignment(HAlign_Fill);
				BoxSlot->SetVerticalAlignment(VAlign_Center);
			}

			UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(
				UVerticalBox::StaticClass(), *(TEXT("SlotStack") + Suffix));
			if (USizeBoxSlot* StackSlot = Cast<USizeBoxSlot>(Sizer->AddChild(Stack)))
			{
				StackSlot->SetHorizontalAlignment(HAlign_Center);
				StackSlot->SetVerticalAlignment(VAlign_Center);
			}

			auto AddLine = [this, Stack](TArray<TObjectPtr<UTextBlock>>& Into, const FString& Name)
			{
				UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *Name);
				if (UVerticalBoxSlot* LineSlot = Cast<UVerticalBoxSlot>(Stack->AddChild(Text)))
				{
					LineSlot->SetHorizontalAlignment(HAlign_Center);
				}
				Into.Add(Text);
			};

			AddLine(SlotKeyTexts, TEXT("SlotKey") + Suffix);
			AddLine(SlotNameTexts, TEXT("SlotName") + Suffix);
			AddLine(SlotAmmoTexts, TEXT("SlotAmmo") + Suffix);

			if (UHorizontalBoxSlot* RowSlot = Cast<UHorizontalBoxSlot>(SlotRow->AddChild(Box)))
			{
				RowSlot->SetPadding(FMargin(6.f, 0.f));
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
		BoundInventory->OnActiveSlotChanged.RemoveDynamic(this, &UCastleHotbarWidget::HandleActiveSlotChanged);
	}

	BoundInventory = Inventory;

	if (BoundInventory)
	{
		BoundInventory->OnInventoryChanged.AddDynamic(this, &UCastleHotbarWidget::HandleInventoryChanged);
		BoundInventory->OnActiveSlotChanged.AddDynamic(this, &UCastleHotbarWidget::HandleActiveSlotChanged);
	}

	RefreshSlots();
}

void UCastleHotbarWidget::HandleInventoryChanged()
{
	RefreshSlots();
}

void UCastleHotbarWidget::HandleActiveSlotChanged(EHotbarSlot /*OldSlot*/, EHotbarSlot /*NewSlot*/)
{
	RefreshSlots();
}

EHotbarSlot UCastleHotbarWidget::GetActiveSlot() const
{
	return BoundInventory ? BoundInventory->GetActiveSlot() : EHotbarSlot::Hands;
}

bool UCastleHotbarWidget::IsSlotActive(EHotbarSlot HotbarSlot) const
{
	return BoundInventory != nullptr && BoundInventory->GetActiveSlot() == HotbarSlot;
}

bool UCastleHotbarWidget::IsSlotEmpty(EHotbarSlot HotbarSlot) const
{
	return !BoundInventory || BoundInventory->IsSlotEmpty(HotbarSlot);
}

FText UCastleHotbarWidget::GetSlotKeyText(EHotbarSlot HotbarSlot)
{
	return FText::AsNumber(static_cast<int32>(HotbarSlot) + 1);
}

FText UCastleHotbarWidget::GetSlotNameText(EHotbarSlot HotbarSlot) const
{
	if (!BoundInventory)
	{
		return FText::GetEmpty();
	}

	const FCastleInventorySlot Entry = BoundInventory->GetSlot(HotbarSlot);
	if (Entry.IsEmpty())
	{
		return NSLOCTEXT("Castle", "HotbarEmptySlot", "--");
	}

	return Entry.Weapon->GetShortNameOrDisplayName();
}

FText UCastleHotbarWidget::GetSlotAmmoText(EHotbarSlot HotbarSlot) const
{
	if (!BoundInventory)
	{
		return FText::GetEmpty();
	}

	const FCastleInventorySlot Entry = BoundInventory->GetSlot(HotbarSlot);
	if (!Entry.IsRanged())
	{
		// Fists have no ammo, and neither does a slot with nothing in it.
		return FText::GetEmpty();
	}

	return FText::FromString(FString::Printf(TEXT("%d / %d"), Entry.Magazine, Entry.Reserve));
}

FLinearColor UCastleHotbarWidget::GetSlotColor(EHotbarSlot HotbarSlot) const
{
	if (IsSlotEmpty(HotbarSlot))
	{
		return EmptySlotColor;
	}
	return IsSlotActive(HotbarSlot) ? ActiveSlotColor : FilledSlotColor;
}

void UCastleHotbarWidget::RefreshSlots()
{
	for (int32 Index = 0; Index < CastleHotbarSlotCount; ++Index)
	{
		const EHotbarSlot HotbarSlot = static_cast<EHotbarSlot>(Index);

		if (SlotBoxes.IsValidIndex(Index) && SlotBoxes[Index])
		{
			SlotBoxes[Index]->SetBrushColor(GetSlotColor(HotbarSlot));
		}
		if (SlotKeyTexts.IsValidIndex(Index) && SlotKeyTexts[Index])
		{
			SlotKeyTexts[Index]->SetText(GetSlotKeyText(HotbarSlot));
		}
		if (SlotNameTexts.IsValidIndex(Index) && SlotNameTexts[Index])
		{
			SlotNameTexts[Index]->SetText(GetSlotNameText(HotbarSlot));
		}
		if (SlotAmmoTexts.IsValidIndex(Index) && SlotAmmoTexts[Index])
		{
			SlotAmmoTexts[Index]->SetText(GetSlotAmmoText(HotbarSlot));
		}
	}
}

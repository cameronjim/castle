// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/HawkeyeSafehouseWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "UI/HawkeyeMenuLayout.h"

TSharedRef<SWidget> UHawkeyeSafehouseWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UVerticalBox* Stack = HawkeyeMenuLayout::BuildFrame(WidgetTree, 0.6f);
		TitleText = HawkeyeMenuLayout::AddText(WidgetTree, Stack, TEXT("TitleText"), SafehouseName, 28, 6.f);
		StatusText = HawkeyeMenuLayout::AddText(WidgetTree, Stack, TEXT("StatusText"), StatusLine, 14, 24.f);
		HawkeyeMenuLayout::AddButton(WidgetTree, Stack, RefillButton, TEXT("RefillButton"),
			NSLOCTEXT("Hawkeye", "SafehouseRefill", "Refill arrows"));
		HawkeyeMenuLayout::AddButton(WidgetTree, Stack, SaveButton, TEXT("SaveButton"),
			NSLOCTEXT("Hawkeye", "SafehouseSave", "Save"));
		HawkeyeMenuLayout::AddButton(WidgetTree, Stack, FastTravelButton, TEXT("FastTravelButton"),
			NSLOCTEXT("Hawkeye", "SafehouseFastTravel", "Fast travel"));
		HawkeyeMenuLayout::AddButton(WidgetTree, Stack, ChapterSelectButton, TEXT("ChapterSelectButton"),
			NSLOCTEXT("Hawkeye", "SafehouseChapterSelect", "Chapter select"));
		HawkeyeMenuLayout::AddButton(WidgetTree, Stack, LeaveButton, TEXT("LeaveButton"),
			NSLOCTEXT("Hawkeye", "SafehouseLeave", "Leave"));
	}
	return Super::RebuildWidget();
}

void UHawkeyeSafehouseWidget::SetSafehouseName(const FText& Name)
{
	SafehouseName = FText::Format(NSLOCTEXT("Hawkeye", "SafehouseTitle", "Safehouse: {0}"), Name);
	if (TitleText)
	{
		TitleText->SetText(SafehouseName);
	}
}

void UHawkeyeSafehouseWidget::SetStatus(const FText& Status)
{
	StatusLine = Status;
	if (StatusText)
	{
		StatusText->SetText(StatusLine);
	}
}

void UHawkeyeSafehouseWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (bBound)
	{
		return;
	}
	if (RefillButton)
	{
		RefillButton->OnClicked.AddDynamic(this, &UHawkeyeSafehouseWidget::HandleRefillClicked);
	}
	if (SaveButton)
	{
		SaveButton->OnClicked.AddDynamic(this, &UHawkeyeSafehouseWidget::HandleSaveClicked);
	}
	if (FastTravelButton)
	{
		FastTravelButton->OnClicked.AddDynamic(this, &UHawkeyeSafehouseWidget::HandleFastTravelClicked);
	}
	if (ChapterSelectButton)
	{
		ChapterSelectButton->OnClicked.AddDynamic(this, &UHawkeyeSafehouseWidget::HandleChapterSelectClicked);
	}
	if (LeaveButton)
	{
		LeaveButton->OnClicked.AddDynamic(this, &UHawkeyeSafehouseWidget::HandleLeaveClicked);
	}
	bBound = true;
}

void UHawkeyeSafehouseWidget::NativeDestruct()
{
	if (bBound)
	{
		if (RefillButton)
		{
			RefillButton->OnClicked.RemoveDynamic(this, &UHawkeyeSafehouseWidget::HandleRefillClicked);
		}
		if (SaveButton)
		{
			SaveButton->OnClicked.RemoveDynamic(this, &UHawkeyeSafehouseWidget::HandleSaveClicked);
		}
		if (FastTravelButton)
		{
			FastTravelButton->OnClicked.RemoveDynamic(this, &UHawkeyeSafehouseWidget::HandleFastTravelClicked);
		}
		if (ChapterSelectButton)
		{
			ChapterSelectButton->OnClicked.RemoveDynamic(this, &UHawkeyeSafehouseWidget::HandleChapterSelectClicked);
		}
		if (LeaveButton)
		{
			LeaveButton->OnClicked.RemoveDynamic(this, &UHawkeyeSafehouseWidget::HandleLeaveClicked);
		}
		bBound = false;
	}
	Super::NativeDestruct();
}

void UHawkeyeSafehouseWidget::HandleRefillClicked()
{
	OnRefillClicked.Broadcast();
}

void UHawkeyeSafehouseWidget::HandleSaveClicked()
{
	OnSaveClicked.Broadcast();
}

void UHawkeyeSafehouseWidget::HandleFastTravelClicked()
{
	OnFastTravelClicked.Broadcast();
}

void UHawkeyeSafehouseWidget::HandleChapterSelectClicked()
{
	OnChapterSelectClicked.Broadcast();
}

void UHawkeyeSafehouseWidget::HandleLeaveClicked()
{
	OnLeaveClicked.Broadcast();
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/HawkeyeThugOverheadWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Camera/PlayerCameraManager.h"
#include "Combat/HealthComponent.h"
#include "Components/CanvasPanel.h"
#include "EngineUtils.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "World/ThugCharacter.h"

TSharedRef<SWidget> UHawkeyeThugOverheadWidget::RebuildWidget()
{
	// Painted, not laid out: an empty canvas gives it a body that fills the HUD's overlay slot.
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		WidgetTree->RootWidget = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ThugOverheadRoot"));
	}
	return Super::RebuildWidget();
}

void UHawkeyeThugOverheadWidget::NativeTick(const FGeometry& MyGeometry, float DeltaSeconds)
{
	Super::NativeTick(MyGeometry, DeltaSeconds);
	GatherEntries();
}

void UHawkeyeThugOverheadWidget::GatherEntries()
{
	Entries.Reset();
	APlayerController* PC = GetOwningPlayer();
	UWorld* World = GetWorld();
	if (!PC || !PC->PlayerCameraManager || !World)
	{
		return;
	}
	const FVector Camera = PC->PlayerCameraManager->GetCameraLocation();
	const APawn* Viewer = PC->GetPawn();
	for (TActorIterator<AThugCharacter> It(World); It; ++It)
	{
		const AThugCharacter* Thug = *It;
		if (Thug->IsLimp() || FVector::Dist(Thug->GetActorLocation(), Camera) > MaxGlyphDistance)
		{
			continue;
		}
		// The bar's range is from Kate, not the lens: it is about who she is fighting.
		const float FromViewer = FVector::Dist(Thug->GetActorLocation(), Viewer ? Viewer->GetActorLocation() : Camera);
		const TCHAR Glyph = Thug->GetAlertGlyph();
		const float BarAlpha = Thug->GetHealthBarAlpha(FromViewer);
		if (!Glyph && BarAlpha <= 0.f)
		{
			continue;
		}
		FHawkeyeThugOverhead Entry;
		if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, Thug->GetOverheadLocation(), Entry.Position, false))
		{
			continue;
		}
		if (Glyph)
		{
			Entry.Glyph = FString::Chr(Glyph);
			// Full for most of its life, a quick fade in the last 0.15 s.
			Entry.GlyphAlpha = FMath::Clamp(Thug->GetAlertGlyphRemaining() / 0.15f, 0.f, 1.f);
		}
		const UHealthComponent* Health = Thug->GetHealthComponent();
		Entry.HealthFraction = Health ? Health->GetHealthPercent() : 1.f;
		Entry.BarAlpha = BarAlpha;
		Entries.Add(Entry);
	}
}

int32 UHawkeyeThugOverheadWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle,
		bParentEnabled) + 1;
	const FSlateBrush* White = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), GlyphFontSize);
	const bool bCanMeasure = FSlateApplication::IsInitialized() && FSlateApplication::Get().GetRenderer();
	for (const FHawkeyeThugOverhead& Entry : Entries)
	{
		float Top = Entry.Position.Y;
		if (Entry.BarAlpha > 0.f)
		{
			const FVector2D BarTopLeft(Entry.Position.X - BarSize.X * 0.5f, Top - BarSize.Y);
			FLinearColor Back = BarBackColor;
			Back.A *= Entry.BarAlpha;
			FLinearColor Fill = BarColor;
			Fill.A *= Entry.BarAlpha;
			FSlateDrawElement::MakeBox(OutDrawElements, Layer,
				AllottedGeometry.ToPaintGeometry(FVector2f(BarSize + FVector2D(2.f, 2.f)),
					FSlateLayoutTransform(FVector2f(BarTopLeft - FVector2D(1.f, 1.f)))),
				White, ESlateDrawEffect::None, Back);
			FSlateDrawElement::MakeBox(OutDrawElements, Layer + 1,
				AllottedGeometry.ToPaintGeometry(FVector2f(BarSize.X * FMath::Clamp(Entry.HealthFraction, 0.f, 1.f), BarSize.Y),
					FSlateLayoutTransform(FVector2f(BarTopLeft))),
				White, ESlateDrawEffect::None, Fill);
			Top -= BarSize.Y + 4.f;
		}
		if (!Entry.Glyph.IsEmpty() && Entry.GlyphAlpha > 0.f)
		{
			const FText Text = FText::FromString(Entry.Glyph);
			const FVector2D Size = bCanMeasure
				? FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text, Font)
				: FVector2D(GlyphFontSize * 0.5f, GlyphFontSize);
			const FVector2D TopLeft(Entry.Position.X - Size.X * 0.5f, Top - Size.Y);
			FLinearColor Color = GlyphColor;
			Color.A *= Entry.GlyphAlpha;
			const FLinearColor Shadow(0.f, 0.f, 0.f, 0.7f * Entry.GlyphAlpha);
			FSlateDrawElement::MakeText(OutDrawElements, Layer,
				AllottedGeometry.ToPaintGeometry(FVector2f(Size),
					FSlateLayoutTransform(FVector2f(TopLeft + FVector2D(1.5f, 1.5f)))),
				Text, Font, ESlateDrawEffect::None, Shadow);
			FSlateDrawElement::MakeText(OutDrawElements, Layer + 1,
				AllottedGeometry.ToPaintGeometry(FVector2f(Size), FSlateLayoutTransform(FVector2f(TopLeft))),
				Text, Font, ESlateDrawEffect::None, Color);
		}
	}
	return Layer + 2;
}

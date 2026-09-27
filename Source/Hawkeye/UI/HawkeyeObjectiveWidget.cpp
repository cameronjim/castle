// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/HawkeyeObjectiveWidget.h"
#include "Audio/HawkeyeAudioSubsystem.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CanvasPanel.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Mission/MissionObjective.h"
#include "Mission/MissionSubsystem.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "SceneView.h"
#include "Styling/CoreStyle.h"

namespace HawkeyeObjectiveHud
{
	static constexpr float CompassHeight = 30.f;
	static const FLinearColor Shadow(0.f, 0.f, 0.f, 0.6f);

	static FVector2D MeasureText(const FText& Text, const FSlateFontInfo& Font)
	{
		if (!FSlateApplication::IsInitialized() || !FSlateApplication::Get().GetRenderer())
		{
			return FVector2D::ZeroVector;
		}
		return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text, Font);
	}

	/** Text centred horizontally on CentreTop, with a one-pixel drop shadow so it reads on sky and snow. */
	static void DrawText(FSlateWindowElementList& Out, int32 LayerId, const FGeometry& Geometry, const FText& Text,
		const FSlateFontInfo& Font, const FVector2D& CentreTop, const FLinearColor& Color)
	{
		const FVector2D Size = MeasureText(Text, Font);
		const FVector2D TopLeft(CentreTop.X - Size.X * 0.5f, CentreTop.Y);
		FLinearColor ShadowColor = Shadow;
		ShadowColor.A *= Color.A;
		FSlateDrawElement::MakeText(Out, LayerId,
			Geometry.ToPaintGeometry(FVector2f(Size), FSlateLayoutTransform(FVector2f(TopLeft + FVector2D(1.f, 1.f)))),
			Text, Font, ESlateDrawEffect::None, ShadowColor);
		FSlateDrawElement::MakeText(Out, LayerId + 1,
			Geometry.ToPaintGeometry(FVector2f(Size), FSlateLayoutTransform(FVector2f(TopLeft))),
			Text, Font, ESlateDrawEffect::None, Color);
	}

	/** A hollow diamond HalfSize from the centre to each point, shadowed like the text. */
	static void DrawDiamond(FSlateWindowElementList& Out, int32 LayerId, const FGeometry& Geometry,
		const FVector2D& Centre, float HalfSize, float Thickness, const FLinearColor& Color)
	{
		auto Points = [HalfSize](const FVector2D& C)
		{
			return TArray<FVector2D>{ C + FVector2D(0.f, -HalfSize), C + FVector2D(HalfSize, 0.f),
				C + FVector2D(0.f, HalfSize), C + FVector2D(-HalfSize, 0.f), C + FVector2D(0.f, -HalfSize) };
		};
		FLinearColor ShadowColor = Shadow;
		ShadowColor.A *= Color.A;
		FSlateDrawElement::MakeLines(Out, LayerId, Geometry.ToPaintGeometry(), Points(Centre + FVector2D(1.f, 1.f)),
			ESlateDrawEffect::None, ShadowColor, true, Thickness);
		FSlateDrawElement::MakeLines(Out, LayerId + 1, Geometry.ToPaintGeometry(), Points(Centre),
			ESlateDrawEffect::None, Color, true, Thickness);
	}
}

TSharedRef<SWidget> UHawkeyeObjectiveWidget::RebuildWidget()
{
	// Nothing is laid out, everything is painted; an empty canvas gives the widget a body that
	// fills the HUD's overlay slot.
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		WidgetTree->RootWidget = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ObjectiveRoot"));
	}
	return Super::RebuildWidget();
}

void UHawkeyeObjectiveWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ObjectiveCompleteHeading.IsEmpty())
	{
		ObjectiveCompleteHeading = NSLOCTEXT("Hawkeye", "ObjectiveComplete", "Objective complete");
	}
	if (NewObjectiveHeading.IsEmpty())
	{
		NewObjectiveHeading = NSLOCTEXT("Hawkeye", "NewObjective", "New objective");
	}

	if (!bBound)
	{
		if (UMissionSubsystem* Missions = UMissionSubsystem::Get(this))
		{
			Missions->OnMissionStarted.AddDynamic(this, &UHawkeyeObjectiveWidget::HandleMissionStarted);
			Missions->OnObjectiveUpdated.AddDynamic(this, &UHawkeyeObjectiveWidget::HandleObjectiveUpdated);
			bBound = true;
		}
	}
	NoteCurrentObjective();
}

void UHawkeyeObjectiveWidget::NativeDestruct()
{
	if (bBound)
	{
		if (UMissionSubsystem* Missions = UMissionSubsystem::Get(this))
		{
			Missions->OnMissionStarted.RemoveDynamic(this, &UHawkeyeObjectiveWidget::HandleMissionStarted);
			Missions->OnObjectiveUpdated.RemoveDynamic(this, &UHawkeyeObjectiveWidget::HandleObjectiveUpdated);
		}
		bBound = false;
	}

	Super::NativeDestruct();
}

void UHawkeyeObjectiveWidget::HandleMissionStarted(UMissionDefinition* /*Mission*/)
{
	LastCurrentObjectiveId = NAME_None;
	NoteCurrentObjective();
}

void UHawkeyeObjectiveWidget::HandleObjectiveUpdated(UMissionObjective* Objective, int32 /*ObjectiveIndex*/)
{
	if (Objective && Objective->IsCompleted())
	{
		PushToast(ObjectiveCompleteHeading, Objective->Title, EHawkeyeUISound::ObjectiveComplete);
	}
	NoteCurrentObjective();
}

void UHawkeyeObjectiveWidget::NoteCurrentObjective()
{
	const UMissionSubsystem* Missions = UMissionSubsystem::Get(this);
	const UMissionObjective* Current = Missions ? Missions->GetCurrentObjective() : nullptr;
	const FName CurrentId = Current ? Current->ObjectiveId : NAME_None;
	if (CurrentId == LastCurrentObjectiveId)
	{
		return;
	}
	LastCurrentObjectiveId = CurrentId;
	if (Current)
	{
		PushToast(NewObjectiveHeading, Current->Title, EHawkeyeUISound::NewObjective);
	}
}

void UHawkeyeObjectiveWidget::PushToast(FText Heading, FText Title, EHawkeyeUISound Sound)
{
	const bool bShowsNow = ToastQueue.Num() == 0;
	if (bShowsNow)
	{
		ToastElapsed = 0.f;
	}
	ToastQueue.Add({ Heading, Title, Sound });
	if (bShowsNow)
	{
		UHawkeyeAudioSubsystem::PlayUI(this, Sound);
	}
}

void UHawkeyeObjectiveWidget::ClearToasts()
{
	ToastQueue.Reset();
	ToastElapsed = 0.f;
}

void UHawkeyeObjectiveWidget::AdvanceToasts(float DeltaSeconds)
{
	if (ToastQueue.Num() == 0)
	{
		return;
	}
	ToastElapsed += FMath::Max(DeltaSeconds, 0.f);
	while (ToastQueue.Num() > 0 && ToastElapsed >= ToastSeconds)
	{
		ToastElapsed -= ToastSeconds;
		ToastQueue.RemoveAt(0);
		if (ToastQueue.Num() > 0)
		{
			UHawkeyeAudioSubsystem::PlayUI(this, ToastQueue[0].Sound);
		}
	}
	if (ToastQueue.Num() == 0)
	{
		ToastElapsed = 0.f;
	}
}

FText UHawkeyeObjectiveWidget::GetToastHeading() const
{
	return ToastQueue.Num() > 0 ? ToastQueue[0].Heading : FText::GetEmpty();
}

FText UHawkeyeObjectiveWidget::GetToastTitle() const
{
	return ToastQueue.Num() > 0 ? ToastQueue[0].Title : FText::GetEmpty();
}

void UHawkeyeObjectiveWidget::NativeTick(const FGeometry& MyGeometry, float DeltaSeconds)
{
	Super::NativeTick(MyGeometry, DeltaSeconds);

	AdvanceToasts(DeltaSeconds);
	UpdateMarker(MyGeometry);
}

void UHawkeyeObjectiveWidget::UpdateMarker(const FGeometry& /*MyGeometry*/)
{
	bMarkerVisible = false;
	bCompassIconVisible = false;

	APlayerController* PC = GetOwningPlayer();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!PC || !PC->PlayerCameraManager)
	{
		return;
	}
	const FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
	ViewBearing = UObjectiveMarkerMath::YawToBearing(PC->PlayerCameraManager->GetCameraRotation().Yaw, NorthYawDegrees);

	const UMissionSubsystem* Missions = UMissionSubsystem::Get(this);
	FVector Target;
	if (!Pawn || !Missions || !Missions->GetCurrentObjectiveLocation(Target))
	{
		return;
	}
	const float Distance = FVector::Dist(Pawn->GetActorLocation(), Target);
	if (Distance < HideWithinDistance)
	{
		return;
	}

	CompassIconOffset = UObjectiveMarkerMath::CompassOffset(
		UObjectiveMarkerMath::BearingBetween(CameraLocation, Target, NorthYawDegrees), ViewBearing,
		CompassWidth, CompassSpanDegrees, bCompassIconClamped);
	bCompassIconVisible = true;

	// The renderer's own view-projection, so the diamond sits exactly where the point is drawn.
	ULocalPlayer* LocalPlayer = PC->GetLocalPlayer();
	FSceneViewProjectionData Projection;
	if (!LocalPlayer || !LocalPlayer->ViewportClient
		|| !LocalPlayer->GetProjectionData(LocalPlayer->ViewportClient->Viewport, Projection))
	{
		return;
	}
	const FIntRect ViewRect = Projection.GetConstrainedViewRect();
	ViewportScale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), UE_KINDA_SMALL_NUMBER);
	ViewRectMin = FVector2D(ViewRect.Min);
	Placement = UObjectiveMarkerMath::PlaceMarker(Target, Projection.ComputeViewProjectionMatrix(),
		FVector2D(ViewRect.Width(), ViewRect.Height()), EdgeMargin * ViewportScale);
	DistanceText = UObjectiveMarkerMath::FormatDistance(Distance);
	bMarkerVisible = true;
}

int32 UHawkeyeObjectiveWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle,
		bParentEnabled);
	PaintCompass(AllottedGeometry, OutDrawElements, Layer + 1);
	PaintMarker(AllottedGeometry, OutDrawElements, Layer + 1);
	PaintToast(AllottedGeometry, OutDrawElements, Layer + 1);
	return Layer + 4;
}

void UHawkeyeObjectiveWidget::PaintMarker(const FGeometry& Geometry, FSlateWindowElementList& Out, int32 LayerId) const
{
	using namespace HawkeyeObjectiveHud;
	if (!bMarkerVisible)
	{
		return;
	}

	const FVector2D Centre = (ViewRectMin + Placement.Position) / ViewportScale;
	const float Half = MarkerSizePixels * 0.5f;
	DrawDiamond(Out, LayerId, Geometry, Centre, Half, MarkerLineWidth, MarkerColor);

	// Off screen, a chevron just outside the diamond points the way to turn.
	const float Radians = FMath::DegreesToRadians(Placement.ArrowAngleDegrees);
	const FVector2D Dir(FMath::Cos(Radians), FMath::Sin(Radians));
	if (!Placement.bOnScreen)
	{
		const FVector2D Side(-Dir.Y, Dir.X);
		const FVector2D Tip = Centre + Dir * (Half + 12.f);
		const FVector2D Base = Centre + Dir * (Half + 5.f);
		const TArray<FVector2D> Chevron{ Base + Side * 5.f, Tip, Base - Side * 5.f };
		FSlateDrawElement::MakeLines(Out, LayerId + 1, Geometry.ToPaintGeometry(), Chevron,
			ESlateDrawEffect::None, MarkerColor, true, MarkerLineWidth);
	}

	// The distance goes under the diamond, or over it when the arrow is pointing down.
	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 13);
	const bool bTextAbove = !Placement.bOnScreen && Dir.Y > 0.5f;
	const float TextHeight = MeasureText(DistanceText, Font).Y;
	const FVector2D TextTop = bTextAbove
		? Centre - FVector2D(0.f, Half + 4.f + TextHeight)
		: Centre + FVector2D(0.f, Half + 4.f);
	DrawText(Out, LayerId, Geometry, DistanceText, Font, TextTop, MarkerColor);
}

void UHawkeyeObjectiveWidget::PaintCompass(const FGeometry& Geometry, FSlateWindowElementList& Out, int32 LayerId) const
{
	using namespace HawkeyeObjectiveHud;
	const float CentreX = Geometry.GetLocalSize().X * 0.5f;
	const float HalfWidth = CompassWidth * 0.5f;
	const float Bottom = CompassTop + CompassHeight;

	FSlateDrawElement::MakeBox(Out, LayerId,
		Geometry.ToPaintGeometry(FVector2f(CompassWidth, CompassHeight),
			FSlateLayoutTransform(FVector2f(CentreX - HalfWidth, CompassTop))),
		FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")), ESlateDrawEffect::None, CompassBackColor);

	static const TCHAR* Cardinals[] = { TEXT("N"), TEXT("E"), TEXT("S"), TEXT("W") };
	const FSlateFontInfo LetterFont = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 14);
	const int32 TickCount = FMath::Max(1, FMath::RoundToInt(360.f / CompassTickDegrees));
	for (int32 Tick = 0; Tick < TickCount; ++Tick)
	{
		const float Bearing = Tick * CompassTickDegrees;
		const float Offset = UObjectiveMarkerMath::BearingDelta(Bearing, ViewBearing) / CompassSpanDegrees * CompassWidth;
		if (FMath::Abs(Offset) > HalfWidth - 4.f)
		{
			continue;
		}
		// Fade toward the ends so the strip has no hard cut.
		const float Edge = FMath::Abs(Offset) / HalfWidth;
		FLinearColor Color = FLinearColor::White;
		Color.A = 1.f - 0.75f * Edge * Edge;
		const float X = CentreX + Offset;
		const int32 WholeBearing = FMath::RoundToInt(Bearing);
		if (WholeBearing % 90 == 0)
		{
			DrawText(Out, LayerId + 1, Geometry, FText::FromString(Cardinals[(WholeBearing / 90) % 4]), LetterFont,
				FVector2D(X, CompassTop + 5.f), Color);
			continue;
		}
		const float Length = WholeBearing % 45 == 0 ? 12.f : 7.f;
		Color.A *= 0.8f;
		FSlateDrawElement::MakeLines(Out, LayerId + 1, Geometry.ToPaintGeometry(),
			TArray<FVector2D>{ FVector2D(X, Bottom - 4.f - Length), FVector2D(X, Bottom - 4.f) },
			ESlateDrawEffect::None, Color, true, 2.f);
	}

	// Where the camera faces: a small caret under the strip's centre.
	FSlateDrawElement::MakeLines(Out, LayerId + 1, Geometry.ToPaintGeometry(),
		TArray<FVector2D>{ FVector2D(CentreX - 6.f, Bottom + 8.f), FVector2D(CentreX, Bottom + 2.f),
			FVector2D(CentreX + 6.f, Bottom + 8.f) },
		ESlateDrawEffect::None, MarkerColor, true, 2.f);

	if (bCompassIconVisible)
	{
		FLinearColor IconColor = MarkerColor;
		IconColor.A = bCompassIconClamped ? 0.55f : 1.f;
		DrawDiamond(Out, LayerId + 2, Geometry, FVector2D(CentreX + CompassIconOffset, CompassTop + CompassHeight * 0.5f),
			8.f, 2.f, IconColor);
	}
}

void UHawkeyeObjectiveWidget::PaintToast(const FGeometry& Geometry, FSlateWindowElementList& Out, int32 LayerId) const
{
	using namespace HawkeyeObjectiveHud;
	if (ToastQueue.Num() == 0)
	{
		return;
	}

	// Quick fade in, slower fade out.
	const float FadeIn = FMath::Clamp(ToastElapsed / 0.15f, 0.f, 1.f);
	const float FadeOut = FMath::Clamp((ToastSeconds - ToastElapsed) / 0.35f, 0.f, 1.f);
	const float Alpha = FMath::Min(FadeIn, FadeOut);

	const float CentreX = Geometry.GetLocalSize().X * 0.5f;
	const float Top = CompassTop + CompassHeight + 44.f;
	FLinearColor HeadingColor = MarkerColor;
	HeadingColor.A = Alpha;
	FLinearColor TitleColor = FLinearColor::White;
	TitleColor.A = Alpha;

	const FSlateFontInfo HeadingFont = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 14);
	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 24);
	const FText Heading = ToastQueue[0].Heading.ToUpper();
	DrawText(Out, LayerId, Geometry, Heading, HeadingFont, FVector2D(CentreX, Top), HeadingColor);
	DrawText(Out, LayerId, Geometry, ToastQueue[0].Title, TitleFont,
		FVector2D(CentreX, Top + MeasureText(Heading, HeadingFont).Y + 2.f), TitleColor);
}

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HawkeyeThugOverheadWidget.generated.h"

class AThugCharacter;

/** One thug's overhead read for this frame: where, which glyph, how opaque the health bar. */
USTRUCT(BlueprintType)
struct HAWKEYE_API FHawkeyeThugOverhead
{
	GENERATED_BODY()

	/** Over his head, in the HUD's local space. */
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Thugs")
	FVector2D Position = FVector2D::ZeroVector;

	/** "!", "?" or empty. */
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Thugs")
	FString Glyph;

	UPROPERTY(BlueprintReadOnly, Category = "HUD|Thugs")
	float GlyphAlpha = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "HUD|Thugs")
	float HealthFraction = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "HUD|Thugs")
	float BarAlpha = 0.f;
};

/**
 * Over every thug's head (claude-docs/gameplay-semantics.md, "thug readability"): a small cream "!" for
 * 0.6 s when he goes Alerted, a "?" when he goes Suspicious, and a thin health bar only while he is hurt,
 * within 1500 cm, fading 3 s after the last hit. The rules live on AThugCharacter (tested there); this
 * only projects and paints. Built into UHawkeyeHudWidget's overlay.
 */
UCLASS(Blueprintable, BlueprintType)
class HAWKEYE_API UHawkeyeThugOverheadWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** This frame's entries, one per thug with something to show. */
	UFUNCTION(BlueprintPure, Category = "HUD|Thugs")
	const TArray<FHawkeyeThugOverhead>& GetEntries() const { return Entries; }

	/** Glyph colour: the objective marker's cream. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Thugs")
	FLinearColor GlyphColor = FLinearColor(1.f, 0.97f, 0.88f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Thugs")
	FLinearColor BarColor = FLinearColor(0.95f, 0.3f, 0.25f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Thugs")
	FLinearColor BarBackColor = FLinearColor(0.f, 0.f, 0.f, 0.55f);

	/** The bar, px. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Thugs")
	FVector2D BarSize = FVector2D(46.f, 4.f);

	/**
	 * A thin dark outline round the glyph, px, so the "!" and "?" read by shape on any background and
	 * in any palette, not by colour alone.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Thugs", meta = (ClampMin = "0"))
	int32 GlyphOutlineSize = 1;

	/** The font the glyphs are painted with, outline included. */
	UFUNCTION(BlueprintPure, Category = "HUD|Thugs")
	FSlateFontInfo GetGlyphFont() const;

	/**
	 * The palette's cream for the glyphs and its health-bar colour. bDesign puts back the colours the
	 * widget was built with (the default palette).
	 */
	void SetAccentColors(const FLinearColor& Glyph, const FLinearColor& Bar, bool bDesign);

	/** The glyph's font size. Small: a flag, not a banner. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Thugs", meta = (ClampMin = "6"))
	int32 GlyphFontSize = 20;

	/** Thugs further than this from the camera are not considered at all, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Thugs", meta = (ClampMin = "0.0"))
	float MaxGlyphDistance = 4000.f;

protected:
	//~ Begin UUserWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float DeltaSeconds) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;
	//~ End UUserWidget interface

	/** Rebuilds Entries from the thugs in the world. */
	void GatherEntries();

	TArray<FHawkeyeThugOverhead> Entries;

	/** The colours the widget was built with, kept for the default palette. */
	FLinearColor DesignGlyphColor = FLinearColor::White;
	FLinearColor DesignBarColor = FLinearColor::White;
	bool bDesignColorsKept = false;
};

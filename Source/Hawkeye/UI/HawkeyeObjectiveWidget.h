// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Audio/HawkeyeAudioTypes.h"
#include "UI/ObjectiveMarkerMath.h"
#include "HawkeyeObjectiveWidget.generated.h"

class UMissionDefinition;
class UMissionObjective;

/**
 * Where the current objective is: a cream diamond projected onto its point with the distance
 * under it (clamped to the screen edge with an arrow when off screen, hidden within
 * HideWithinDistance), a compass strip at the top centre with the objective on it, and the
 * "Objective complete" / "New objective" toasts. Built into UHawkeyeHudWidget's overlay.
 *
 * Everything is painted in NativePaint from state NativeTick computes, so there is no widget tree
 * to keep in step; the arithmetic is in UObjectiveMarkerMath and tested there.
 */
UCLASS(Blueprintable, BlueprintType)
class HAWKEYE_API UHawkeyeObjectiveWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * Queues a toast: a small heading ("Objective complete") over the objective's title. Sound plays
	 * when it comes up (at once, or when the one in front of it goes).
	 */
	UFUNCTION(BlueprintCallable, Category = "HUD|Objective")
	void PushToast(FText Heading, FText Title, EHawkeyeUISound Sound = EHawkeyeUISound::Toast);

	/** Drops every queued toast, the showing one included (a chapter ending supersedes them). */
	UFUNCTION(BlueprintCallable, Category = "HUD|Objective")
	void ClearToasts();

	/** Counts the showing toast down and moves on to the next queued one. */
	UFUNCTION(BlueprintCallable, Category = "HUD|Objective")
	void AdvanceToasts(float DeltaSeconds);

	UFUNCTION(BlueprintPure, Category = "HUD|Objective")
	bool IsToastVisible() const { return ToastQueue.Num() > 0; }

	UFUNCTION(BlueprintPure, Category = "HUD|Objective")
	FText GetToastHeading() const;

	UFUNCTION(BlueprintPure, Category = "HUD|Objective")
	FText GetToastTitle() const;

	UFUNCTION(BlueprintPure, Category = "HUD|Objective")
	bool IsMarkerVisible() const { return bMarkerVisible; }

	/** This frame's placement, in viewport pixels. */
	UFUNCTION(BlueprintPure, Category = "HUD|Objective")
	FObjectiveMarkerPlacement GetMarkerPlacement() const { return Placement; }

	UFUNCTION(BlueprintPure, Category = "HUD|Objective")
	FText GetDistanceText() const { return DistanceText; }

	UFUNCTION(BlueprintPure, Category = "HUD|Compass")
	float GetViewBearing() const { return ViewBearing; }

	UFUNCTION(BlueprintPure, Category = "HUD|Compass")
	bool IsCompassIconVisible() const { return bCompassIconVisible; }

	/** Offset of the objective on the strip from its centre, px. */
	UFUNCTION(BlueprintPure, Category = "HUD|Compass")
	float GetCompassIconOffset() const { return CompassIconOffset; }

	UFUNCTION(BlueprintPure, Category = "HUD|Compass")
	float GetCompassWidth() const { return CompassWidth; }

	UFUNCTION(BlueprintPure, Category = "HUD|Objective")
	float GetMarkerSize() const { return MarkerSizePixels; }

	/** Secondary markers drawn this frame (they replace the objective's marker while there are any). */
	UFUNCTION(BlueprintPure, Category = "HUD|Objective")
	int32 GetSecondaryMarkerCount() const { return Secondary.Num(); }

	/** This frame's placement of secondary marker Index, in viewport pixels. */
	UFUNCTION(BlueprintPure, Category = "HUD|Objective")
	FObjectiveMarkerPlacement GetSecondaryPlacement(int32 Index) const
	{
		return Secondary.IsValidIndex(Index) ? Secondary[Index].Placement : FObjectiveMarkerPlacement();
	}

protected:
	//~ Begin UUserWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float DeltaSeconds) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;
	//~ End UUserWidget interface

	/** Projects the current objective's point and fills the marker and compass state. */
	void UpdateMarker(const FGeometry& MyGeometry);

	/**
	 * Projects the mission's secondary markers (a challenge's targets or next checkpoint). True when
	 * there are any, in which case they take the objective marker's place.
	 */
	bool UpdateSecondaryMarkers(APlayerController* PC, const FVector& PawnLocation, const FVector& CameraLocation);

	void PaintSecondary(const FGeometry& Geometry, FSlateWindowElementList& Out, int32 LayerId) const;

	void PaintMarker(const FGeometry& Geometry, FSlateWindowElementList& Out, int32 LayerId) const;
	void PaintCompass(const FGeometry& Geometry, FSlateWindowElementList& Out, int32 LayerId) const;
	void PaintToast(const FGeometry& Geometry, FSlateWindowElementList& Out, int32 LayerId) const;

	UFUNCTION()
	void HandleMissionStarted(UMissionDefinition* Mission);

	UFUNCTION()
	void HandleObjectiveUpdated(UMissionObjective* Objective, int32 ObjectiveIndex);

	/** Queues "New objective" when the current objective is no longer the one last seen. */
	void NoteCurrentObjective();

	/** Secondary markers: Kate's purple, smaller than the objective's diamond. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HUD|Objective")
	FLinearColor SecondaryColor = FLinearColor(0.78f, 0.55f, 1.f, 1.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HUD|Objective", meta = (ClampMin = "4.0"))
	float SecondarySizePixels = 12.f;

	/** Warm off-white: distinct from Kate's purple reticle and the grapple marker's green. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HUD|Objective")
	FLinearColor MarkerColor = FLinearColor(1.f, 0.93f, 0.78f, 1.f);

	/** Point to point size of the diamond, px. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HUD|Objective", meta = (ClampMin = "4.0"))
	float MarkerSizePixels = 16.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HUD|Objective", meta = (ClampMin = "0.5"))
	float MarkerLineWidth = 2.f;

	/** Closer than this (cm) the marker and the compass icon hide: you are there. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HUD|Objective", meta = (ClampMin = "0.0"))
	float HideWithinDistance = 300.f;

	/** How far in from the screen edge an off-screen marker is held, px. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HUD|Objective", meta = (ClampMin = "0.0"))
	float EdgeMargin = 48.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HUD|Compass", meta = (ClampMin = "50.0"))
	float CompassWidth = 400.f;

	/** Degrees of heading the strip shows across its width. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HUD|Compass", meta = (ClampMin = "30.0", ClampMax = "360.0"))
	float CompassSpanDegrees = 180.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HUD|Compass", meta = (ClampMin = "5.0"))
	float CompassTickDegrees = 15.f;

	/** Distance from the top of the screen to the strip, px. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HUD|Compass")
	float CompassTop = 24.f;

	/**
	 * The world yaw that points north. The district generator puts north along -Y (East 11th
	 * Street has a smaller Y than East 6th), so -90.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HUD|Compass")
	float NorthYawDegrees = -90.f;

	/** The strip's backing: the hotbar's filled-slot black, lighter. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HUD|Compass")
	FLinearColor CompassBackColor = FLinearColor(0.f, 0.f, 0.f, 0.35f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HUD|Objective", meta = (ClampMin = "0.1"))
	float ToastSeconds = 2.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HUD|Objective")
	FText ObjectiveCompleteHeading;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HUD|Objective")
	FText NewObjectiveHeading;

private:
	struct FToast
	{
		FText Heading;
		FText Title;
		EHawkeyeUISound Sound = EHawkeyeUISound::Toast;
	};

	TArray<FToast> ToastQueue;

	/** Seconds the front toast has been up. */
	float ToastElapsed = 0.f;

	/** Id of the objective that was current when last looked, so a change can be announced. */
	FName LastCurrentObjectiveId;

	struct FSecondaryMark
	{
		FObjectiveMarkerPlacement Placement;
		float CompassOffset = 0.f;
		bool bCompassClamped = false;
		float Distance = 0.f;
	};

	/** This frame's secondary markers; the nearest one carries the distance. */
	TArray<FSecondaryMark> Secondary;
	int32 NearestSecondary = INDEX_NONE;
	FText SecondaryDistanceText;

	bool bMarkerVisible = false;
	FObjectiveMarkerPlacement Placement;
	FText DistanceText;

	/** Placement is in viewport pixels; this converts it to the widget's own units. */
	float ViewportScale = 1.f;
	FVector2D ViewRectMin = FVector2D::ZeroVector;

	float ViewBearing = 0.f;
	bool bCompassIconVisible = false;
	bool bCompassIconClamped = false;
	float CompassIconOffset = 0.f;

	bool bBound = false;
};

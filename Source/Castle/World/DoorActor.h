// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/Interactable.h"
#include "DoorActor.generated.h"

class USceneComponent;
class UStaticMeshComponent;

/** How the door leaf gets out of the way. */
UENUM(BlueprintType)
enum class EDoorMotion : uint8
{
	/** Swings around the frame's vertical axis by OpenYawDegrees. */
	Swing,
	/** Slides along its own Y axis by SlideDistance. */
	Slide
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDoorOpenedSignature, AActor*, Opener);

/**
 * A door the player opens with Interact, optionally locked behind a keycard id.
 *
 * Animation is a lerp in Tick rather than a timeline so the whole thing stays in C++ and a
 * test can drive it by calling OpenNow(). Blocks the player while closed.
 */
UCLASS(Blueprintable, BlueprintType)
class CASTLE_API ADoorActor : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ADoorActor();

	/** When true the door refuses to open until the player carries RequiredKeycardId. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	bool bLocked = false;

	/** Keycard id that unlocks this door. Ignored when bLocked is false. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	FName RequiredKeycardId = FName(TEXT("cellblock"));

	/** Mission objective completed the first time this door opens. Optional. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door")
	FName CompletesObjectiveId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door")
	EDoorMotion Motion = EDoorMotion::Slide;

	/** Seconds the door takes to travel from closed to open. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door", meta = (ClampMin = "0.01"))
	float OpenSeconds = 1.f;

	/** Swing amount, in degrees, for EDoorMotion::Swing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door")
	float OpenYawDegrees = -95.f;

	/** Slide amount along the leaf's local Y, in centimetres, for EDoorMotion::Slide. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door")
	float SlideDistance = 110.f;

	UPROPERTY(BlueprintAssignable, Category = "Door")
	FOnDoorOpenedSignature OnDoorOpened;

	/** Static surround; never moves. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door")
	TObjectPtr<UStaticMeshComponent> FrameMesh;

	/** The leaf that animates. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door")
	TObjectPtr<UStaticMeshComponent> DoorMesh;

	/**
	 * Opens the door if it can be. Returns false when it is locked and Interactor has no keycard,
	 * or when it is already open / opening.
	 */
	UFUNCTION(BlueprintCallable, Category = "Door")
	bool TryOpen(AActor* Interactor);

	/** Opens with no keycard check (level scripting, tests). Returns false if already opening. */
	UFUNCTION(BlueprintCallable, Category = "Door")
	bool OpenNow(AActor* Interactor);

	/** True when Interactor carries RequiredKeycardId, or the door is not locked at all. */
	UFUNCTION(BlueprintPure, Category = "Door")
	bool IsUnlockedFor(const AActor* Interactor) const;

	UFUNCTION(BlueprintPure, Category = "Door")
	bool IsOpen() const { return bOpen; }

	/** 0 closed, 1 fully open. */
	UFUNCTION(BlueprintPure, Category = "Door")
	float GetOpenAlpha() const { return OpenAlpha; }

	/** Blueprint hook for the buzzer / denied beep. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Door")
	void OnOpenRefused(AActor* Interactor);

	//~ Begin IInteractable interface
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractPrompt_Implementation() const override;
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	//~ End IInteractable interface

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Puts the leaf at OpenAlpha along its closed -> open path. */
	void ApplyOpenAlpha();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door")
	bool bOpen = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door")
	float OpenAlpha = 0.f;

private:
	/** Leaf transform at BeginPlay; the open pose is derived from it. */
	FVector ClosedRelativeLocation = FVector::ZeroVector;
	FRotator ClosedRelativeRotation = FRotator::ZeroRotator;

	bool bAnimating = false;
};

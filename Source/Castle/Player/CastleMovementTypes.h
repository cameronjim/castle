// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CastleMovementTypes.generated.h"

/**
 * How the player is moving across the ground. Each gait has its own top speed on
 * ACastleCharacter; the character picks one from the input every frame (SelectGait).
 */
UENUM(BlueprintType)
enum class ECastleGait : uint8
{
	/** Light stick input, or aiming. */
	Walk,
	/** The default: keyboard input, a full stick, or any stick held for a moment. */
	Run,
	/** Sprint held while moving. */
	Sprint,
	/** Crouched and not sliding. */
	Crouch,
	/** Crouch pressed mid-sprint: a short, fast, low slide. */
	Slide,
};

/** Where the spring arm and lens want to be for one aim state. The camera blends between two of these. */
USTRUCT(BlueprintType)
struct CASTLE_API FCastleCameraTargets
{
	GENERATED_BODY()

	FCastleCameraTargets() = default;

	FCastleCameraTargets(float InArmLength, const FVector& InSocketOffset, float InFieldOfView)
		: ArmLength(InArmLength), SocketOffset(InSocketOffset), FieldOfView(InFieldOfView)
	{
	}

	/** Spring arm length, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "0.0"))
	float ArmLength = 350.f;

	/** Offset of the camera at the end of the arm, in the arm's space (Y is right, Z is up). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	FVector SocketOffset = FVector(0.f, 0.f, 60.f);

	/** Horizontal field of view, degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "10.0", ClampMax = "170.0"))
	float FieldOfView = 90.f;

	/** Straight blend between two targets; Alpha 0 is A, 1 is B. */
	static FCastleCameraTargets Lerp(const FCastleCameraTargets& A, const FCastleCameraTargets& B, float Alpha)
	{
		FCastleCameraTargets Out;
		Out.ArmLength = FMath::Lerp(A.ArmLength, B.ArmLength, Alpha);
		Out.SocketOffset = FMath::Lerp(A.SocketOffset, B.SocketOffset, Alpha);
		Out.FieldOfView = FMath::Lerp(A.FieldOfView, B.FieldOfView, Alpha);
		return Out;
	}
};

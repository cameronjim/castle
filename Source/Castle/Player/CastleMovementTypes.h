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

/** The traversal move an obstacle calls for (claude-docs/gameplay-semantics.md, traversal). */
UENUM(BlueprintType)
enum class ECastleParkourMove : uint8
{
	/** Nothing to do: too low, too high, or no room. A jump press jumps. */
	None,
	/** 60 to 110 cm with room on the far side: over it and down. */
	Vault,
	/** Up to 200 cm with somewhere to stand on (or just over) the top. */
	Mantle,
	/** 200 to 260 cm: jump, hang from the edge, then climb up or drop. */
	LedgeGrab,
	/** Hanging to standing on top. Only ever started from a hang. */
	Climb,
};

/** Who is moving the body through a traversal move. */
UENUM(BlueprintType)
enum class ECastleParkourRoute : uint8
{
	None,
	/** The Game Animation Sample's AC_TraversalLogic (motion-warped montage, its own detection). */
	SampleTraversal,
	/** UParkourComponent: a sample clip for the pose, the capsule moved along its warped root motion. */
	Procedural,
};

/** What the parkour probe found in front of the character. Heights are above the character's feet. */
USTRUCT(BlueprintType)
struct CASTLE_API FCastleParkourObstacle
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Parkour")
	bool bFound = false;

	/** Where the probe met the obstacle's front face. */
	UPROPERTY(BlueprintReadOnly, Category = "Parkour")
	FVector WallPoint = FVector::ZeroVector;

	/** Horizontal, out of the front face, back towards the character. */
	UPROPERTY(BlueprintReadOnly, Category = "Parkour")
	FVector WallNormal = FVector::ZeroVector;

	/** The top front edge: on the top surface, directly above WallPoint. */
	UPROPERTY(BlueprintReadOnly, Category = "Parkour")
	FVector LedgePoint = FVector::ZeroVector;

	/** Top of the obstacle above the feet, cm. */
	UPROPERTY(BlueprintReadOnly, Category = "Parkour")
	float Height = 0.f;

	/** Horizontal gap between the capsule's surface and the front face, cm. */
	UPROPERTY(BlueprintReadOnly, Category = "Parkour")
	float Distance = 0.f;

	/** Front face to back edge, cm; 0 when no back edge was found within reach. */
	UPROPERTY(BlueprintReadOnly, Category = "Parkour")
	float Depth = 0.f;

	/** A back edge within vault reach and a floor beyond it the capsule fits on. */
	UPROPERTY(BlueprintReadOnly, Category = "Parkour")
	bool bClearBeyond = false;

	/** Feet location on the far side (valid with bClearBeyond). */
	UPROPERTY(BlueprintReadOnly, Category = "Parkour")
	FVector LandingPoint = FVector::ZeroVector;

	/** How far the far-side floor is below the feet now, cm (negative: higher). */
	UPROPERTY(BlueprintReadOnly, Category = "Parkour")
	float LandingDrop = 0.f;

	/** The capsule fits on the top, or just over a thin top (a parapet) onto a floor close below it. */
	UPROPERTY(BlueprintReadOnly, Category = "Parkour")
	bool bStandingSurface = false;

	/** Feet location a mantle or climb ends at (valid with bStandingSurface). */
	UPROPERTY(BlueprintReadOnly, Category = "Parkour")
	FVector StandPoint = FVector::ZeroVector;

	/** The actor that owns the front face. */
	UPROPERTY(BlueprintReadOnly, Category = "Parkour")
	TObjectPtr<AActor> Actor = nullptr;
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

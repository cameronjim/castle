// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimEnums.h"
#include "Components/ActorComponent.h"
#include "Player/HawkeyeMovementTypes.h"
#include "ParkourComponent.generated.h"

class ACharacter;
class UAnimMontage;
class UAnimSequenceBase;
class UCharacterMovementComponent;
class USoundBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnParkourStartedSignature, EHawkeyeParkourMove, Move, EHawkeyeParkourRoute, Route);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnParkourFinishedSignature, EHawkeyeParkourMove, Move);

/** A stretch of one of the sample's traversal clips that poses the body for a move. */
USTRUCT(BlueprintType)
struct HAWKEYE_API FHawkeyeParkourClip
{
	GENERATED_BODY()

	/** Played in the full-body slot with root motion ignored; the capsule follows its root motion, warped. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parkour")
	TSoftObjectPtr<UAnimSequenceBase> Sequence;

	/** Clip time the move starts at, s. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parkour", meta = (ClampMin = "0.0"))
	float StartTime = 0.f;

	/** Clip time the move ends at, s. The stretch is played faster or slower to fit the move's duration. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parkour", meta = (ClampMin = "0.0"))
	float EndTime = 1.f;
};

/**
 * Vault, mantle and ledge grab (claude-docs/gameplay-semantics.md, traversal).
 *
 * A few traces ahead of the character find the front face, the top, whether the capsule fits on
 * (or just over) the top, and whether there is a back edge with a floor beyond it; ChooseMove
 * turns the numbers into a move. Sprinting into an obstacle within AutoTriggerDistance starts it
 * on its own; the jump key starts it at any speed. Vault and mantle go to the Game Animation
 * Sample's traversal first (GaspTraversal.h) when the owner carries it; when the sample finds no
 * traversable block or no montage fits, or the owner has no sample, this component moves the
 * capsule itself: along the root motion of a sample clip (or a plain arc when there is no
 * animation), warped so it starts where the character is and ends on the far side, on top, or
 * at the hang point. Input is locked exactly as long as the move lasts.
 *
 * The ledge grab has no sample equivalent: the character jumps to a hang (the sample's 2.5 m
 * climb clip paused where the hands reach the edge), then the jump key climbs and crouch drops.
 * Falling past a ledge between CatchMinHeight and CatchMaxHeight above the feet catches it.
 *
 * The way down: standing within DropToHangReach of an edge with more than DropToHangMinDrop
 * beyond it (a roof edge over its parapet, a fire-escape landing over its rail, or any walkable
 * edge), TryDropToHang goes over it to the same hang on its outer face. A drop from that hang
 * catches the next ledge below, so a fire escape comes down as hang, drop, catch, drop.
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = (Hawkeye), meta = (BlueprintSpawnableComponent))
class HAWKEYE_API UParkourComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UParkourComponent();

	/**
	 * The move for an obstacle Height cm above the feet: vault at VaultMinHeight to VaultMaxHeight
	 * when clear beyond (else mantle if there is somewhere to stand), mantle up to MantleMaxHeight
	 * with a standing surface, ledge grab up to LedgeMaxHeight, otherwise none.
	 */
	UFUNCTION(BlueprintPure, Category = "Parkour")
	EHawkeyeParkourMove ChooseMove(float Height, bool bClearBeyond, bool bStandingSurface) const;

	/**
	 * ChooseMove for a probed obstacle. A vault whose far floor is more than MaxAutoVaultDrop
	 * (auto) or MaxVaultDrop (jump key) below the feet counts as not clear: sprinting at a parapet
	 * never throws the character off the roof.
	 */
	UFUNCTION(BlueprintPure, Category = "Parkour")
	EHawkeyeParkourMove ChooseMoveFor(const FHawkeyeParkourObstacle& Obstacle, bool bAuto) const;

	/** Probes up to MaxDistance cm ahead of the capsule. False when there is no usable front face. */
	UFUNCTION(BlueprintCallable, Category = "Parkour")
	bool DetectObstacle(float MaxDistance, FHawkeyeParkourObstacle& OutObstacle) const;

	/** DetectObstacle for faces and tops up to MaxHeight above the feet (the catch looks higher). */
	bool DetectObstacleUpTo(float MaxDistance, float MaxHeight, FHawkeyeParkourObstacle& OutObstacle) const;

	/** The ledge the last drop let go of is still ignored (the fall has not ended). */
	bool IsIgnoringDroppedLedge() const { return bIgnoreDroppedLedge; }

	/** Probe and start whatever fits: bAuto is the sprint trigger, false the jump key. */
	UFUNCTION(BlueprintCallable, Category = "Parkour")
	bool TryParkour(bool bAuto);

	/** Starts Move over Obstacle with this component moving the capsule (no sample traversal). */
	UFUNCTION(BlueprintCallable, Category = "Parkour")
	bool StartMove(EHawkeyeParkourMove Move, const FHawkeyeParkourObstacle& Obstacle);

	/** Moves the current move DeltaSeconds on. Tick calls it; a test with no ticking calls it directly. */
	void AdvanceMove(float DeltaSeconds);

	/** From a hang: up onto the top. False when not hanging or there is nowhere to stand. */
	UFUNCTION(BlueprintCallable, Category = "Parkour")
	bool ClimbFromHang();

	/**
	 * From a hang: let go and fall. The same ledge is not caught again for DropRegrabSeconds, nor
	 * at all until the fall ends; any other ledge in the catch window is.
	 */
	UFUNCTION(BlueprintCallable, Category = "Parkour")
	bool DropFromHang();

	/**
	 * The edge Direction points at, if there is one to hang from: within DropToHangReach of the
	 * capsule, either a thin top at most MaxLipHeight high (a parapet or a rail) or the floor
	 * simply ending, with more than DropToHangMinDrop beyond it and room to hang on its outer
	 * face. WallNormal points out of that face (away from the character), LedgePoint is its top
	 * edge, LandingDrop the drop beyond, StandPoint where the character stands now.
	 */
	UFUNCTION(BlueprintCallable, Category = "Parkour")
	bool FindDropEdge(const FVector& Direction, FHawkeyeParkourObstacle& OutEdge) const;

	/**
	 * On the ground at an edge along Direction: over it to the hang (crouch moving toward an edge,
	 * or the jump key with the edge behind). False when there is no edge to hang from.
	 */
	UFUNCTION(BlueprintCallable, Category = "Parkour")
	bool TryDropToHang(const FVector& Direction, const FString& Trigger);

	/** Falling with a ledge in reach: catch it. Tick calls this while airborne. */
	UFUNCTION(BlueprintCallable, Category = "Parkour")
	bool TryCatchLedge();

	UFUNCTION(BlueprintPure, Category = "Parkour")
	bool IsHanging() const { return bHanging; }

	/** A move this component is driving (not the sample's). */
	UFUNCTION(BlueprintPure, Category = "Parkour")
	bool IsPerformingMove() const { return ActiveMove != EHawkeyeParkourMove::None; }

	/** The sample's traversal montage is running. */
	UFUNCTION(BlueprintPure, Category = "Parkour")
	bool IsSampleTraversalActive() const;

	/** Any traversal in progress: a move, a hang, or the sample's traversal. */
	UFUNCTION(BlueprintPure, Category = "Parkour")
	bool IsBusy() const { return IsPerformingMove() || bHanging || IsSampleTraversalActive(); }

	/**
	 * Movement input is ignored: during a move of ours and while hanging. The sample's traversal
	 * leaves input alone, because its montages blend out early on movement input.
	 */
	UFUNCTION(BlueprintPure, Category = "Parkour")
	bool IsLockingInput() const { return IsPerformingMove() || bHanging; }

	UFUNCTION(BlueprintPure, Category = "Parkour")
	EHawkeyeParkourMove GetActiveMove() const { return ActiveMove; }

	UFUNCTION(BlueprintPure, Category = "Parkour")
	EHawkeyeParkourMove GetLastMove() const { return LastMove; }

	UFUNCTION(BlueprintPure, Category = "Parkour")
	EHawkeyeParkourRoute GetLastRoute() const { return LastRoute; }

	UFUNCTION(BlueprintPure, Category = "Parkour")
	FHawkeyeParkourObstacle GetLastObstacle() const { return LastObstacle; }

	/** How long this component takes over Move, s. */
	UFUNCTION(BlueprintPure, Category = "Parkour")
	float GetMoveSeconds(EHawkeyeParkourMove Move) const;

	UPROPERTY(BlueprintAssignable, Category = "Parkour")
	FOnParkourStartedSignature OnParkourStarted;

	/** The effort of a vault, mantle, grab or climb: a short low grunt (MS_Vault_Grunt). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parkour|Audio")
	TSoftObjectPtr<USoundBase> EffortSound;

	/** Plays EffortSound for the moves that take effort (not a drop to a hang). */
	void PlayEffortSound(EHawkeyeParkourMove Move) const;

	UPROPERTY(BlueprintAssignable, Category = "Parkour")
	FOnParkourFinishedSignature OnParkourFinished;

	// --- Heights (above the feet), cm ---------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parkour|Heights", meta = (ClampMin = "0.0"))
	float VaultMinHeight = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parkour|Heights", meta = (ClampMin = "0.0"))
	float VaultMaxHeight = 110.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parkour|Heights", meta = (ClampMin = "0.0"))
	float MantleMaxHeight = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parkour|Heights", meta = (ClampMin = "0.0"))
	float LedgeMaxHeight = 260.f;

	/** Falling, a ledge at least this far above the feet (and at most CatchMaxHeight) is caught. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parkour|Heights", meta = (ClampMin = "0.0"))
	float CatchMinHeight = 150.f;

	/**
	 * Falling, a ledge up to this far above the feet is caught. One floor (330 cm) so a drop from
	 * one fire-escape landing reaches the next one down even when a frame is late.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parkour|Heights", meta = (ClampMin = "0.0"))
	float CatchMaxHeight = 330.f;

	// --- Detection ------------------------------------------------------------------------------

	/** Sprinting, an obstacle this close to the capsule starts a move by itself, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parkour|Detection", meta = (ClampMin = "0.0"))
	float AutoTriggerDistance = 120.f;

	/** The jump key looks this far ahead of the capsule, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parkour|Detection", meta = (ClampMin = "0.0"))
	float ManualTriggerDistance = 120.f;

	/** Falling, a wall this close to the capsule can be caught, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parkour|Detection", meta = (ClampMin = "0.0"))
	float CatchDistance = 60.f;

	/** Ground speed below which sprinting into something does not auto-trigger, cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parkour|Detection", meta = (ClampMin = "0.0"))
	float AutoMinSpeed = 300.f;

	/** A back edge further than this from the front face means the obstacle is not vaultable, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parkour|Detection", meta = (ClampMin = "0.0"))
	float MaxVaultDepth = 120.f;

	/** Auto vaults refuse a far floor this far below the feet, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parkour|Detection", meta = (ClampMin = "0.0"))
	float MaxAutoVaultDrop = 150.f;

	/** Jump-key vaults refuse a far floor this far below the feet (the roll height), cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parkour|Detection", meta = (ClampMin = "0.0"))
	float MaxVaultDrop = 400.f;

	/** A thin top (a parapet) can be climbed over onto a floor at most this far below it, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parkour|Detection", meta = (ClampMin = "0.0"))
	float ClimbOverMaxDrop = 120.f;

	/** An edge this close to the capsule (along the probe) can be dropped to a hang from, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parkour|Detection", meta = (ClampMin = "0.0"))
	float DropToHangReach = 60.f;

	/** Only an edge with more than this below it is one to hang from (less is a step down), cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parkour|Detection", meta = (ClampMin = "0.0"))
	float DropToHangMinDrop = 150.f;

	/** The tallest lip at an edge that still counts as one (a 90 cm parapet, a 98 cm rail), cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parkour|Detection", meta = (ClampMin = "0.0"))
	float MaxLipHeight = 130.f;

	/** The deepest lip that still counts (a 30 cm parapet); anything deeper is a raised floor, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parkour|Detection", meta = (ClampMin = "0.0"))
	float MaxLipDepth = 60.f;

	/** Vault and mantle try the Game Animation Sample's traversal first when the owner has it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parkour|Detection")
	bool bUseSampleTraversal = true;

	// --- Timing, s ------------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parkour|Timing", meta = (ClampMin = "0.05"))
	float VaultSeconds = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parkour|Timing", meta = (ClampMin = "0.05"))
	float MantleSeconds = 0.8f;

	/** Standing jump up to the hang. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parkour|Timing", meta = (ClampMin = "0.05"))
	float GrabSeconds = 0.35f;

	/** Falling onto the hang. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parkour|Timing", meta = (ClampMin = "0.05"))
	float CatchSeconds = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parkour|Timing", meta = (ClampMin = "0.05"))
	float ClimbSeconds = 1.f;

	/** Standing at an edge, over it and down to the hang. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parkour|Timing", meta = (ClampMin = "0.05"))
	float DropToHangSeconds = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parkour|Timing", meta = (ClampMin = "0.0"))
	float DropRegrabSeconds = 0.6f;

	// --- Hang -----------------------------------------------------------------------------------

	/**
	 * Feet below the ledge top while hanging, cm. The 2.5 m climb clip has them 151 cm down when the
	 * wrists reach the edge; 145 puts the hands over the top in the -game renders.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parkour|Hang")
	float HangBelowLedge = 145.f;

	/** Capsule centre out from the ledge edge while hanging, cm. More than the capsule radius. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parkour|Hang")
	float HangBackFromEdge = 36.f;

	// --- Clips ----------------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parkour|Clips")
	FHawkeyeParkourClip VaultClip;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parkour|Clips")
	FHawkeyeParkourClip MantleClip;

	/** Standing jump to the hang; its EndTime is the hang pose. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parkour|Clips")
	FHawkeyeParkourClip GrabClip;

	/** The last moment before the hang pose, for a catch while falling. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parkour|Clips")
	FHawkeyeParkourClip CatchClip;

	/** From the hang pose to standing on top. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parkour|Clips")
	FHawkeyeParkourClip ClimbClip;

	/** The montage slot the clips play in: the sample AnimBP's full-body slot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parkour|Clips")
	FName ClipSlot = TEXT("DefaultSlot");

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** Sprinting into something: TryParkour(true). */
	void TryAutoParkour();

	/** Nearest near-vertical face within Reach of the capsule axis, probed at several heights up to MaxHeight. */
	bool FindFrontFace(const FVector& Feet, const FVector& Forward, float Reach, float MaxHeight, FHitResult& OutHit) const;

	/** The top surface just behind the front face, with open air above it. */
	bool FindTop(const FHitResult& Face, const FVector& Normal, const FVector& Feet, float MaxHeight, FVector& OutTop) const;

	/** Back edge, far floor and whether the capsule fits there. */
	void ProbeBeyond(const FVector& Feet, FHawkeyeParkourObstacle& Obstacle) const;

	/** Room for the capsule on the top, or over a thin top onto a floor close below. */
	void ProbeStanding(FHawkeyeParkourObstacle& Obstacle) const;

	/** No blocking geometry overlaps the capsule with its centre at Centre. */
	bool CapsuleFits(const FVector& Centre) const;

	bool TraceLine(const FVector& From, const FVector& To, FHitResult& OutHit) const;

	/** The sample's TryTraversalAction; true when it started a montage. */
	bool TryStartSampleTraversal();

	/** Common start: face the wall, Flying, clip, state. Rise < 0 means End.Z - Start.Z. */
	bool BeginMove(EHawkeyeParkourMove Move, const FVector& End, float Rise, float Seconds, const FHawkeyeParkourClip& Clip);

	/** Where the capsule is Alpha of the way through the current move. */
	FVector ComputeMoveLocation(float Alpha) const;

	/** Forward (0..1 of the way) and up (in units of the rise) at Alpha, from the clip or an arc. */
	void ComputeMoveShape(float Alpha, float& OutForward, float& OutUp) const;

	/** Clip root motion from its start to Time, in the actor's frame: X forward, Z up. */
	FVector ClipRootOffset(float Time) const;

	void FinishMove();
	void EnterHang();
	FVector HangLocationFor(const FHawkeyeParkourObstacle& Obstacle) const;

	/** Starts Clip on the body (root motion ignored) so its stretch lasts Seconds. False without an AnimBP. */
	bool PlayClip(const FHawkeyeParkourClip& Clip, float Seconds);
	void StopClip(float BlendOutSeconds);
	void RestoreRootMotionMode();

	ACharacter* GetCharacter() const;
	UCharacterMovementComponent* GetMovement() const;

	// --- Runtime state --------------------------------------------------------------------------

	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Parkour")
	EHawkeyeParkourMove ActiveMove = EHawkeyeParkourMove::None;

	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Parkour")
	bool bHanging = false;

	UPROPERTY(Transient)
	EHawkeyeParkourMove LastMove = EHawkeyeParkourMove::None;

	UPROPERTY(Transient)
	EHawkeyeParkourRoute LastRoute = EHawkeyeParkourRoute::None;

	UPROPERTY(Transient)
	FHawkeyeParkourObstacle LastObstacle;

	/** The ledge being hung from (or jumped to). */
	UPROPERTY(Transient)
	FHawkeyeParkourObstacle HangObstacle;

	/** The clip posing the current move; null means the plain arc. */
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> MoveClip;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveMontage;

	FVector MoveStart = FVector::ZeroVector;
	FVector MoveEnd = FVector::ZeroVector;
	float MoveRise = 0.f;
	float MoveElapsed = 0.f;
	float MoveSeconds = 0.f;
	float ClipStart = 0.f;
	float ClipEnd = 0.f;
	/** The clip's root motion over the stretch: forward at the end, peak rise, rise at the end. */
	float ClipForwardTotal = 0.f;
	float ClipPeakUp = 0.f;
	float ClipEndUp = 0.f;
	float RegrabCooldown = 0.f;
	/** The ledge the last drop let go of: not caught again until the fall ends. */
	FVector DroppedLedgePoint = FVector::ZeroVector;
	bool bIgnoreDroppedLedge = false;
	/** Hop over the lip during a drop to hang, cm above a straight line. */
	float DropHop = 0.f;
	bool bRootMotionOverridden = false;
	TEnumAsByte<ERootMotionMode::Type> SavedRootMotionMode = ERootMotionMode::RootMotionFromMontagesOnly;
	TWeakObjectPtr<UActorComponent> SampleTraversal;
	bool bLoggedSampleApi = false;
};

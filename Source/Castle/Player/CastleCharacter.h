// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Player/CastleMovementTypes.h"
#include "Settings/CastleSettings.h"
#include "CastleCharacter.generated.h"

class UAnimSequence;
class UCameraComponent;
class UInputAction;
class USpringArmComponent;
class UInputMappingContext;
class UHealthComponent;
class UInteractionComponent;
class UInventoryComponent;
class UPawnNoiseEmitterComponent;
class UTakedownComponent;
class UWeaponComponent;
struct FInputActionValue;

/**
 * The player character, in third person: a spring arm behind and above the body that probes
 * against the world, the camera on the end of it, and the body turning to face the way it moves
 * (or the camera, while aiming). Aim blends the arm in to the right shoulder.
 *
 * Movement picks a gait every frame (walk, run, sprint, crouch, slide) from how hard the stick
 * is pushed and for how long; every number is a property so BP_Kate can tune it. Landings are
 * measured from the top of the arc: a long drop dips speed and camera (the roll placeholder)
 * and a very long one costs health, never all of it.
 *
 * TODO(stage2): body animation is the idle/walk/run helper in LocomotionAnim.h until the Game
 * Animation Sample's motion-matched locomotion replaces it.
 *
 * BP_CastleCharacter is the input-wired base; BP_Kate is the playable child.
 */
UCLASS(Blueprintable, BlueprintType)
class CASTLE_API ACastleCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ACastleCharacter();

	/** The boom the camera hangs off. Rotates with the control rotation. */
	UFUNCTION(BlueprintPure, Category = "Castle|Character")
	USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** The player's view, on the end of the boom. */
	UFUNCTION(BlueprintPure, Category = "Castle|Character")
	UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	UFUNCTION(BlueprintPure, Category = "Castle|Character")
	UHealthComponent* GetHealthComponent() const { return HealthComponent; }

	UFUNCTION(BlueprintPure, Category = "Castle|Character")
	UTakedownComponent* GetTakedownComponent() const { return TakedownComponent; }

	/** Always present; bHasWeapon is false while Hands are the active slot. */
	UFUNCTION(BlueprintPure, Category = "Castle|Character")
	UWeaponComponent* GetWeaponComponent() const;

	UFUNCTION(BlueprintPure, Category = "Castle|Character")
	UInteractionComponent* GetInteractionComponent() const { return InteractionComponent; }

	/** The hotbar and keycard ring. Always present on the player. */
	UFUNCTION(BlueprintPure, Category = "Castle|Character")
	UInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }

	/** True once GiveKeycard(KeycardId) has been called for that id. Forwards to the inventory. */
	UFUNCTION(BlueprintPure, Category = "Castle|Character")
	bool HasKeycard(FName KeycardId) const;

	/** Adds a keycard to the player's ring. Returns false when they already had it. */
	UFUNCTION(BlueprintCallable, Category = "Castle|Character")
	bool GiveKeycard(FName KeycardId);

	/** Keycards picked up so far. Doors check this by id. */
	UFUNCTION(BlueprintPure, Category = "Castle|Character")
	TSet<FName> GetKeycards() const;

	/**
	 * Loudness the player is currently emitting, per claude-docs/gameplay-semantics.md:
	 * 1.0 sprinting, 0.4 walking, 0 crouching or airborne.
	 */
	UFUNCTION(BlueprintPure, Category = "Castle|Movement")
	float GetMovementNoiseLoudness() const;

	/** True while a takedown animation is playing; movement and firing are ignored. */
	UFUNCTION(BlueprintPure, Category = "Castle|Character")
	bool IsLockedOutByTakedown() const;

	/**
	 * Starts aiming: the camera blends in to AimCamera over the right shoulder, the body turns to
	 * face the camera, the speed drops to WalkSpeed, and the weapon tightens its spread cone.
	 */
	UFUNCTION(BlueprintCallable, Category = "Castle|Aim")
	void StartAim();

	/** Ends aiming. Safe to call when not aiming; sprinting calls it. */
	UFUNCTION(BlueprintCallable, Category = "Castle|Aim")
	void StopAim();

	UFUNCTION(BlueprintPure, Category = "Castle|Aim")
	bool IsAiming() const { return bIsAiming; }

	UFUNCTION(BlueprintPure, Category = "Castle|Movement")
	bool IsSprinting() const { return bIsSprinting; }

	/** Current camera field of view. Exposed so a test or a Blueprint can read the blend. */
	UFUNCTION(BlueprintPure, Category = "Castle|Aim")
	float GetCurrentFOV() const;

	/** LookSensitivity, reduced by AimLookMultiplier while aiming. */
	UFUNCTION(BlueprintPure, Category = "Input")
	float GetEffectiveLookSensitivity() const;

	/**
	 * The yaw/pitch delta a raw look input produces. Split out of Input_Look so the sensitivity
	 * rules can be tested without an input stack.
	 */
	UFUNCTION(BlueprintPure, Category = "Input")
	FVector2D ComputeLookDelta(FVector2D RawInput, bool bAiming) const;

	// --- Camera ---------------------------------------------------------------------------------

	/** Arm length, shoulder offset and FOV the camera settles at for this aim state. No blending. */
	UFUNCTION(BlueprintPure, Category = "Castle|Camera")
	FCastleCameraTargets ComputeCameraTargets(bool bAiming) const;

	/** 0 at the hip, 1 fully aimed; moves over AimBlendSeconds. */
	UFUNCTION(BlueprintPure, Category = "Castle|Camera")
	float GetAimBlendAlpha() const { return AimAlpha; }

	// --- Gait -----------------------------------------------------------------------------------

	/**
	 * Which gait these inputs ask for. Slide beats crouch beats sprint beats aim; otherwise a full
	 * stick (or any keyboard press) runs, a medium stick runs once held for RunAfterHeldSeconds,
	 * and a light one walks.
	 */
	UFUNCTION(BlueprintPure, Category = "Castle|Movement")
	ECastleGait SelectGait(float InputMagnitude, float InputHeldSeconds, bool bWantsSprint, bool bCrouched,
		bool bSliding, bool bAiming) const;

	/** Top ground speed for a gait, cm/s. The slide starts at SlideSpeed and eases to CrouchSpeed. */
	UFUNCTION(BlueprintPure, Category = "Castle|Movement")
	float GetGaitSpeed(ECastleGait Gait) const;

	/** The gait chosen on the last update. */
	UFUNCTION(BlueprintPure, Category = "Castle|Movement")
	ECastleGait GetGait() const { return CurrentGait; }

	UFUNCTION(BlueprintPure, Category = "Castle|Movement")
	bool IsSliding() const { return bIsSliding; }

	/**
	 * Starts a slide: only while sprinting on the ground above SlideMinSpeed. Shrinks the capsule
	 * to the crouched height for SlideSeconds, then stands back up. Returns true when it started.
	 */
	UFUNCTION(BlueprintCallable, Category = "Castle|Movement")
	bool StartSlide();

	// --- Falling --------------------------------------------------------------------------------

	/**
	 * Fraction of MaxHealth a fall of this height costs: 0 below FallDamageMinHeight, then
	 * FallDamageMinFraction rising to FallDamageMaxFraction at FallDamageMaxHeight and capped there.
	 */
	UFUNCTION(BlueprintPure, Category = "Castle|Falling")
	float ComputeFallDamageFraction(float FallHeight) const;

	/** Height of the last landing, from the top of the arc to the ground, cm. */
	UFUNCTION(BlueprintPure, Category = "Castle|Falling")
	float GetLastFallHeight() const { return LastFallHeight; }

	/** How far below the top of the current arc the character is, cm. 0 on the ground. */
	UFUNCTION(BlueprintPure, Category = "Castle|Falling")
	float GetCurrentFallHeight() const;

	/** True for LandingRecoverSeconds after a landing from above RollHeight. */
	UFUNCTION(BlueprintPure, Category = "Castle|Falling")
	bool IsRecoveringFromLanding() const { return LandingRecoverRemaining > 0.f; }

	// --- Debug ----------------------------------------------------------------------------------

	/** One line for the HUD: gait, ground speed, fall height. Shown when castle.DebugMovement is 1. */
	UFUNCTION(BlueprintPure, Category = "Castle|Debug")
	FString GetMovementDebugText() const;

	/** Reads the castle.DebugMovement console variable. */
	UFUNCTION(BlueprintPure, Category = "Castle|Debug")
	static bool IsMovementDebugEnabled();

	/** Ends a slide before jumping: a character cannot jump while crouched. */
	virtual void Jump() override;

	/** Fired when the Interact action is pressed; implement in Blueprint to drive doors, levers, pickups. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Castle|Character")
	void OnInteractPressed();

protected:
	//~ Begin APawn interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PawnClientRestart() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	//~ End APawn interface

	//~ Begin ACharacter interface
	virtual void Landed(const FHitResult& Hit) override;
	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode = 0) override;
	//~ End ACharacter interface

	/** Adds DefaultMappingContext to the local player's Enhanced Input subsystem. */
	void AddDefaultMappingContext();

	// --- Input handlers -------------------------------------------------------------------------
	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);
	void Input_SprintStarted(const FInputActionValue& Value);
	void Input_SprintCompleted(const FInputActionValue& Value);
	void Input_CrouchToggle(const FInputActionValue& Value);
	void Input_Fire(const FInputActionValue& Value);
	void Input_Reload(const FInputActionValue& Value);
	void Input_Takedown(const FInputActionValue& Value);
	void Input_Interact(const FInputActionValue& Value);
	void Input_AimStarted(const FInputActionValue& Value);
	void Input_AimCompleted(const FInputActionValue& Value);
	void Input_Slot1(const FInputActionValue& Value);
	void Input_Slot2(const FInputActionValue& Value);
	void Input_Slot3(const FInputActionValue& Value);
	void Input_SlotScroll(const FInputActionValue& Value);
	void Input_Inventory(const FInputActionValue& Value);

	/** Picks the gait and writes its speed to CharacterMovement (walking and crouched). */
	void UpdateMaxWalkSpeed();

	/** Moves the aim blend one frame and writes arm length, socket offset and FOV. */
	void UpdateCamera(float DeltaSeconds);

	/** Hides the body from its own camera while a wall has pulled the lens in to it. */
	void UpdateBodyVisibilityForCamera();

	/** Accumulates how long move input has been held; resets on the first frame without it. */
	void UpdateMoveInputTiming(float DeltaSeconds);

	/** Counts the slide down, steers it, and stands up at the end. */
	void UpdateSlide(float DeltaSeconds);

	/** Tracks the top of the arc while falling and counts the landing recovery down. */
	void UpdateFalling(float DeltaSeconds);

	/** Stands back up and restores friction. Safe when not sliding. */
	void EndSlide();

	/** Roll placeholder and fall damage for a landing FallHeight below the top of the arc. */
	void ApplyLanding(float FallHeight);

	/** Swaps the body between IdleAnim, WalkAnim, RunAnim and FallAnim. See LocomotionAnim.h. */
	void UpdateBodyLocomotion();

	/** Aiming faces the camera; otherwise the body turns towards where it moves. */
	void ApplyRotationMode();

	/**
	 * Copies the tuning properties onto the boom and CharacterMovement. Run at BeginPlay so a
	 * Blueprint child's values win over the constructor's.
	 */
	void ApplyTuningToComponents();

	// --- Components -----------------------------------------------------------------------------

	/**
	 * Turned by the control rotation; lags behind in position and rotation; probes with a sphere on
	 * the Camera channel and pulls in against walls. Length and offset are driven every frame.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Castle|Components")
	TObjectPtr<USpringArmComponent> CameraBoom;

	/** On the end of CameraBoom. The boom does the rotating, so this does not follow the pawn. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Castle|Components")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Castle|Components")
	TObjectPtr<UHealthComponent> HealthComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Castle|Components")
	TObjectPtr<UTakedownComponent> TakedownComponent;

	/** Starts on Hands (bHasWeapon false). The inventory hands it whatever slot is active. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Castle|Components")
	TObjectPtr<UWeaponComponent> WeaponComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Castle|Components")
	TObjectPtr<UInteractionComponent> InteractionComponent;

	/** Three hotbar slots and the keycard ring. Hands are always in slot 0. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Castle|Components")
	TObjectPtr<UInventoryComponent> InventoryComponent;

	/** What AISense_Hearing listens to. MakeNoise routes through this. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Castle|Components")
	TObjectPtr<UPawnNoiseEmitterComponent> NoiseEmitter;

	// --- Input assets ---------------------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (ClampMin = "0"))
	int32 DefaultMappingPriority = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> SprintAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> CrouchAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> FireAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> AimAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ReloadAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> TakedownAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> InteractAction;

	/** Number key 1: Hands. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> Slot1Action;

	/** Number key 2: the second hotbar slot. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> Slot2Action;

	/** Number key 3: the third hotbar slot. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> Slot3Action;

	/** Mouse wheel. Positive is the next slot, negative the previous; empty slots are skipped. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> SlotScrollAction;

	/** Tab: opens the read-only inventory screen, which pauses like the pause menu does. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> InventoryAction;

	// --- Movement tuning ------------------------------------------------------------------------

	/** Light stick input, and the speed while aiming. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Castle|Movement", meta = (ClampMin = "0.0"))
	float WalkSpeed = 250.f;

	/** The default on foot. Keyboard input is always full, so the keyboard runs unless sprinting. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Castle|Movement", meta = (ClampMin = "0.0"))
	float RunSpeed = 500.f;

	/** Sprint held. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Castle|Movement", meta = (ClampMin = "0.0"))
	float SprintSpeed = 700.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Castle|Movement", meta = (ClampMin = "0.0"))
	float CrouchSpeed = 200.f;

	/** Stick deflection at or above which the character runs straight away. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Castle|Movement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FullInputThreshold = 0.9f;

	/** Stick deflection below which the character always walks. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Castle|Movement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LightInputThreshold = 0.4f;

	/** A medium stick breaks from walk into run after being held this long. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Castle|Movement", meta = (ClampMin = "0.0"))
	float RunAfterHeldSeconds = 0.2f;

	/** Apex of a standing jump, cm. JumpZVelocity is derived from it and gravity at BeginPlay. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Castle|Movement", meta = (ClampMin = "0.0"))
	float JumpHeight = 90.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Castle|Movement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float JumpAirControl = 0.3f;

	/** Yaw the body turns at, degrees per second, towards movement or (aiming) the camera. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Castle|Movement", meta = (ClampMin = "0.0"))
	float TurnRateDegrees = 720.f;

	/** Sprint input is held. Only moves the character at SprintSpeed while it is on its feet. */
	UPROPERTY(BlueprintReadOnly, Category = "Castle|Movement")
	bool bIsSprinting = false;

	// --- Slide ----------------------------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Castle|Slide", meta = (ClampMin = "0.05"))
	float SlideSeconds = 0.7f;

	/** Speed at the start of a slide; eases down to CrouchSpeed by the end. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Castle|Slide", meta = (ClampMin = "0.0"))
	float SlideSpeed = 750.f;

	/** A sprint slower than this crouches instead of sliding. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Castle|Slide", meta = (ClampMin = "0.0"))
	float SlideMinSpeed = 350.f;

	/** Ground friction while sliding; low so the slide carries. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Castle|Slide", meta = (ClampMin = "0.0"))
	float SlideGroundFriction = 0.5f;

	/** Capsule half-height while crouched or sliding, cm. Applied at BeginPlay. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Castle|Slide", meta = (ClampMin = "10.0"))
	float CrouchedCapsuleHalfHeight = 50.f;

	// --- Falling --------------------------------------------------------------------------------

	/** A landing from above this height rolls: a short speed and camera dip. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Castle|Falling", meta = (ClampMin = "0.0"))
	float RollHeight = 400.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Castle|Falling", meta = (ClampMin = "0.0"))
	float LandingRecoverSeconds = 0.3f;

	/** Speed multiplier for the length of the roll. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Castle|Falling", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LandingSpeedMultiplier = 0.5f;

	/** How far the camera drops at the bottom of the roll, cm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Castle|Falling", meta = (ClampMin = "0.0"))
	float LandingCameraDip = 30.f;

	/** Falls shorter than this cost nothing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Castle|Falling", meta = (ClampMin = "0.0"))
	float FallDamageMinHeight = 900.f;

	/** Falls from here up cost FallDamageMaxFraction; about the tallest tenement on the block. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Castle|Falling", meta = (ClampMin = "0.0"))
	float FallDamageMaxHeight = 2500.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Castle|Falling", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FallDamageMinFraction = 0.1f;

	/** The cap. A fall never kills either way: damage stops one point short of the last. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Castle|Falling", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FallDamageMaxFraction = 0.6f;

	// --- Camera ---------------------------------------------------------------------------------

	/** Where the camera sits while not aiming: behind, above and a little right, so the body sits left of centre. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Castle|Camera")
	FCastleCameraTargets HipCamera = { 350.f, FVector(0.f, 70.f, 60.f), 90.f };

	/** Where it sits while aiming: in close over the right shoulder. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Castle|Camera")
	FCastleCameraTargets AimCamera = { 180.f, FVector(0.f, 45.f, 55.f), 70.f };

	/** Seconds the camera takes to travel the whole way between HipCamera and AimCamera. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Castle|Camera", meta = (ClampMin = "0.0"))
	float AimBlendSeconds = 0.15f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Castle|Camera", meta = (ClampMin = "0.0"))
	float CameraLagSpeed = 10.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Castle|Camera", meta = (ClampMin = "0.0"))
	float CameraRotationLagSpeed = 12.f;

	/** Radius of the sphere the arm sweeps to find walls. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Castle|Camera", meta = (ClampMin = "0.0"))
	float CameraProbeSize = 12.f;

	/**
	 * When a wall pulls the lens closer than this to the arm's pivot, the body is hidden from the
	 * player's view (it still casts its shadow). Otherwise backing into a tenement fills the
	 * screen with the inside of Kate's shoulder.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Castle|Camera", meta = (ClampMin = "0.0"))
	float CameraHideBodyDistance = 100.f;

	// --- Look -----------------------------------------------------------------------------------

	/**
	 * Multiplier on the raw Look input. The mouse mapping is 1 degree per unit, which is far
	 * too fast to hold an aim; this is the one number to change when the mouse feels wrong.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input", meta = (ClampMin = "0.02"))
	float LookSensitivity = 0.2f;

	/**
	 * The sensitivity UCastleSettingsSubsystem last handed us. The subsystem wins whenever one
	 * exists; LookSensitivity above is only the fallback for a world without a game instance,
	 * which is every automation test.
	 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Input")
	float SettingsLookSensitivity = 0.f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Input")
	bool bHasSettingsLookSensitivity = false;

	/** LookSensitivity is multiplied by this again while aiming. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float AimLookMultiplier = 0.7f;

	/** Reads the current sensitivity out of the settings subsystem and subscribes to changes. */
	void BindToSettingsSubsystem();

	void UnbindFromSettingsSubsystem();

	UFUNCTION()
	void HandleSettingsChanged(FCastleSettings Settings);

	// --- Aim ------------------------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Category = "Castle|Aim")
	bool bIsAiming = false;

	// --- Animation ------------------------------------------------------------------------------

	/** Body animation while standing still. The same sequence the thugs use. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Castle|Animation")
	TObjectPtr<UAnimSequence> IdleAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Castle|Animation")
	TObjectPtr<UAnimSequence> WalkAnim;

	/** Played above RunAnimSpeedThreshold. Falls back to WalkAnim when unset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Castle|Animation")
	TObjectPtr<UAnimSequence> RunAnim;

	/** Played while falling. Falls back to the ground animation when unset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Castle|Animation")
	TObjectPtr<UAnimSequence> FallAnim;

	/** WalkAnim above this much ground speed, IdleAnim below it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Castle|Animation", meta = (ClampMin = "0.0"))
	float WalkAnimSpeedThreshold = 20.f;

	/** RunAnim above this much ground speed; between walk and run speed so each reads. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Castle|Animation", meta = (ClampMin = "0.0"))
	float RunAnimSpeedThreshold = 375.f;

	// --- Noise ----------------------------------------------------------------------------------

	/** Seconds between movement noise events while moving. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Castle|Noise", meta = (ClampMin = "0.01"))
	float NoiseIntervalSeconds = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Castle|Noise", meta = (ClampMin = "0.0"))
	float SprintNoiseLoudness = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Castle|Noise", meta = (ClampMin = "0.0"))
	float WalkNoiseLoudness = 0.4f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Castle|Noise", meta = (ClampMin = "0.0"))
	float GunshotNoiseLoudness = 3.f;

	/** Timer body: emits one movement noise event if the player is making any. */
	void EmitMovementNoise();

	UFUNCTION()
	void HandleDeath(UHealthComponent* Health, AActor* Killer);

	// --- Runtime state --------------------------------------------------------------------------

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Castle|Movement")
	ECastleGait CurrentGait = ECastleGait::Walk;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Castle|Slide")
	bool bIsSliding = false;

	/** Seconds of slide left. */
	UPROPERTY(Transient)
	float SlideRemaining = 0.f;

	/** Ground direction the slide travels in, fixed when it starts. */
	UPROPERTY(Transient)
	FVector SlideDirection = FVector::ForwardVector;

	/** GroundFriction before the slide lowered it. */
	UPROPERTY(Transient)
	float PreSlideGroundFriction = 8.f;

	/** Length of the last move input, 0..1. Keyboard is always 1. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Castle|Movement")
	float MoveInputMagnitude = 0.f;

	/** How long move input has been held without a break. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Castle|Movement")
	float MoveInputHeldSeconds = 0.f;

	/** Set by Input_Move, cleared by the next Tick: how the tick knows the stick was let go. */
	bool bMoveInputThisFrame = false;

	/** Highest capsule Z since the character last left the ground. */
	UPROPERTY(Transient)
	float FallApexZ = 0.f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Castle|Falling")
	float LastFallHeight = 0.f;

	UPROPERTY(Transient)
	float LandingRecoverRemaining = 0.f;

	/** Aim blend, 0 hip to 1 aimed, moved at 1 / AimBlendSeconds. */
	UPROPERTY(Transient)
	float AimAlpha = 0.f;

private:
	FTimerHandle NoiseTimerHandle;

	/** Whichever of IdleAnim / WalkAnim the body is playing, so Tick only re-plays on a change. */
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> CurrentLocomotionAnim;
};

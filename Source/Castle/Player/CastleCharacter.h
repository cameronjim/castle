// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CastleCharacter.generated.h"

class UAnimSequence;
class UCameraComponent;
class UInputAction;
class UPointLightComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UInputMappingContext;
class UHealthComponent;
class UInteractionComponent;
class UPawnNoiseEmitterComponent;
class UTakedownComponent;
class UWeaponComponent;
struct FInputActionValue;

/**
 * First-person player character. Create a Blueprint child (BP_CastleCharacter), assign the
 * Input assets, add a mesh, and add a UWeaponComponent for shooting.
 */
UCLASS(Blueprintable, BlueprintType)
class CASTLE_API ACastleCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ACastleCharacter();

	UFUNCTION(BlueprintPure, Category = "Castle|Character")
	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }

	UFUNCTION(BlueprintPure, Category = "Castle|Character")
	UHealthComponent* GetHealthComponent() const { return HealthComponent; }

	UFUNCTION(BlueprintPure, Category = "Castle|Character")
	UTakedownComponent* GetTakedownComponent() const { return TakedownComponent; }

	/** Always present; bHasWeapon is false until a pistol pickup arms it. */
	UFUNCTION(BlueprintPure, Category = "Castle|Character")
	UWeaponComponent* GetWeaponComponent() const;

	UFUNCTION(BlueprintPure, Category = "Castle|Character")
	UInteractionComponent* GetInteractionComponent() const { return InteractionComponent; }

	/** True once GiveKeycard(KeycardId) has been called for that id. */
	UFUNCTION(BlueprintPure, Category = "Castle|Character")
	bool HasKeycard(FName KeycardId) const;

	/** Adds a keycard to the player's ring. Returns false when they already had it. */
	UFUNCTION(BlueprintCallable, Category = "Castle|Character")
	bool GiveKeycard(FName KeycardId);

	/** Keycards picked up so far. Doors check this by id. */
	UFUNCTION(BlueprintPure, Category = "Castle|Character")
	TSet<FName> GetKeycards() const { return Keycards; }

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
	 * Starts aiming down sights: the camera blends to AimFOV, the walk speed drops to
	 * WalkSpeed * AimSpeedMultiplier, and the weapon tightens its spread cone.
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

	/** Fired when the Interact action is pressed; implement in Blueprint to drive doors, levers, pickups. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Castle|Character")
	void OnInteractPressed();

	// --- View model -----------------------------------------------------------------------------

	/** Arms rendered in front of the camera. Owner-only, no shadow. */
	UFUNCTION(BlueprintPure, Category = "Castle|ViewModel")
	USkeletalMeshComponent* GetArmsMesh() const { return ArmsMesh; }

	/** The pistol in the arms' right hand. Hidden until bHasWeapon. */
	UFUNCTION(BlueprintPure, Category = "Castle|ViewModel")
	UStaticMeshComponent* GetWeaponMesh() const { return WeaponMesh; }

	/** Kicks the arms back and up, then settles them. Called for every shot that goes out. */
	UFUNCTION(BlueprintCallable, Category = "Castle|ViewModel")
	void PlayFireFeedback();

	/** Plays the pistol idle pose and shows the weapon, or the empty-handed pose and hides it. */
	UFUNCTION(BlueprintCallable, Category = "Castle|ViewModel")
	void RefreshViewModelForWeapon();

	/** Offset the view model is currently drawn at, relative to its rest pose. Exposed for tests. */
	UFUNCTION(BlueprintPure, Category = "Castle|ViewModel")
	FVector GetViewModelOffset() const;

protected:
	//~ Begin APawn interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PawnClientRestart() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	//~ End APawn interface

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

	/** Walk speed for the current sprint/aim combination, written to CharacterMovement. */
	void UpdateMaxWalkSpeed();

	/** Moves the camera FOV one frame towards its target. */
	void UpdateAimFOV(float DeltaSeconds);

	// --- View model -----------------------------------------------------------------------------

	/** Hides the bones that are not arms and plays the resting pose. Runs once at BeginPlay. */
	void InitialiseViewModel();

	/** Advances the recoil, reload dip and sway clocks and writes the arms' relative transform. */
	void UpdateViewModel(float DeltaSeconds);

	/** Where the recoil curve is this frame: 0 at rest, 1 fully kicked back. */
	float GetRecoilAlpha() const;

	/** Turns the muzzle flash light off again. Timer body. */
	void EndMuzzleFlash();

	// --- Components -----------------------------------------------------------------------------

	/** Eye-height camera attached to the capsule and driven by control rotation. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Castle|Components")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Castle|Components")
	TObjectPtr<UHealthComponent> HealthComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Castle|Components")
	TObjectPtr<UTakedownComponent> TakedownComponent;

	/** Starts disarmed (bHasWeapon false); BP_Pickup_Pistol calls GiveWeapon on it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Castle|Components")
	TObjectPtr<UWeaponComponent> WeaponComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Castle|Components")
	TObjectPtr<UInteractionComponent> InteractionComponent;

	/** What AISense_Hearing listens to. MakeNoise routes through this. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Castle|Components")
	TObjectPtr<UPawnNoiseEmitterComponent> NoiseEmitter;

	/**
	 * Optional first-person arms. UE 5.8 ships no arms-only skeletal mesh, and the full body
	 * mannequin wraps its torso and shoulders around the camera, so this is off by default
	 * (bUseArmsMesh false) and the pistol alone is the view model.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Castle|Components")
	TObjectPtr<USkeletalMeshComponent> ArmsMesh;

	/** The view model pistol. Attached to the camera, or to WeaponSocketName when arms are on. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Castle|Components")
	TObjectPtr<UStaticMeshComponent> WeaponMesh;

	/** Flashed for MuzzleFlashSeconds on every shot. There is no Niagara system yet. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Castle|Components")
	TObjectPtr<UPointLightComponent> MuzzleFlash;

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

	// --- Movement tuning ------------------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Castle|Movement", meta = (ClampMin = "0.0"))
	float WalkSpeed = 450.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Castle|Movement", meta = (ClampMin = "0.0"))
	float SprintSpeed = 750.f;

	/** Camera height above the capsule centre. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Castle|Character")
	float EyeHeightOffset = 64.f;

	UPROPERTY(BlueprintReadOnly, Category = "Castle|Movement")
	bool bIsSprinting = false;

	// --- Look -----------------------------------------------------------------------------------

	/**
	 * Multiplier on the raw Look input. The mouse mapping is 1 degree per unit, which is far
	 * too fast to hold an aim; this is the one number to change when the mouse feels wrong.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input", meta = (ClampMin = "0.05"))
	float LookSensitivity = 0.45f;

	/** LookSensitivity is multiplied by this again while aiming down sights. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float AimLookMultiplier = 0.7f;

	// --- Aim ------------------------------------------------------------------------------------

	/** Field of view when hip-firing. The camera starts here and returns here. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Castle|Aim", meta = (ClampMin = "10.0", ClampMax = "170.0"))
	float HipFOV = 90.f;

	/** Field of view while aiming down sights. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Castle|Aim", meta = (ClampMin = "10.0", ClampMax = "170.0"))
	float AimFOV = 70.f;

	/** Seconds the camera takes to travel the whole way between HipFOV and AimFOV. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Castle|Aim", meta = (ClampMin = "0.0"))
	float AimBlendSeconds = 0.15f;

	/** WalkSpeed is multiplied by this while aiming. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Castle|Aim", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AimSpeedMultiplier = 0.6f;

	UPROPERTY(BlueprintReadOnly, Category = "Castle|Aim")
	bool bIsAiming = false;

	// --- View model -----------------------------------------------------------------------------

	/**
	 * Off by default. The only arms mesh available is the full body mannequin, whose torso and
	 * shoulders surround the camera; turn this on only once a real arms-only mesh exists.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Castle|ViewModel")
	bool bUseArmsMesh = false;

	/** Rest pose of the arms relative to the camera: down and forward so only the hands show. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Castle|ViewModel")
	FVector ArmsRelativeLocation = FVector(12.f, 0.f, -152.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Castle|ViewModel")
	FRotator ArmsRelativeRotation = FRotator(0.f, -90.f, 0.f);

	/**
	 * Bones hidden so the full-body mannequin reads as a pair of arms. Hiding a bone hides its
	 * children, so the legs go with the thighs and the head goes with the neck.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Castle|ViewModel")
	TArray<FName> HiddenViewModelBones;

	/** Socket or bone on ArmsMesh the weapon hangs off. Only used while bUseArmsMesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Castle|ViewModel")
	FName WeaponSocketName = FName(TEXT("hand_r"));

	/**
	 * Hip rest pose of the pistol in camera space: X forward, Y right, Z up. Lower right of
	 * frame, barrel down the camera's forward axis.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Castle|ViewModel")
	FVector WeaponRelativeLocation = FVector(42.f, 18.f, -14.f);

	/**
	 * SM_Pistol is modelled barrel-along-+Y (its bounds run y -5.3..20.9, x only -2.9..3.1),
	 * so a -90 degree yaw is what puts the muzzle down the camera's forward axis.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Castle|ViewModel")
	FRotator WeaponRelativeRotation = FRotator(0.f, -90.f, 0.f);

	/**
	 * Where the pistol sits while aiming: centred, and dropped far enough that the top of the
	 * slide - the sights - lands on the crosshair rather than the middle of the slide.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Castle|ViewModel")
	FVector WeaponAimLocation = FVector(38.f, 0.f, -11.5f);

	/** Empty-handed pose. Optional: with no animation asset the arms hold their reference pose. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Castle|ViewModel|Animation")
	TObjectPtr<UAnimSequence> ArmsIdleAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Castle|ViewModel|Animation")
	TObjectPtr<UAnimSequence> ArmsPistolIdleAnim;

	/** Played once per shot. With none set, the procedural recoil kick carries the feedback. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Castle|ViewModel|Animation")
	TObjectPtr<UAnimSequence> ArmsFireAnim;

	/** Played on reload. With none set, the arms dip out of frame and back over ReloadSeconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Castle|ViewModel|Animation")
	TObjectPtr<UAnimSequence> ArmsReloadAnim;

	/** How far back the arms travel at the peak of the recoil, in centimetres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Castle|ViewModel|Recoil", meta = (ClampMin = "0.0"))
	float RecoilKickDistance = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Castle|ViewModel|Recoil", meta = (ClampMin = "0.0"))
	float RecoilKickPitchDegrees = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Castle|ViewModel|Recoil", meta = (ClampMin = "0.0"))
	float RecoilKickSeconds = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Castle|ViewModel|Recoil", meta = (ClampMin = "0.0"))
	float RecoilReturnSeconds = 0.15f;

	/** How far the arms drop out of frame during a reload with no reload animation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Castle|ViewModel", meta = (ClampMin = "0.0"))
	float ReloadDipDistance = 22.f;

	/** Centimetres of bob at full sprint speed. Scaled by the pawn's actual speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Castle|ViewModel|Sway", meta = (ClampMin = "0.0"))
	float SwayAmplitude = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Castle|ViewModel|Sway", meta = (ClampMin = "0.0"))
	float SwayCyclesPerSecond = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Castle|ViewModel", meta = (ClampMin = "0.0"))
	float MuzzleFlashSeconds = 0.05f;

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

	/** Keycard ids collected so far. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Castle|Character")
	TSet<FName> Keycards;

	/** Timer body: emits one movement noise event if the player is making any. */
	void EmitMovementNoise();

	UFUNCTION()
	void HandleDeath(UHealthComponent* Health, AActor* Killer);

private:
	FTimerHandle NoiseTimerHandle;
	FTimerHandle MuzzleFlashTimerHandle;

	/** Seconds since the last shot, for the recoil curve. Negative means "no shot yet". */
	float RecoilElapsed = -1.f;

	/** Sway phase, advanced by speed rather than by time so standing still is still. */
	float SwayPhase = 0.f;

	/** 0 hip, 1 fully aimed. Follows the same clock as the FOV blend. */
	float AimOffsetAlpha = 0.f;

	/** What bHasWeapon was last frame, so the arms only re-pose when it actually changes. */
	bool bViewModelArmed = false;
};

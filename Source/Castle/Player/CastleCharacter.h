// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
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
 * The player character, in third person: a spring arm behind and above the body, the camera on
 * the end of it, and the body turning to face the way it moves. The mesh is fully visible and
 * the idle/walk helper in LocomotionAnim.h drives it.
 *
 * TODO(stage2): this is the placeholder rig. The camera task replaces it with the tuned one
 * (aim offset to the shoulder, collision probe, lag tuning); do not tune the numbers here.
 *
 * Create a Blueprint child (BP_CastleCharacter), assign the Input assets, and let
 * Tools/Editor/create_blueprints.py fill in the mesh and animations.
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
	 * Starts aiming: the camera blends to AimFOV, the walk speed drops to
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

	/**
	 * The yaw/pitch delta a raw look input produces. Split out of Input_Look so the sensitivity
	 * rules can be tested without an input stack.
	 */
	UFUNCTION(BlueprintPure, Category = "Input")
	FVector2D ComputeLookDelta(FVector2D RawInput, bool bAiming) const;

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

	/** Walk speed for the current sprint/aim combination, written to CharacterMovement. */
	void UpdateMaxWalkSpeed();

	/** Moves the camera FOV one frame towards its target. */
	void UpdateAimFOV(float DeltaSeconds);

	/** Swaps the body between IdleAnim and WalkAnim. There is no AnimBP; see LocomotionAnim.h. */
	void UpdateBodyLocomotion();

	// --- Components -----------------------------------------------------------------------------

	/**
	 * Placeholder boom: 350 behind the capsule, socket lifted 60, turned by the control rotation,
	 * camera lag 10. TODO(stage2): replaced by the tuned camera rig in the next task.
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Castle|Movement", meta = (ClampMin = "0.0"))
	float WalkSpeed = 450.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Castle|Movement", meta = (ClampMin = "0.0"))
	float SprintSpeed = 750.f;

	UPROPERTY(BlueprintReadOnly, Category = "Castle|Movement")
	bool bIsSprinting = false;

	// --- Camera (placeholder) -------------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Castle|Camera", meta = (ClampMin = "0.0"))
	float CameraBoomLength = 350.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Castle|Camera")
	FVector CameraBoomSocketOffset = FVector(0.f, 0.f, 60.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Castle|Camera", meta = (ClampMin = "0.0"))
	float CameraLagSpeed = 10.f;

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

	/** Field of view when not aiming. The camera starts here and returns here. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Castle|Aim", meta = (ClampMin = "10.0", ClampMax = "170.0"))
	float HipFOV = 90.f;

	/** Field of view while aiming. */
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

	// --- Animation ------------------------------------------------------------------------------

	/** Body animation while standing still. The same sequence the thugs use. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Castle|Animation")
	TObjectPtr<UAnimSequence> IdleAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Castle|Animation")
	TObjectPtr<UAnimSequence> WalkAnim;

	/** WalkAnim above this much ground speed, IdleAnim below it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Castle|Animation", meta = (ClampMin = "0.0"))
	float WalkAnimSpeedThreshold = 20.f;

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

private:
	FTimerHandle NoiseTimerHandle;

	/** Whichever of IdleAnim / WalkAnim the body is playing, so Tick only re-plays on a change. */
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> CurrentLocomotionAnim;
};

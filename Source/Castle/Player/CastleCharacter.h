// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CastleCharacter.generated.h"

class UCameraComponent;
class UInputAction;
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

	/** Fired when the Interact action is pressed; implement in Blueprint to drive doors, levers, pickups. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Castle|Character")
	void OnInteractPressed();

protected:
	//~ Begin APawn interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
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
};

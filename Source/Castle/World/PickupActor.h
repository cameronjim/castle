// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/Interactable.h"
#include "PickupActor.generated.h"

class UStaticMeshComponent;

/** What picking this up actually does. */
UENUM(BlueprintType)
enum class EPickupType : uint8
{
	/** Arms the player's UWeaponComponent with MagazineAmount / AmmoAmount rounds. */
	Weapon,
	/** Adds KeycardId to the player's keycard ring. */
	Keycard,
	/** Adds AmmoAmount rounds to the player's reserve. */
	Ammo
};

/**
 * A thing on the floor the player walks up to and presses Interact on. One class covers the
 * pistol, the keycard and spare ammo; which one it is, is data.
 *
 * Guards spawn these from AGuardCharacter::DropOnDeath.
 */
UCLASS(Blueprintable, BlueprintType)
class CASTLE_API APickupActor : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	APickupActor();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup")
	EPickupType PickupType = EPickupType::Weapon;

	/** Keycard id granted by a Keycard pickup. Doors match on this. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup")
	FName KeycardId = FName(TEXT("cellblock"));

	/** Reserve rounds for an Ammo pickup; spare rounds for a Weapon pickup. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup", meta = (ClampMin = "0"))
	int32 AmmoAmount = 24;

	/** Rounds already in the magazine when a Weapon pickup is taken. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup", meta = (ClampMin = "0"))
	int32 MagazineAmount = 12;

	/** Optional mission objective completed the moment this is picked up. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup")
	FName CompletesObjectiveId;

	/** Prompt override; when empty a sensible default per PickupType is used. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup")
	FText PromptOverride;

	/** Root and collision proxy. The silhouette is built from Part1..Part4 instead. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/**
	 * Four boxes the pickup's shape is assembled from: slide, frame, grip and trigger guard for
	 * the pistol, body and stripe for the keycard. Fixed components rather than script-created
	 * subobjects because a Blueprint CDO's component templates have to exist in C++ for the
	 * content script to be able to write to them reliably.
	 *
	 * Mesh, scale, offset and material are all set in Tools/Editor/create_world_blueprints.py.
	 * A part with no static mesh simply does not render, so a two-part pickup leaves two empty.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup|Parts")
	TObjectPtr<UStaticMeshComponent> Part1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup|Parts")
	TObjectPtr<UStaticMeshComponent> Part2;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup|Parts")
	TObjectPtr<UStaticMeshComponent> Part3;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup|Parts")
	TObjectPtr<UStaticMeshComponent> Part4;

	/** All four parts in order, for a script or a test that wants to walk them. */
	UFUNCTION(BlueprintPure, Category = "Pickup|Parts")
	TArray<UStaticMeshComponent*> GetParts() const;

	// --- Idle motion ----------------------------------------------------------------------------

	/** Slow yaw spin so a pickup on a grey floor reads as a pickup. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup|Motion")
	float SpinDegreesPerSecond = 45.f;

	/** Centimetres the pickup rises and falls around its hover height. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup|Motion", meta = (ClampMin = "0.0"))
	float BobAmplitude = 3.f;

	/** Bobs per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup|Motion", meta = (ClampMin = "0.0"))
	float BobHz = 0.7f;

	/** Height the pickup floats at above wherever it was placed or dropped. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup|Motion", meta = (ClampMin = "0.0"))
	float HoverHeight = 40.f;

	/** Applies the pickup to Interactor and destroys this actor. Returns true when it applied. */
	UFUNCTION(BlueprintCallable, Category = "Pickup")
	bool ApplyTo(AActor* Interactor);

	/** VFX/SFX hook. Fired just before the actor is destroyed. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Pickup")
	void OnPickedUp(AActor* Interactor);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Where the pickup was placed or dropped; the hover and bob are measured from here. */
	UPROPERTY(Transient)
	FVector RestLocation = FVector::ZeroVector;

	/** Seeded per actor so two pickups dropped together do not bob in lockstep. */
	UPROPERTY(Transient)
	float BobPhase = 0.f;

	float BobElapsed = 0.f;

public:
	//~ Begin IInteractable interface
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractPrompt_Implementation() const override;
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	//~ End IInteractable interface
};

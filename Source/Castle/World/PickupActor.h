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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Applies the pickup to Interactor and destroys this actor. Returns true when it applied. */
	UFUNCTION(BlueprintCallable, Category = "Pickup")
	bool ApplyTo(AActor* Interactor);

	/** VFX/SFX hook. Fired just before the actor is destroyed. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Pickup")
	void OnPickedUp(AActor* Interactor);

	//~ Begin IInteractable interface
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractPrompt_Implementation() const override;
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	//~ End IInteractable interface
};

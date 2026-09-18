// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/PickupActor.h"

#include "Castle.h"
#include "Combat/WeaponComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Mission/MissionSubsystem.h"
#include "Player/CastleCharacter.h"

APickupActor::APickupActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	// Blocks the interaction sweep (ECC_Visibility) but never the player walking into it.
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Mesh->SetMobility(EComponentMobility::Movable);
}

bool APickupActor::CanInteract_Implementation(AActor* Interactor) const
{
	return Cast<ACastleCharacter>(Interactor) != nullptr;
}

FText APickupActor::GetInteractPrompt_Implementation() const
{
	if (!PromptOverride.IsEmpty())
	{
		return PromptOverride;
	}

	switch (PickupType)
	{
	case EPickupType::Weapon:
		return NSLOCTEXT("Castle", "PickupWeapon", "[E] Take the pistol");
	case EPickupType::Keycard:
		return NSLOCTEXT("Castle", "PickupKeycard", "[E] Take the keycard");
	case EPickupType::Ammo:
	default:
		return NSLOCTEXT("Castle", "PickupAmmo", "[E] Take the ammo");
	}
}

void APickupActor::Interact_Implementation(AActor* Interactor)
{
	ApplyTo(Interactor);
}

bool APickupActor::ApplyTo(AActor* Interactor)
{
	ACastleCharacter* Character = Cast<ACastleCharacter>(Interactor);
	if (!Character)
	{
		return false;
	}

	switch (PickupType)
	{
	case EPickupType::Weapon:
	{
		UWeaponComponent* Weapon = Character->GetWeaponComponent();
		if (!Weapon)
		{
			UE_LOG(LogCastle, Warning, TEXT("%s: %s has no UWeaponComponent to arm."),
				*GetName(), *Character->GetName());
			return false;
		}
		Weapon->GiveWeapon(MagazineAmount, AmmoAmount);
		break;
	}
	case EPickupType::Keycard:
		Character->GiveKeycard(KeycardId);
		break;
	case EPickupType::Ammo:
	{
		UWeaponComponent* Weapon = Character->GetWeaponComponent();
		if (!Weapon)
		{
			return false;
		}
		Weapon->AddAmmo(AmmoAmount);
		break;
	}
	}

	if (!CompletesObjectiveId.IsNone())
	{
		if (UMissionSubsystem* Missions = UMissionSubsystem::Get(this))
		{
			Missions->CompleteObjective(CompletesObjectiveId);
		}
	}

	UE_LOG(LogCastle, Log, TEXT("%s picked up %s."), *Character->GetName(), *GetName());

	OnPickedUp(Interactor);
	Destroy();
	return true;
}

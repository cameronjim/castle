// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/PickupActor.h"

#include "Castle.h"
#include "Combat/WeaponComponent.h"
#include "Combat/WeaponDefinition.h"
#include "Components/StaticMeshComponent.h"
#include "Mission/MissionSubsystem.h"
#include "Player/CastleCharacter.h"
#include "Player/InventoryComponent.h"

APickupActor::APickupActor()
{
	// Ticks for the hover, bob and spin that make a pickup look like one.
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	// Blocks the interaction sweep (ECC_Visibility) but never the player walking into it.
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Mesh->SetMobility(EComponentMobility::Movable);

	// The parts carry the silhouette and, with the root mesh cleared, the interaction sweep.
	const TCHAR* PartNames[] = { TEXT("Part1"), TEXT("Part2"), TEXT("Part3"), TEXT("Part4") };
	TObjectPtr<UStaticMeshComponent>* PartSlots[] = { &Part1, &Part2, &Part3, &Part4 };
	for (int32 Index = 0; Index < 4; ++Index)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(PartNames[Index]);
		Part->SetupAttachment(Mesh);
		Part->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Part->SetCollisionResponseToAllChannels(ECR_Ignore);
		Part->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		Part->SetMobility(EComponentMobility::Movable);
		Part->SetCastShadow(false);
		*PartSlots[Index] = Part;
	}
}

TArray<UStaticMeshComponent*> APickupActor::GetParts() const
{
	return { Part1, Part2, Part3, Part4 };
}

void APickupActor::BeginPlay()
{
	Super::BeginPlay();

	RestLocation = GetActorLocation();

	// A stable per-actor seed: two pickups dropped by the same guard must not bob together.
	BobPhase = FMath::Fmod(static_cast<float>(GetUniqueID()) * 0.37f, 2.f * PI);
}

void APickupActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	BobElapsed += DeltaSeconds;

	const float Bob = FMath::Sin(BobElapsed * BobHz * 2.f * PI + BobPhase) * BobAmplitude;
	SetActorLocation(RestLocation + FVector(0.f, 0.f, HoverHeight + Bob));
	AddActorWorldRotation(FRotator(0.f, SpinDegreesPerSecond * DeltaSeconds, 0.f));
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

	UInventoryComponent* Inventory = Character->GetInventoryComponent();
	UWeaponDefinition* Definition = Weapon.IsNull() ? nullptr : Weapon.LoadSynchronous();

	switch (PickupType)
	{
	case EPickupType::Weapon:
	{
		if (Inventory && Definition)
		{
			Inventory->AddWeapon(Definition);
			// MagazineAmount and AmmoAmount are what this particular pickup carries, which can
			// differ from the definition's defaults (a half-empty gun off a dead guard).
			Inventory->SetSlotAmmo(Definition->Slot,
				FMath::Clamp(MagazineAmount, 0, Definition->MagazineSize), FMath::Max(AmmoAmount, 0));
			if (Inventory->GetActiveSlot() == Definition->Slot)
			{
				if (UWeaponComponent* Held = Character->GetWeaponComponent())
				{
					Held->SetActiveWeapon(Definition, MagazineAmount, AmmoAmount);
				}
			}
			break;
		}

		// No definition set on the pickup (or a pawn with no inventory): fall back to arming
		// the weapon component directly, which is what this did before the hotbar existed.
		UWeaponComponent* Held = Character->GetWeaponComponent();
		if (!Held)
		{
			UE_LOG(LogCastle, Warning, TEXT("%s: %s has no UWeaponComponent to arm."),
				*GetName(), *Character->GetName());
			return false;
		}
		Held->GiveWeapon(MagazineAmount, AmmoAmount);
		break;
	}
	case EPickupType::Keycard:
		Character->GiveKeycard(KeycardId);
		break;
	case EPickupType::Ammo:
	{
		if (Inventory)
		{
			Inventory->AddAmmo(Definition, AmmoAmount);
			break;
		}

		UWeaponComponent* Held = Character->GetWeaponComponent();
		if (!Held)
		{
			return false;
		}
		Held->AddAmmo(AmmoAmount);
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

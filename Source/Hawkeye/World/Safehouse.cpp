// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/Safehouse.h"

#include "Hawkeye.h"
#include "HawkeyePlayerController.h"
#include "Combat/HealthComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Player/HawkeyeCharacter.h"
#include "Player/InventoryComponent.h"
#include "Save/HawkeyeSaveSubsystem.h"
#include "UObject/ConstructorHelpers.h"

namespace HawkeyeSafehouse
{
	// The engine's 1 m cube, scaled per part; sizes in cm.
	static const FVector FrameSize(24.f, 170.f, 270.f);
	static const FVector DoorSize(8.f, 110.f, 230.f);
	static const FVector BoardSize(6.f, 240.f, 50.f);
	static constexpr float BoardBottom = 285.f;
	static const FLinearColor Purple(0.62f, 0.25f, 1.f);

	static void PlaceCube(UStaticMeshComponent* Part, UStaticMesh* Cube, const FVector& Size, float FrontX, float BottomZ)
	{
		Part->SetStaticMesh(Cube);
		Part->SetRelativeScale3D(Size / 100.f);
		Part->SetRelativeLocation(FVector(FrontX - Size.X * 0.5f, 0.f, BottomZ + Size.Z * 0.5f));
	}
}

ASafehouse::ASafehouse()
{
	PrimaryActorTick.bCanEverTick = false;
	using namespace HawkeyeSafehouse;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Cube = CubeFinder.Object;

	DoorFrame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorFrame"));
	DoorFrame->SetupAttachment(Root);
	PlaceCube(DoorFrame, Cube, FrameSize, 12.f, 0.f);

	Door = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Door"));
	Door->SetupAttachment(Root);
	PlaceCube(Door, Cube, DoorSize, 16.f, 2.f);

	SignBoard = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SignBoard"));
	SignBoard->SetupAttachment(Root);
	PlaceCube(SignBoard, Cube, BoardSize, 18.f, BoardBottom);
	SignBoard->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	SignText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("SignText"));
	SignText->SetupAttachment(Root);
	SignText->SetText(NSLOCTEXT("Hawkeye", "SafehouseSign", "SAFEHOUSE"));
	SignText->SetHorizontalAlignment(EHTA_Center);
	SignText->SetVerticalAlignment(EVRTA_TextCenter);
	SignText->SetWorldSize(34.f);
	SignText->SetTextRenderColor(FColor(235, 215, 255));
	SignText->SetRelativeLocation(FVector(19.f, 0.f, BoardBottom + BoardSize.Z * 0.5f));
	SignText->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	DoorLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("DoorLight"));
	DoorLight->SetupAttachment(Root);
	DoorLight->SetRelativeLocation(FVector(70.f, 0.f, 250.f));
	DoorLight->SetIntensityUnits(ELightUnits::Lumens);
	DoorLight->SetIntensity(1200.f);
	DoorLight->SetAttenuationRadius(700.f);
	DoorLight->SetLightColor(Purple);
	DoorLight->SetCastShadows(false);

	EntryZone = CreateDefaultSubobject<UBoxComponent>(TEXT("EntryZone"));
	EntryZone->SetupAttachment(Root);
	EntryZone->SetBoxExtent(FVector(110.f, 110.f, 110.f));
	EntryZone->SetRelativeLocation(FVector(110.f, 0.f, 110.f));
	EntryZone->SetCollisionProfileName(UCollisionProfile::CustomCollisionProfileName);
	EntryZone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	EntryZone->SetCollisionResponseToAllChannels(ECR_Ignore);
	EntryZone->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	EntryZone->SetGenerateOverlapEvents(true);
}

FText ASafehouse::GetDisplayName() const
{
	return DisplayName.IsEmpty() ? FText::FromName(SafehouseId) : DisplayName;
}

bool ASafehouse::CanInteract_Implementation(AActor* Interactor) const
{
	const APawn* Pawn = Cast<APawn>(Interactor);
	return Pawn && Pawn->IsPlayerControlled();
}

FText ASafehouse::GetInteractPrompt_Implementation() const
{
	return NSLOCTEXT("Hawkeye", "SafehousePrompt", "Enter safehouse");
}

int32 ASafehouse::RefillArrows(UInventoryComponent* Inventory)
{
	return Inventory ? Inventory->RefillToCaps() : 0;
}

void ASafehouse::Interact_Implementation(AActor* Interactor)
{
	AHawkeyeCharacter* Character = Cast<AHawkeyeCharacter>(Interactor);
	if (!Character)
	{
		return;
	}
	if (UHealthComponent* Health = Character->GetHealthComponent(); Health && Health->IsAlive())
	{
		Health->Heal(Health->GetMaxHealth() - Health->GetCurrentHealth(), this);
	}

	if (UHawkeyeSaveSubsystem* Save = UHawkeyeSaveSubsystem::Get(this))
	{
		const bool bFirstVisit = Save->DiscoverSafehouse(SafehouseId);
		Save->SaveCampaign(bFirstVisit ? TEXT("safehouse discovered") : TEXT("safehouse"));
	}
	UE_LOG(LogHawkeye, Log, TEXT("%s: %s entered the safehouse; healed to full."), *GetName(), *Character->GetName());

	if (AHawkeyePlayerController* PC = Cast<AHawkeyePlayerController>(Character->GetController()))
	{
		PC->OpenSafehouseMenu(this);
	}
}

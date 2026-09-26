// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/ArrowProjectile.h"

#include "Hawkeye.h"
#include "CollisionQueryParams.h"
#include "Combat/ArrowDefinition.h"
#include "Combat/BowComponent.h"
#include "Combat/BowDefinition.h"
#include "Combat/HealthComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Player/InventoryComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace HawkeyeArrow
{
	/** How far back along the flight path the bone probe starts, and how far past the impact it reaches. */
	static constexpr float BoneProbeBack = 100.f;
	static constexpr float BoneProbeAhead = 150.f;
}

const TCHAR* const AArrowProjectile::DefaultNockMaterialPath = TEXT("/Game/Blueprints/Weapons/M_ArrowNock.M_ArrowNock");

AArrowProjectile::AArrowProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	NockMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(DefaultNockMaterialPath));

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	RootComponent = Collision;
	Collision->InitSphereRadius(2.f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	// World and bodies stop it. Not the Weapon trace channel: the hidden traversal ledges on every
	// roof edge block only that, and an arrow that stuck in thin air 2 cm off a facade is a bug.
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Destructible, ECR_Block);
	Collision->SetGenerateOverlapEvents(false);
	Collision->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	UMaterialInterface* ShapeMaterialObject = ShapeMaterial.Succeeded() ? ShapeMaterial.Object : nullptr;
	auto MakePart = [this, ShapeMaterialObject](const TCHAR* Name, UStaticMesh* Mesh)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(Collision);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCanEverAffectNavigation(false);
		Part->SetCastShadow(false);
		if (Mesh)
		{
			Part->SetStaticMesh(Mesh);
		}
		if (ShapeMaterialObject)
		{
			Part->SetMaterial(0, ShapeMaterialObject);
		}
		return Part;
	};
	UStaticMesh* CylinderMesh = Cylinder.Succeeded() ? Cylinder.Object : nullptr;
	UStaticMesh* CubeMesh = Cube.Succeeded() ? Cube.Object : nullptr;
	Shaft = MakePart(TEXT("Shaft"), CylinderMesh);
	Fletching = MakePart(TEXT("Fletching"), CubeMesh);
	Fletching2 = MakePart(TEXT("Fletching2"), CubeMesh);
	Fletching3 = MakePart(TEXT("Fletching3"), CubeMesh);
	Nock = MakePart(TEXT("Nock"), CylinderMesh);
	LayoutParts();

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->SetUpdatedComponent(Collision);
	Movement->InitialSpeed = 0.f;
	Movement->MaxSpeed = 0.f;
	Movement->ProjectileGravityScale = 1.f;
	Movement->bRotationFollowsVelocity = true;
	Movement->bShouldBounce = false;
	Movement->bAutoActivate = false;
}

void AArrowProjectile::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	// Again here, so a Blueprint child's ShaftLength or thickness shapes its parts too.
	LayoutParts();
}

void AArrowProjectile::LayoutParts()
{
	// The engine cylinder is 100 cm along Z about its centre; pitched 90 it lies along X, back from
	// the tip at the root.
	const float Radius = ShaftThickness * 0.5f;
	if (Shaft)
	{
		Shaft->SetRelativeLocationAndRotation(FVector(-ShaftLength * 0.5f, 0.f, 0.f), FRotator(90.f, 0.f, 0.f));
		Shaft->SetRelativeScale3D(FVector(ShaftThickness / 100.f, ShaftThickness / 100.f, ShaftLength / 100.f));
	}
	// Vanes 12 cm long and 4 cm tall, 3 mm thin, standing out from the shaft 120 degrees apart.
	const float VaneHeight = 4.f;
	const float VaneX = -ShaftLength + 9.f;
	UStaticMeshComponent* const Vanes[] = { Fletching.Get(), Fletching2.Get(), Fletching3.Get() };
	for (int32 Index = 0; Index < 3; ++Index)
	{
		UStaticMeshComponent* Vane = Vanes[Index];
		if (!Vane)
		{
			continue;
		}
		const FRotator Roll(0.f, 0.f, 120.f * Index);
		Vane->SetRelativeLocationAndRotation(FVector(VaneX, 0.f, 0.f) + Roll.RotateVector(FVector(0.f, 0.f, Radius + VaneHeight * 0.5f)), Roll);
		Vane->SetRelativeScale3D(FVector(0.12f, 0.003f, VaneHeight / 100.f));
	}
	if (Nock)
	{
		Nock->SetRelativeLocationAndRotation(FVector(-ShaftLength - 1.f, 0.f, 0.f), FRotator(90.f, 0.f, 0.f));
		Nock->SetRelativeScale3D(FVector((ShaftThickness + 0.8f) / 100.f, (ShaftThickness + 0.8f) / 100.f, 0.03f));
	}
}

void AArrowProjectile::InitArrow(UArrowDefinition* InArrow, UBowDefinition* InBow, float InDamage, AActor* InShooter,
	UBowComponent* InSource)
{
	Arrow = InArrow;
	Bow = InBow;
	Damage = InDamage;
	Shooter = InShooter;
	Source = InSource;
	SetOwner(InShooter);
	SetInstigator(Cast<APawn>(InShooter));

	if (InShooter && Collision)
	{
		// It leaves from her hand, inside her own capsule.
		Collision->IgnoreActorWhenMoving(InShooter, true);
	}

	// Pale shaft so it reads against a dark jacket, coloured fletching, Kate's purple nock (glowing
	// when M_ArrowNock is there). BasicShapeMaterial and M_ArrowNock both take a Color parameter.
	if (Nock && !NockMaterial.IsNull())
	{
		if (UMaterialInterface* Glow = NockMaterial.LoadSynchronous())
		{
			Nock->SetMaterial(0, Glow);
		}
	}
	const TPair<UStaticMeshComponent*, FLinearColor> Tints[] = {
		{ Shaft.Get(), ShaftColor },
		{ Fletching.Get(), FletchingColor },
		{ Fletching2.Get(), FletchingColor },
		{ Fletching3.Get(), FletchingColor },
		{ Nock.Get(), NockColor },
	};
	for (const TPair<UStaticMeshComponent*, FLinearColor>& Tint : Tints)
	{
		if (UMaterialInstanceDynamic* Material = Tint.Key ? Tint.Key->CreateDynamicMaterialInstance(0) : nullptr)
		{
			Material->SetVectorParameterValue(TEXT("Color"), Tint.Value);
		}
	}
}

void AArrowProjectile::LaunchWithVelocity(const FVector& Velocity)
{
	if (!Movement || !Collision)
	{
		return;
	}
	bInFlight = true;
	bStuck = false;
	SetActorRotation(Velocity.Rotation());
	Movement->SetUpdatedComponent(Collision);
	Movement->Velocity = Velocity;
	Movement->Activate(true);
	Movement->UpdateComponentVelocity();
	if (!Movement->OnProjectileStop.IsAlreadyBound(this, &AArrowProjectile::HandleImpact))
	{
		Movement->OnProjectileStop.AddDynamic(this, &AArrowProjectile::HandleImpact);
	}
}

void AArrowProjectile::AdvanceFlight(float DeltaSeconds)
{
	if (bInFlight && Movement && Movement->IsActive())
	{
		Movement->TickComponent(DeltaSeconds, LEVELTICK_All, nullptr);
	}
	FaceVelocity();
}

void AArrowProjectile::FaceVelocity()
{
	if (bInFlight && Movement && !Movement->Velocity.IsNearlyZero())
	{
		SetActorRotation(Movement->Velocity.Rotation());
	}
}

void AArrowProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bInFlight)
	{
		// The movement component already turns the root; this keeps the rule explicit and holds
		// for a subclass that moves itself.
		FaceVelocity();
		return;
	}

	if (bStuck && Arrow && Arrow->bRecoverable)
	{
		const UWorld* World = GetWorld();
		const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		TryRecoverBy(PC ? PC->GetPawn() : nullptr);
	}
}

FName AArrowProjectile::ResolveHitBone(const FHitResult& Hit, const FVector& Direction) const
{
	if (!Hit.BoneName.IsNone())
	{
		return Hit.BoneName;
	}

	const AActor* HitActor = Hit.GetActor();
	USkeletalMeshComponent* SkeletalMesh = HitActor ? HitActor->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
	if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshAsset() || !SkeletalMesh->GetPhysicsAsset())
	{
		return Hit.BoneName;
	}

	// The capsule is what the sweep met and it is wider than the head; ask the body's physics
	// shapes (what the Weapon channel meets on a mesh) which bone the flight line really crosses.
	const FVector Start = Hit.ImpactPoint - Direction * HawkeyeArrow::BoneProbeBack;
	const FVector End = Hit.ImpactPoint + Direction * HawkeyeArrow::BoneProbeAhead;
	FHitResult BoneHit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HawkeyeArrowBone), /*bTraceComplex=*/false);
	if (SkeletalMesh->LineTraceComponent(BoneHit, Start, End, Params))
	{
		return BoneHit.BoneName;
	}
	return Hit.BoneName;
}

void AArrowProjectile::DamageVictim(const FHitResult& Hit, const FVector& Direction, FName Bone)
{
	AActor* Victim = Hit.GetActor();
	UHealthComponent* Health = Victim ? Victim->FindComponentByClass<UHealthComponent>() : nullptr;
	if (!Health || !Health->IsAlive())
	{
		return;
	}

	const bool bHeadshot = Bow && Bow->IsHeadBone(Bone);
	const float Dealt = Bow ? Bow->ApplyHeadshot(Damage, Bone) : Damage;

	AActor* ShooterActor = Shooter.Get();
	const APawn* ShooterPawn = Cast<APawn>(ShooterActor);
	UGameplayStatics::ApplyPointDamage(Victim, Dealt, Direction, Hit,
		ShooterPawn ? ShooterPawn->GetController() : nullptr, ShooterActor ? ShooterActor : this, nullptr);

	// A surviving victim flinches through the same path a punch uses.
	if (Health->IsAlive())
	{
		Health->Stagger(ShooterActor);
	}

	UE_LOG(LogHawkeye, Log, TEXT("%s: %s hit %s (bone %s) for %.1f%s, health now %.1f."), *GetNameSafe(ShooterActor),
		*GetNameSafe(Arrow), *GetNameSafe(Victim), *Bone.ToString(), Dealt, bHeadshot ? TEXT(" (headshot)") : TEXT(""),
		Health->GetCurrentHealth());

	if (UBowComponent* SourceBow = Source.Get())
	{
		SourceBow->NotifyArrowHit(Victim, Dealt, bHeadshot);
	}
}

void AArrowProjectile::Embed(const FHitResult& Hit, const FVector& Direction, FName Bone)
{
	if (Movement)
	{
		Movement->StopMovementImmediately();
		Movement->Deactivate();
	}
	if (Collision)
	{
		Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	const FVector Tip = (Hit.bBlockingHit ? FVector(Hit.ImpactPoint) : GetActorLocation()) + Direction * EmbedDepth;
	SetActorLocationAndRotation(Tip, Direction.Rotation());

	// Onto the bone when the flight line found one, so an arrow in a thug goes down with the
	// ragdoll instead of staying on the capsule he leaves standing.
	const AActor* HitActor = Hit.GetActor();
	USkeletalMeshComponent* Body = HitActor ? HitActor->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
	if (Body && !Bone.IsNone() && Body->GetBoneIndex(Bone) != INDEX_NONE)
	{
		AttachToComponent(Body, FAttachmentTransformRules::KeepWorldTransform, Bone);
	}
	else if (USceneComponent* HitComponent = Hit.GetComponent())
	{
		AttachToComponent(HitComponent, FAttachmentTransformRules::KeepWorldTransform);
	}
	StuckIn = Hit.GetActor();
	SetLifeSpan(StuckLifeSeconds);
}

void AArrowProjectile::HandleImpact(const FHitResult& Hit)
{
	if (bStuck)
	{
		return;
	}

	const FVector Direction = (Hit.TraceEnd - Hit.TraceStart).GetSafeNormal().IsNearlyZero()
		? GetActorForwardVector()
		: (Hit.TraceEnd - Hit.TraceStart).GetSafeNormal();

	bInFlight = false;
	bStuck = true;

	const FName Bone = ResolveHitBone(Hit, Direction);
	DamageVictim(Hit, Direction, Bone);
	Embed(Hit, Direction, Bone);
}

bool AArrowProjectile::TryRecoverBy(AActor* Collector)
{
	if (!bStuck || !Arrow || !Arrow->bRecoverable || !IsValid(Collector))
	{
		return false;
	}
	if (FVector::Dist(Collector->GetActorLocation(), GetActorLocation()) > RecoverRadius)
	{
		return false;
	}
	UInventoryComponent* Inventory = Collector->FindComponentByClass<UInventoryComponent>();
	if (!Inventory || Inventory->AddArrows(Arrow, 1) <= 0)
	{
		// A full quiver leaves the arrow where it is.
		return false;
	}

	UE_LOG(LogHawkeye, Log, TEXT("%s recovered %s; %d in slot %d."), *GetNameSafe(Collector), *GetNameSafe(Arrow),
		Inventory->GetArrowCount(Arrow->Slot), Arrow->Slot);
	bStuck = false;
	Destroy();
	return true;
}

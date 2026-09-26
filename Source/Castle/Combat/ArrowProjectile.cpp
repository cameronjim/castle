// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/ArrowProjectile.h"

#include "Castle.h"
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

namespace CastleArrow
{
	/** How far back along the flight path the bone probe starts, and how far past the impact it reaches. */
	static constexpr float BoneProbeBack = 100.f;
	static constexpr float BoneProbeAhead = 150.f;
}

AArrowProjectile::AArrowProjectile()
{
	PrimaryActorTick.bCanEverTick = true;

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

	// The engine cylinder is 100 cm along Z about its centre; pitched 90 it lies along X.
	Shaft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Shaft"));
	Shaft->SetupAttachment(Collision);
	Shaft->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Shaft->SetCastShadow(false);
	Shaft->SetRelativeLocationAndRotation(FVector(-ShaftLength * 0.5f, 0.f, 0.f), FRotator(90.f, 0.f, 0.f));
	Shaft->SetRelativeScale3D(FVector(0.016f, 0.016f, ShaftLength / 100.f));

	Fletching = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Fletching"));
	Fletching->SetupAttachment(Collision);
	Fletching->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Fletching->SetCastShadow(false);
	Fletching->SetRelativeLocation(FVector(-ShaftLength + 8.f, 0.f, 0.f));
	Fletching->SetRelativeScale3D(FVector(0.12f, 0.07f, 0.07f));

	if (Cylinder.Succeeded())
	{
		Shaft->SetStaticMesh(Cylinder.Object);
	}
	if (Cube.Succeeded())
	{
		Fletching->SetStaticMesh(Cube.Object);
	}
	if (ShapeMaterial.Succeeded())
	{
		Shaft->SetMaterial(0, ShapeMaterial.Object);
		Fletching->SetMaterial(0, ShapeMaterial.Object);
	}

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->SetUpdatedComponent(Collision);
	Movement->InitialSpeed = 0.f;
	Movement->MaxSpeed = 0.f;
	Movement->ProjectileGravityScale = 1.f;
	Movement->bRotationFollowsVelocity = true;
	Movement->bShouldBounce = false;
	Movement->bAutoActivate = false;
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

	// Pale shaft so it reads against a dark jacket, coloured fletching. BasicShapeMaterial takes a
	// Color parameter.
	if (UMaterialInstanceDynamic* ShaftMaterial = Shaft ? Shaft->CreateDynamicMaterialInstance(0) : nullptr)
	{
		ShaftMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.6f, 0.58f, 0.52f));
	}
	if (UMaterialInstanceDynamic* FletchMaterial = Fletching ? Fletching->CreateDynamicMaterialInstance(0) : nullptr)
	{
		FletchMaterial->SetVectorParameterValue(TEXT("Color"), FletchingColor);
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
	const FVector Start = Hit.ImpactPoint - Direction * CastleArrow::BoneProbeBack;
	const FVector End = Hit.ImpactPoint + Direction * CastleArrow::BoneProbeAhead;
	FHitResult BoneHit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CastleArrowBone), /*bTraceComplex=*/false);
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

	UE_LOG(LogCastle, Log, TEXT("%s: %s hit %s (bone %s) for %.1f%s, health now %.1f."), *GetNameSafe(ShooterActor),
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

	UE_LOG(LogCastle, Log, TEXT("%s recovered %s; %d in slot %d."), *GetNameSafe(Collector), *GetNameSafe(Arrow),
		Inventory->GetArrowCount(Arrow->Slot), Arrow->Slot);
	bStuck = false;
	Destroy();
	return true;
}

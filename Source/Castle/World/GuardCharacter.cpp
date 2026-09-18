// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/GuardCharacter.h"

#include "AIController.h"
#include "Animation/AnimSequence.h"
#include "Castle.h"
#include "Combat/HealthComponent.h"
#include "Combat/WeaponComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "World/PickupActor.h"

AGuardCharacter::AGuardCharacter()
{
	// Ticks to swap between the idle and walk sequences; the mannequin pack's AnimBP does not
	// compile headless, so the guards drove no animation at all and stood in a T-pose.
	PrimaryActorTick.bCanEverTick = true;

	// UTakedownComponent finds candidates by tag, so it has to be set before BeginPlay.
	Tags.Add(FName(TEXT("Guard")));

	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
	HealthComponent->MaxHealth = 100.f;
	HealthComponent->CurrentHealth = 100.f;

	WeaponComponent = CreateDefaultSubobject<UWeaponComponent>(TEXT("WeaponComponent"));
	WeaponComponent->bHasWeapon = true;
	// Guards are worse shots than Frank: 12 a hit, so the player survives a few.
	WeaponComponent->Damage = 12.f;
	WeaponComponent->MagazineSize = 12;
	WeaponComponent->CurrentAmmo = 12;
	WeaponComponent->ReserveAmmo = 120;
	WeaponComponent->FireRate = 180.f;
	WeaponComponent->Range = 4000.f;

	GetCapsuleComponent()->SetCapsuleSize(34.f, 96.f);

	// Both the capsule and the mesh block bullets. The capsule is the guarantee - a greybox
	// guard with no skeletal mesh still has to be killable - and the mesh is what gives the
	// hit a bone name, which is where the headshot multiplier comes from.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_CastleWeapon, ECR_Block);
	if (USkeletalMeshComponent* SkeletalMesh = GetMesh())
	{
		SkeletalMesh->SetCollisionResponseToChannel(ECC_CastleWeapon, ECR_Block);
	}

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = 300.f;
		Movement->bUseControllerDesiredRotation = true;
		Movement->bOrientRotationToMovement = false;
		Movement->RotationRate = FRotator(0.f, 360.f, 0.f);
	}

	Flashlight = CreateDefaultSubobject<USpotLightComponent>(TEXT("Flashlight"));
	Flashlight->SetupAttachment(GetMesh() ? static_cast<USceneComponent*>(GetMesh()) : GetCapsuleComponent());
	Flashlight->SetRelativeLocation(FVector(20.f, 0.f, 60.f));
	Flashlight->SetIntensity(3000.f);
	Flashlight->SetIntensityUnits(ELightUnits::Candelas);
	Flashlight->SetInnerConeAngle(25.f);
	Flashlight->SetOuterConeAngle(35.f);
	Flashlight->SetAttenuationRadius(2500.f);
	Flashlight->SetLightColor(FLinearColor(0.85f, 0.92f, 1.f));
	Flashlight->SetCastShadows(true);
	Flashlight->SetMobility(EComponentMobility::Movable);
	Flashlight->SetVisibility(true);

	bUseControllerRotationYaw = false;

	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void AGuardCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Bound here rather than in BeginPlay: a guard killed the same frame he spawns still has to
	// drop and go limp, and a world that never begins play (tests) still wires the death path.
	if (HealthComponent)
	{
		HealthComponent->OnDeath.AddDynamic(this, &AGuardCharacter::HandleDeath);
	}
}

void AGuardCharacter::BeginPlay()
{
	Super::BeginPlay();

	AttachFlashlight();
	UpdateLocomotionAnimation();
}

void AGuardCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateLocomotionAnimation();
}

void AGuardCharacter::AttachFlashlight()
{
	USkeletalMeshComponent* SkeletalMesh = GetMesh();
	if (!Flashlight || !SkeletalMesh)
	{
		return;
	}

	// The head socket points the cone where the guard is looking. Without one the light stays
	// on the mesh root, which still faces forward because the capsule does.
	if (SkeletalMesh->DoesSocketExist(FlashlightSocketName))
	{
		Flashlight->AttachToComponent(
			SkeletalMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, FlashlightSocketName);
		Flashlight->SetRelativeLocationAndRotation(FVector(10.f, 0.f, 0.f), FRotator(0.f, 90.f, -90.f));
	}
}

UAnimSequence* AGuardCharacter::SelectLocomotionAnim() const
{
	return GetVelocity().Size2D() > WalkAnimSpeedThreshold ? WalkAnim : IdleAnim;
}

void AGuardCharacter::UpdateLocomotionAnimation()
{
	if (bLimp)
	{
		return;
	}

	UAnimSequence* Wanted = SelectLocomotionAnim();
	if (!Wanted || Wanted == CurrentLocomotionAnim)
	{
		// Re-playing the same sequence every frame would hold it on its first pose forever.
		return;
	}

	CurrentLocomotionAnim = Wanted;

	// A greybox guard with no mesh still tracks which animation it would be playing, which is
	// what the test asserts on; there is simply nothing to play it through.
	USkeletalMeshComponent* SkeletalMesh = GetMesh();
	if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshAsset())
	{
		return;
	}

	SkeletalMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	SkeletalMesh->PlayAnimation(Wanted, /*bLooping=*/true);
}

void AGuardCharacter::SetAlertState(EGuardAlertState NewState)
{
	if (AlertState == NewState)
	{
		return;
	}

	const EGuardAlertState OldState = AlertState;
	AlertState = NewState;

	UE_LOG(LogCastle, Verbose, TEXT("%s: alert state %d -> %d."),
		*GetName(), static_cast<int32>(OldState), static_cast<int32>(NewState));

	OnAlertStateChanged.Broadcast(OldState, NewState);
}

bool AGuardCharacter::CanBeTakenDown_Implementation(AActor* /*Attacker*/)
{
	// The stealth reward: once he has confirmed you, you have to shoot him.
	return AlertState != EGuardAlertState::Alerted && HealthComponent && HealthComponent->IsAlive();
}

void AGuardCharacter::OnTakedown_Implementation(AActor* /*Attacker*/)
{
	GoLimp();
	DropLoot();

	if (HealthComponent)
	{
		// Routed through health so OnDeath listeners (and the drop) behave identically to a bullet.
		HealthComponent->ApplyDamage(9999.f, nullptr);
	}
}

void AGuardCharacter::HandleDeath(UHealthComponent* /*Health*/, AActor* Killer)
{
	// Log-level, not Verbose: "did anything I shot actually die" is the first question of
	// every playtest, and it has to be answerable from the default log.
	UE_LOG(LogCastle, Log, TEXT("%s died (killed by %s, alert state %d)."),
		*GetName(), *GetNameSafe(Killer), static_cast<int32>(AlertState));

	GoLimp();
	DropLoot();
}

void AGuardCharacter::GoLimp()
{
	if (bLimp)
	{
		return;
	}
	bLimp = true;

	// Stop the AI first: a behaviour tree still issuing move orders fights the ragdoll.
	if (AController* MyController = GetController())
	{
		MyController->StopMovement();
		MyController->UnPossess();
	}

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
		// The capsule stops driving the mesh; the bodies do.
		Movement->SetComponentTickEnabled(false);
	}

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// A body on the floor is not still sweeping the corridor with a torch.
	if (Flashlight)
	{
		Flashlight->SetVisibility(false);
	}

	// A greybox guard may have no skeletal mesh at all; ragdoll only when there is something to
	// sim, and only when the mesh has a physics asset to sim it with.
	USkeletalMeshComponent* SkeletalMesh = GetMesh();
	if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshAsset())
	{
		return;
	}

	if (!SkeletalMesh->GetPhysicsAsset())
	{
		// No bodies to simulate: fall back to the death sequence if the data provides one.
		if (DeathAnim)
		{
			CurrentLocomotionAnim = DeathAnim;
			SkeletalMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			SkeletalMesh->PlayAnimation(DeathAnim, /*bLooping=*/false);
			return;
		}

		UE_LOG(LogCastle, Warning,
			TEXT("%s: the mesh %s has no physics asset, so the body cannot ragdoll."),
			*GetName(), *GetNameSafe(SkeletalMesh->GetSkeletalMeshAsset()));
		return;
	}

	// The mesh is attached to the capsule in the character's default layout, and a simulating
	// body that is still welded to its parent will not fall.
	SkeletalMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);

	SkeletalMesh->SetCollisionProfileName(TEXT("Ragdoll"));
	SkeletalMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	SkeletalMesh->SetAllBodiesSimulatePhysics(true);
	SkeletalMesh->SetSimulatePhysics(true);
	SkeletalMesh->WakeAllRigidBodies();

	// The corpse stays where it lands; nothing should ever try to move the actor again.
	SetActorTickEnabled(false);
}

void AGuardCharacter::DropLoot()
{
	if (bLootDropped)
	{
		return;
	}
	bLootDropped = true;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	int32 Index = 0;
	for (const TSubclassOf<APickupActor>& PickupClass : DropOnDeath)
	{
		if (!PickupClass)
		{
			continue;
		}

		// Fan the drops around the body so two of them never land inside each other, and drop
		// them to floor level: the pickups hover back up to their own height from there.
		const float FanDegrees = 360.f / FMath::Max(DropOnDeath.Num(), 1) * Index;
		const FVector Fan = FVector(DropSpacing, 0.f, 0.f).RotateAngleAxis(FanDegrees, FVector::UpVector);
		const FVector Offset = GetActorRotation().RotateVector(Fan)
			+ FVector(0.f, 0.f, -GetCapsuleComponent()->GetScaledCapsuleHalfHeight());

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.Owner = this;

		if (World->SpawnActor<APickupActor>(PickupClass, GetActorLocation() + Offset, FRotator::ZeroRotator, SpawnParams))
		{
			++Index;
		}
	}

	UE_LOG(LogCastle, Log, TEXT("%s dropped %d pickup(s)."), *GetName(), Index);
}

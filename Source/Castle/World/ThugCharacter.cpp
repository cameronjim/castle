// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/ThugCharacter.h"

#include "AIController.h"
#include "Animation/AnimSequence.h"
#include "Castle.h"
#include "Combat/HealthComponent.h"
#include "Combat/MeleeComponent.h"
#include "Combat/WeaponComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Player/LocomotionAnim.h"
#include "World/PickupActor.h"
#include "World/ThugAIController.h"

AThugCharacter::AThugCharacter()
{
	// Ticks to swap between the idle and walk sequences; the mannequin pack's AnimBP does not
	// compile headless, so the thugs drove no animation at all and stood in a T-pose.
	PrimaryActorTick.bCanEverTick = true;

	// UTakedownComponent finds candidates by tag, so it has to be set before BeginPlay.
	Tags.Add(FName(TEXT("Thug")));

	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
	HealthComponent->MaxHealth = 100.f;
	HealthComponent->CurrentHealth = 100.f;

	WeaponComponent = CreateDefaultSubobject<UWeaponComponent>(TEXT("WeaponComponent"));
	WeaponComponent->bHasWeapon = true;
	// Thugs are poor shots: 12 a hit, so the player survives a few.
	WeaponComponent->Damage = 12.f;
	WeaponComponent->MagazineSize = 12;
	WeaponComponent->CurrentAmmo = 12;
	WeaponComponent->ReserveAmmo = 120;
	WeaponComponent->FireRate = 180.f;
	WeaponComponent->Range = 4000.f;

	// A thug's swing never lands on the thug beside him.
	MeleeComponent = CreateDefaultSubobject<UMeleeComponent>(TEXT("MeleeComponent"));
	MeleeComponent->IgnoreTag = FName(TEXT("Thug"));

	// The wind-up is the telegraph and the dodge window: long enough to read, not so long it
	// stops being a threat. Recovery is what leaves room to hit back.
	FistsAttack.Name = FName(TEXT("fists"));
	FistsAttack.Damage = 15.f;
	FistsAttack.WindupSeconds = 0.6f;
	FistsAttack.RecoverSeconds = 0.6f;
	FistsAttack.Range = 120.f;
	FistsAttack.Radius = 35.f;
	FistsAttack.bStagger = true;

	BatAttack = FistsAttack;
	BatAttack.Name = FName(TEXT("bat"));
	BatAttack.Damage = 25.f;
	BatAttack.Range = 140.f;

	GetCapsuleComponent()->SetCapsuleSize(34.f, 96.f);

	// Both the capsule and the mesh block bullets. The capsule is the guarantee - a greybox
	// thug with no skeletal mesh still has to be killable - and the mesh is what gives the
	// hit a bone name, which is where the headshot multiplier comes from.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_CastleWeapon, ECR_Block);
	if (USkeletalMeshComponent* SkeletalMesh = GetMesh())
	{
		SkeletalMesh->SetCollisionResponseToChannel(ECC_CastleWeapon, ECR_Block);

		// The mannequin is authored facing its own +Y, so a -90 degree yaw is what points it down
		// the actor's +X and makes the walk cycle agree with the direction of travel. This lived
		// only in create_world_blueprints.py, which meant a thug spawned from C++ faced ninety
		// degrees off and nothing in the test suite could see it.
		SkeletalMesh->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -96.f), FRotator(0.f, -90.f, 0.f));
	}

	// The bat rides in the right hand. No collision: the swing is a sweep, not the prop.
	HeldWeaponComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeldWeapon"));
	HeldWeaponComponent->SetupAttachment(GetMesh(), HeldWeaponSocketName);
	HeldWeaponComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeldWeaponComponent->SetCanEverAffectNavigation(false);
	HeldWeaponComponent->SetVisibility(false);

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = 300.f;
		// Face where you are actually going. With bUseControllerDesiredRotation the body chased
		// the control rotation instead, and the controller points that at the player the moment
		// he is seen - so an alerted thug closing the distance played a forward walk cycle while
		// travelling sideways or backwards. AThugAIController turns him to face a target only
		// when he has stopped to shoot or swing.
		Movement->bUseControllerDesiredRotation = false;
		Movement->bOrientRotationToMovement = true;
		Movement->RotationRate = FRotator(0.f, 360.f, 0.f);
	}

	bUseControllerRotationYaw = false;

	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void AThugCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Bound here rather than in BeginPlay: a thug killed the same frame he spawns still has to
	// drop and go limp, and a world that never begins play (tests) still wires the death path.
	if (HealthComponent)
	{
		HealthComponent->OnDeath.AddDynamic(this, &AThugCharacter::HandleDeath);
		HealthComponent->OnStaggered.AddDynamic(this, &AThugCharacter::HandleStaggered);
		HealthComponent->OnHealthChanged.AddDynamic(this, &AThugCharacter::HandleHealthChanged);
	}
}

void AThugCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (const USkeletalMeshComponent* SkeletalMesh = GetMesh())
	{
		MeshRelativeTransform = SkeletalMesh->GetRelativeTransform();
	}
	RefreshHeldWeapon();
	CreateBodyMaterials();
	UpdateLocomotionAnimation();
}

void AThugCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bCollapsing)
	{
		// A dead thug has no locomotion to update; he only has a floor to reach.
		UpdateProceduralCollapse(DeltaSeconds);
		return;
	}

	UpdateMaterialPulse(DeltaSeconds);
	if (bLimp)
	{
		return;
	}

	StaggerRemaining = FMath::Max(0.f, StaggerRemaining - DeltaSeconds);
	UpdateKnockdown(DeltaSeconds);
	if (!bKnockedDown)
	{
		UpdateLocomotionAnimation();
	}
}

void AThugCharacter::RefreshHeldWeapon()
{
	if (!HeldWeaponComponent)
	{
		return;
	}

	const bool bShowBat = Weapon == EThugWeapon::Bat && BatMesh != nullptr;
	if (bShowBat && HeldWeaponComponent->GetStaticMesh() != BatMesh)
	{
		HeldWeaponComponent->SetStaticMesh(BatMesh);
	}
	if (USkeletalMeshComponent* SkeletalMesh = GetMesh())
	{
		if (SkeletalMesh->DoesSocketExist(HeldWeaponSocketName)
			&& HeldWeaponComponent->GetAttachSocketName() != HeldWeaponSocketName)
		{
			HeldWeaponComponent->AttachToComponent(
				SkeletalMesh, FAttachmentTransformRules::KeepRelativeTransform, HeldWeaponSocketName);
		}
	}
	HeldWeaponComponent->SetVisibility(bShowBat);
}

void AThugCharacter::CreateBodyMaterials()
{
	USkeletalMeshComponent* SkeletalMesh = GetMesh();
	if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshAsset())
	{
		return;
	}

	BodyMaterials.Reset();
	for (int32 Slot = 0; Slot < SkeletalMesh->GetNumMaterials(); ++Slot)
	{
		if (UMaterialInstanceDynamic* Instance = SkeletalMesh->CreateAndSetMaterialInstanceDynamic(Slot))
		{
			BodyMaterials.Add(Instance);
		}
	}
}

void AThugCharacter::FlashHit()
{
	HitFlashRemaining = HitFlashSeconds;
}

void AThugCharacter::UpdateMaterialPulse(float DeltaSeconds)
{
	HitFlashRemaining = FMath::Max(0.f, HitFlashRemaining - DeltaSeconds);
	const float Flash = GetHitFlashAlpha();
	const float Telegraph = MeleeComponent && MeleeComponent->IsWindingUp() && !bLimp ? 1.f : 0.f;
	if (Flash == LastFlashWritten && Telegraph == LastTelegraphWritten)
	{
		return;
	}

	LastFlashWritten = Flash;
	LastTelegraphWritten = Telegraph;
	for (UMaterialInstanceDynamic* Instance : BodyMaterials)
	{
		if (Instance)
		{
			Instance->SetScalarParameterValue(HitFlashParameter, Flash);
			Instance->SetScalarParameterValue(TelegraphParameter, Telegraph);
		}
	}
}

void AThugCharacter::HandleHealthChanged(UHealthComponent* /*Health*/, float /*NewHealth*/, float Delta, AActor* /*DamageInstigator*/)
{
	if (Delta < 0.f)
	{
		FlashHit();
	}
}

void AThugCharacter::HandleStaggered(UHealthComponent* /*Health*/, AActor* DamageInstigator)
{
	HitReaction(DamageInstigator);
}

void AThugCharacter::AlertTo(AActor* By)
{
	// Being hit is how he finds out: he turns on whoever did it.
	if (AThugAIController* Brain = Cast<AThugAIController>(GetController()))
	{
		if (By)
		{
			Brain->ReportStimulus(EStimulusKind::Hearing, By->GetActorLocation(), true, Brain->GunshotLoudnessThreshold);
			if (Cast<APawn>(By))
			{
				Brain->SetTarget(By);
			}
		}
	}
	else if (AlertState != EThugAlertState::Alerted)
	{
		SetAlertState(EThugAlertState::Alerted);
	}
}

void AThugCharacter::HitReaction(AActor* HitBy)
{
	if (bLimp || !HealthComponent || !HealthComponent->IsAlive())
	{
		return;
	}

	StaggerRemaining = StaggerSeconds;

	// A punch in the wind-up interrupts the swing: hitting first is the counter.
	if (MeleeComponent)
	{
		MeleeComponent->CancelAttack();
	}

	// Stop whatever he was doing and shove him back a step, away from the hit. On the floor the
	// ragdoll is the reaction; a shove would only drag the empty capsule about.
	if (AController* MyController = GetController())
	{
		MyController->StopMovement();
	}
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (Movement && !bKnockedDown)
	{
		Movement->StopMovementImmediately();
		FVector Away = HitBy ? (GetActorLocation() - HitBy->GetActorLocation()).GetSafeNormal2D()
			: -GetActorForwardVector();
		if (Away.IsNearlyZero())
		{
			Away = -GetActorForwardVector();
		}
		LaunchCharacter(Away * HitShoveSpeed, true, false);
	}

	AlertTo(HitBy);

	UE_LOG(LogCastle, Log, TEXT("%s: hit reaction (by %s), staggered %.2f s, health %.1f."), *GetName(),
		*GetNameSafe(HitBy), StaggerSeconds, HealthComponent->GetCurrentHealth());
}

void AThugCharacter::Knockdown(AActor* By)
{
	if (bLimp || !HealthComponent || !HealthComponent->IsAlive())
	{
		return;
	}

	bKnockedDown = true;
	KnockdownRemaining = KnockdownSeconds;
	StaggerRemaining = 0.f;
	if (MeleeComponent)
	{
		MeleeComponent->CancelAttack();
	}
	if (AController* MyController = GetController())
	{
		MyController->StopMovement();
	}
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}
	// Kate walks over him while he is down; the capsule still takes hits and arrows.
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}

	bKnockdownRagdoll = BeginKnockdownRagdoll(By);
	AlertTo(By);

	UE_LOG(LogCastle, Log, TEXT("%s: knocked down by %s for %.1f s (%s), health %.1f."), *GetName(), *GetNameSafe(By),
		KnockdownSeconds, bKnockdownRagdoll ? TEXT("ragdoll") : TEXT("no ragdoll"), HealthComponent->GetCurrentHealth());
}

bool AThugCharacter::BeginKnockdownRagdoll(AActor* By)
{
	USkeletalMeshComponent* SkeletalMesh = GetMesh();
	if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshAsset() || !SkeletalMesh->GetPhysicsAsset())
	{
		return false;
	}

	MeshRelativeTransform = SkeletalMesh->GetRelativeTransform();
	SkeletalMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	SkeletalMesh->SetCollisionProfileName(TEXT("Ragdoll"));
	SkeletalMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	SkeletalMesh->SetAllBodiesSimulatePhysics(true);
	SkeletalMesh->SetSimulatePhysics(true);
	SkeletalMesh->WakeAllRigidBodies();
	if (!SkeletalMesh->IsSimulatingPhysics())
	{
		StandUp();
		return false;
	}

	FVector Away = By ? (GetActorLocation() - By->GetActorLocation()).GetSafeNormal2D() : -GetActorForwardVector();
	if (Away.IsNearlyZero())
	{
		Away = -GetActorForwardVector();
	}
	SkeletalMesh->SetAllPhysicsLinearVelocity(Away * KnockdownLaunchSpeed + FVector(0.f, 0.f, 150.f));
	return true;
}

void AThugCharacter::UpdateKnockdown(float DeltaSeconds)
{
	if (!bKnockedDown)
	{
		return;
	}
	KnockdownRemaining -= DeltaSeconds;
	if (KnockdownRemaining <= 0.f)
	{
		StandUp();
	}
}

void AThugCharacter::StandUp()
{
	USkeletalMeshComponent* SkeletalMesh = GetMesh();
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (SkeletalMesh && Capsule && SkeletalMesh->IsSimulatingPhysics())
	{
		// Stand where the body came to rest, on whatever floor is under the pelvis.
		const FVector Pelvis = SkeletalMesh->GetBoneLocation(FName(TEXT("pelvis")));
		FVector Stand = Pelvis + FVector(0.f, 0.f, Capsule->GetScaledCapsuleHalfHeight());
		FHitResult Floor;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ThugStandUp), false, this);
		if (GetWorld() && GetWorld()->LineTraceSingleByChannel(Floor, Pelvis + FVector(0.f, 0.f, 50.f),
				Pelvis - FVector(0.f, 0.f, 300.f), ECC_Visibility, Params))
		{
			Stand = Floor.ImpactPoint + FVector(0.f, 0.f, Capsule->GetScaledCapsuleHalfHeight() + 2.f);
		}

		SkeletalMesh->SetSimulatePhysics(false);
		SkeletalMesh->SetAllBodiesSimulatePhysics(false);
		SkeletalMesh->SetCollisionProfileName(TEXT("CharacterMesh"));
		SkeletalMesh->SetCollisionResponseToChannel(ECC_CastleWeapon, ECR_Block);
		SetActorLocation(Stand, false, nullptr, ETeleportType::TeleportPhysics);
		SkeletalMesh->AttachToComponent(Capsule, FAttachmentTransformRules::KeepRelativeTransform);
		SkeletalMesh->SetRelativeTransform(MeshRelativeTransform);
	}

	if (Capsule)
	{
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	}
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->SetMovementMode(MOVE_Walking);
	}

	const bool bWasDown = bKnockedDown;
	bKnockedDown = false;
	bKnockdownRagdoll = false;
	KnockdownRemaining = 0.f;

	// The ragdoll left the bones wherever they fell; re-playing the clip puts him back on his feet.
	CurrentLocomotionAnim = nullptr;
	UpdateLocomotionAnimation();
	if (bWasDown)
	{
		UE_LOG(LogCastle, Log, TEXT("%s: back on his feet."), *GetName());
	}
}

UAnimSequence* AThugCharacter::SelectLocomotionAnim() const
{
	const float Speed = GetVelocity().Size2D();
	if (Speed > RunAnimSpeedThreshold && RunAnim)
	{
		return RunAnim;
	}
	return Speed > WalkAnimSpeedThreshold ? WalkAnim : IdleAnim;
}

void AThugCharacter::UpdateLocomotionAnimation()
{
	if (bLimp)
	{
		return;
	}

	// A greybox thug with no mesh still tracks which animation it would be playing, which is
	// what the test asserts on; there is simply nothing to play it through.
	CastleLocomotion::PlayIfChanged(GetMesh(), SelectLocomotionAnim(), CurrentLocomotionAnim);
}

void AThugCharacter::SetAlertState(EThugAlertState NewState)
{
	if (AlertState == NewState)
	{
		return;
	}

	const EThugAlertState OldState = AlertState;
	AlertState = NewState;

	UE_LOG(LogCastle, Verbose, TEXT("%s: alert state %d -> %d."),
		*GetName(), static_cast<int32>(OldState), static_cast<int32>(NewState));

	OnAlertStateChanged.Broadcast(OldState, NewState);
}

bool AThugCharacter::CanBeTakenDown_Implementation(AActor* /*Attacker*/)
{
	// The stealth reward: once he has confirmed you, you have to fight him.
	return AlertState != EThugAlertState::Alerted && HealthComponent && HealthComponent->IsAlive();
}

void AThugCharacter::OnTakedown_Implementation(AActor* Attacker)
{
	GoLimp(Attacker);
	DropLoot();

	if (HealthComponent)
	{
		// Routed through health so OnDeath listeners (and the drop) behave identically to a
		// bullet. The attacker goes with it, so the death line names who did it instead of None.
		HealthComponent->ApplyDamage(9999.f, Attacker);
	}
}

void AThugCharacter::HandleDeath(UHealthComponent* /*Health*/, AActor* Killer)
{
	// Log-level, not Verbose: "did anything I shot actually die" is the first question of
	// every playtest, and it has to be answerable from the default log.
	UE_LOG(LogCastle, Log, TEXT("%s died (killed by %s, alert state %d)."),
		*GetName(), *GetNameSafe(Killer), static_cast<int32>(AlertState));

	GoLimp(Killer);
	DropLoot();
}

void AThugCharacter::GoLimp(AActor* Killer)
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

	// A swing in progress dies with him; a knockdown ends here.
	if (MeleeComponent)
	{
		MeleeComponent->CancelAttack();
	}
	bKnockedDown = false;

	// A greybox thug may have no skeletal mesh at all; ragdoll only when there is something to
	// sim, and only when the mesh has a physics asset to sim it with.
	USkeletalMeshComponent* SkeletalMesh = GetMesh();
	if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshAsset())
	{
		UE_LOG(LogCastle, Warning, TEXT("%s: died with no skeletal mesh; nothing to drop."), *GetName());
		SetActorTickEnabled(false);
		return;
	}

	// The mesh is attached to the capsule in the character's default layout, and a simulating
	// body that is still welded to its parent will not fall.
	SkeletalMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);

	if (SkeletalMesh->GetPhysicsAsset())
	{
		SkeletalMesh->SetCollisionProfileName(TEXT("Ragdoll"));
		SkeletalMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		SkeletalMesh->SetAllBodiesSimulatePhysics(true);
		SkeletalMesh->SetSimulatePhysics(true);
		SkeletalMesh->WakeAllRigidBodies();

		if (SkeletalMesh->IsSimulatingPhysics())
		{
			UE_LOG(LogCastle, Warning, TEXT("%s: went down by ragdoll (physics asset %s)."),
				*GetName(), *GetNameSafe(SkeletalMesh->GetPhysicsAsset()));

			// The corpse stays where it lands; nothing should ever try to move the actor again.
			SetActorTickEnabled(false);
			return;
		}

		// A physics asset whose bodies are named for a different skeleton leaves
		// InitArticulated with no root body, and SetSimulatePhysics silently does nothing.
		// That is the bug that had thugs freezing upright, so never trust it: check.
		UE_LOG(LogCastle, Warning,
			TEXT("%s: physics asset %s did not start simulating (no matching root body); "
				 "falling back to the procedural collapse."),
			*GetName(), *GetNameSafe(SkeletalMesh->GetPhysicsAsset()));
		SkeletalMesh->SetSimulatePhysics(false);
	}
	else
	{
		UE_LOG(LogCastle, Warning,
			TEXT("%s: the mesh %s has no physics asset; falling back to the procedural collapse."),
			*GetName(), *GetNameSafe(SkeletalMesh->GetSkeletalMeshAsset()));
	}

	BeginProceduralCollapse(Killer);
}

bool AThugCharacter::IsRagdolling() const
{
	const USkeletalMeshComponent* SkeletalMesh = GetMesh();
	return SkeletalMesh != nullptr && SkeletalMesh->IsSimulatingPhysics();
}

float AThugCharacter::GetCollapseAlpha() const
{
	if (!bLimp || CollapseSeconds <= 0.f)
	{
		return bCollapsing ? 0.f : (bLimp ? 1.f : 0.f);
	}
	return FMath::Clamp(CollapseElapsed / CollapseSeconds, 0.f, 1.f);
}

void AThugCharacter::BeginProceduralCollapse(AActor* Killer)
{
	USkeletalMeshComponent* SkeletalMesh = GetMesh();
	if (!SkeletalMesh)
	{
		return;
	}

	// A body that is still mid-walk-cycle while it tips over reads as a bug, so freeze the pose.
	SkeletalMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	if (DeathAnim)
	{
		CurrentLocomotionAnim = DeathAnim;
		SkeletalMesh->PlayAnimation(DeathAnim, /*bLooping=*/false);
	}
	else
	{
		CurrentLocomotionAnim = nullptr;
		SkeletalMesh->Stop();
	}

	// Nothing should collide with the corpse; it is a prop from here on.
	SkeletalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// He falls away from whoever put him down, or straight forward when that is not known.
	FVector Away = GetActorForwardVector().GetSafeNormal2D();
	if (Killer)
	{
		const FVector Delta = (GetActorLocation() - Killer->GetActorLocation()).GetSafeNormal2D();
		if (!Delta.IsNearlyZero())
		{
			Away = Delta;
		}
	}
	if (Away.IsNearlyZero())
	{
		Away = FVector::ForwardVector;
	}

	// Rotating about Up x Away carries the body's up vector towards Away: he pitches over
	// that way whatever direction the mesh itself happens to be facing.
	CollapseAxis = FVector::CrossProduct(FVector::UpVector, Away).GetSafeNormal();
	if (CollapseAxis.IsNearlyZero())
	{
		CollapseAxis = FVector::RightVector;
	}

	CollapseStartLocation = SkeletalMesh->GetComponentLocation();
	CollapseStartRotation = SkeletalMesh->GetComponentQuat();
	CollapseElapsed = 0.f;
	bCollapsing = true;

	// The collapse is driven from Tick, so the actor has to keep ticking through it.
	SetActorTickEnabled(true);
}

void AThugCharacter::UpdateProceduralCollapse(float DeltaSeconds)
{
	USkeletalMeshComponent* SkeletalMesh = GetMesh();
	if (!SkeletalMesh)
	{
		bCollapsing = false;
		return;
	}

	CollapseElapsed += DeltaSeconds;
	const float Alpha = CollapseSeconds > 0.f
		? FMath::Clamp(CollapseElapsed / CollapseSeconds, 0.f, 1.f) : 1.f;
	// Accelerating, not linear: a body does not tip over at a constant rate.
	const float Eased = Alpha * Alpha;

	const FQuat Tip(CollapseAxis, FMath::DegreesToRadians(CollapsePitchDegrees * Eased));
	SkeletalMesh->SetWorldLocationAndRotation(
		CollapseStartLocation - FVector(0.f, 0.f, CollapseDropDistance * Eased),
		Tip * CollapseStartRotation);

	if (Alpha >= 1.f)
	{
		bCollapsing = false;
		SetActorTickEnabled(false);
	}
}

void AThugCharacter::DropLoot()
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

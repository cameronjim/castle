// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/CastleCharacter.h"

#include "Camera/CameraComponent.h"
#include "Castle.h"
#include "CastleGameMode.h"
#include "Combat/HealthComponent.h"
#include "Combat/TakedownComponent.h"
#include "Combat/WeaponComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Components/PawnNoiseEmitterComponent.h"
#include "TimerManager.h"
#include "World/InteractionComponent.h"

ACastleCharacter::ACastleCharacter()
{
	// Ticks only to blend the aim FOV; everything else is event driven.
	PrimaryActorTick.bCanEverTick = true;

	// First-person: the controller drives the camera, not the mesh.
	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, EyeHeightOffset));
	FirstPersonCamera->bUsePawnControlRotation = true;
	FirstPersonCamera->SetFieldOfView(HipFOV);

	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
	TakedownComponent = CreateDefaultSubobject<UTakedownComponent>(TEXT("TakedownComponent"));
	InteractionComponent = CreateDefaultSubobject<UInteractionComponent>(TEXT("InteractionComponent"));
	NoiseEmitter = CreateDefaultSubobject<UPawnNoiseEmitterComponent>(TEXT("NoiseEmitter"));

	// Frank starts the mission empty-handed; the pistol pickup calls GiveWeapon.
	WeaponComponent = CreateDefaultSubobject<UWeaponComponent>(TEXT("WeaponComponent"));
	WeaponComponent->bHasWeapon = false;

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = WalkSpeed;
		Movement->NavAgentProps.bCanCrouch = true;
		Movement->bOrientRotationToMovement = false;
		Movement->JumpZVelocity = 480.f;
		Movement->AirControl = 0.35f;
	}

	// Lets the player crouch under and through geometry without the capsule popping.
	GetCapsuleComponent()->SetCapsuleSize(34.f, 88.f);

	// The Pawn profile ignores Visibility, so bullets need their own channel to land on
	// Frank at all - without this the guards' shots went through him as well.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_CastleWeapon, ECR_Block);
	if (USkeletalMeshComponent* SkeletalMesh = GetMesh())
	{
		SkeletalMesh->SetCollisionResponseToChannel(ECC_CastleWeapon, ECR_Block);
	}
}

void ACastleCharacter::BeginPlay()
{
	Super::BeginPlay();

	UpdateMaxWalkSpeed();

	if (FirstPersonCamera)
	{
		FirstPersonCamera->SetFieldOfView(HipFOV);
	}

	if (HealthComponent)
	{
		HealthComponent->OnDeath.AddDynamic(this, &ACastleCharacter::HandleDeath);
	}

	// Guards hear the player through AISense_Hearing; MakeNoise on a fixed beat is enough
	// resolution for a stealth game and costs nothing per frame.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			NoiseTimerHandle, this, &ACastleCharacter::EmitMovementNoise, NoiseIntervalSeconds, true);
	}
}

void ACastleCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(NoiseTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

float ACastleCharacter::GetMovementNoiseLoudness() const
{
	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!Movement || !Movement->IsMovingOnGround())
	{
		return 0.f;
	}

	if (bIsCrouched)
	{
		return 0.f;
	}

	if (GetVelocity().SizeSquared2D() < FMath::Square(10.f))
	{
		return 0.f;
	}

	return bIsSprinting ? SprintNoiseLoudness : WalkNoiseLoudness;
}

void ACastleCharacter::EmitMovementNoise()
{
	const float Loudness = GetMovementNoiseLoudness();
	if (Loudness <= 0.f)
	{
		return;
	}

	MakeNoise(Loudness, this, GetActorLocation());
}

void ACastleCharacter::HandleDeath(UHealthComponent* /*Health*/, AActor* Killer)
{
	UE_LOG(LogCastle, Log, TEXT("%s died (killer: %s); restarting the mission."),
		*GetName(), *GetNameSafe(Killer));

	if (ACastleGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ACastleGameMode>() : nullptr)
	{
		GameMode->RestartMission();
	}
}

bool ACastleCharacter::HasKeycard(FName KeycardId) const
{
	return !KeycardId.IsNone() && Keycards.Contains(KeycardId);
}

bool ACastleCharacter::GiveKeycard(FName KeycardId)
{
	if (KeycardId.IsNone() || Keycards.Contains(KeycardId))
	{
		return false;
	}

	Keycards.Add(KeycardId);
	UE_LOG(LogCastle, Log, TEXT("%s picked up keycard '%s'."), *GetName(), *KeycardId.ToString());
	return true;
}

void ACastleCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	AddDefaultMappingContext();
}

void ACastleCharacter::AddDefaultMappingContext()
{
	if (!DefaultMappingContext)
	{
		return;
	}

	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
	{
		Subsystem->AddMappingContext(DefaultMappingContext, DefaultMappingPriority);
	}
}

UWeaponComponent* ACastleCharacter::GetWeaponComponent() const
{
	return WeaponComponent ? WeaponComponent.Get() : FindComponentByClass<UWeaponComponent>();
}

void ACastleCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInput)
	{
		UE_LOG(LogCastle, Error,
			TEXT("Expected an EnhancedInputComponent; check DefaultEngine.ini input component class."));
		return;
	}

	if (MoveAction)
	{
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ACastleCharacter::Input_Move);
	}
	if (LookAction)
	{
		EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &ACastleCharacter::Input_Look);
	}
	if (JumpAction)
	{
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	}
	if (SprintAction)
	{
		EnhancedInput->BindAction(SprintAction, ETriggerEvent::Started, this, &ACastleCharacter::Input_SprintStarted);
		EnhancedInput->BindAction(SprintAction, ETriggerEvent::Completed, this, &ACastleCharacter::Input_SprintCompleted);
	}
	if (CrouchAction)
	{
		EnhancedInput->BindAction(CrouchAction, ETriggerEvent::Started, this, &ACastleCharacter::Input_CrouchToggle);
	}
	if (FireAction)
	{
		// Triggered (not Started) so a Hold/Pulse trigger on the action gives automatic fire.
		EnhancedInput->BindAction(FireAction, ETriggerEvent::Triggered, this, &ACastleCharacter::Input_Fire);
	}
	if (AimAction)
	{
		EnhancedInput->BindAction(AimAction, ETriggerEvent::Started, this, &ACastleCharacter::Input_AimStarted);
		EnhancedInput->BindAction(AimAction, ETriggerEvent::Completed, this, &ACastleCharacter::Input_AimCompleted);
	}
	if (ReloadAction)
	{
		EnhancedInput->BindAction(ReloadAction, ETriggerEvent::Started, this, &ACastleCharacter::Input_Reload);
	}
	if (TakedownAction)
	{
		EnhancedInput->BindAction(TakedownAction, ETriggerEvent::Started, this, &ACastleCharacter::Input_Takedown);
	}
	if (InteractAction)
	{
		EnhancedInput->BindAction(InteractAction, ETriggerEvent::Started, this, &ACastleCharacter::Input_Interact);
	}
}

void ACastleCharacter::Input_Move(const FInputActionValue& Value)
{
	const FVector2D MoveInput = Value.Get<FVector2D>();
	if (MoveInput.IsNearlyZero() || !Controller || IsLockedOutByTakedown())
	{
		return;
	}

	const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), MoveInput.Y);
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), MoveInput.X);
}

void ACastleCharacter::Input_Look(const FInputActionValue& Value)
{
	const FVector2D LookInput = Value.Get<FVector2D>();

	AddControllerYawInput(LookInput.X);
	AddControllerPitchInput(LookInput.Y);
}

void ACastleCharacter::Input_SprintStarted(const FInputActionValue& /*Value*/)
{
	bIsSprinting = true;

	// You cannot sprint down the sights; the aim drops before the speed goes up.
	StopAim();
	UpdateMaxWalkSpeed();

	// Sprinting cancels a reload; the magazine keeps whatever it had.
	if (UWeaponComponent* Weapon = GetWeaponComponent())
	{
		Weapon->CancelReload();
	}
}

void ACastleCharacter::Input_SprintCompleted(const FInputActionValue& /*Value*/)
{
	bIsSprinting = false;
	UpdateMaxWalkSpeed();
}

void ACastleCharacter::Input_AimStarted(const FInputActionValue& /*Value*/)
{
	StartAim();
}

void ACastleCharacter::Input_AimCompleted(const FInputActionValue& /*Value*/)
{
	StopAim();
}

void ACastleCharacter::StartAim()
{
	if (bIsAiming || bIsSprinting || IsLockedOutByTakedown())
	{
		return;
	}

	bIsAiming = true;
	UpdateMaxWalkSpeed();

	if (UWeaponComponent* Weapon = GetWeaponComponent())
	{
		Weapon->SetAiming(true);
	}
}

void ACastleCharacter::StopAim()
{
	if (!bIsAiming)
	{
		// Still clear the weapon: a pickup mid-aim could otherwise leave the flags disagreeing.
		if (UWeaponComponent* Weapon = GetWeaponComponent())
		{
			Weapon->SetAiming(false);
		}
		return;
	}

	bIsAiming = false;
	UpdateMaxWalkSpeed();

	if (UWeaponComponent* Weapon = GetWeaponComponent())
	{
		Weapon->SetAiming(false);
	}
}

void ACastleCharacter::UpdateMaxWalkSpeed()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!Movement)
	{
		return;
	}

	if (bIsSprinting)
	{
		Movement->MaxWalkSpeed = SprintSpeed;
		return;
	}

	Movement->MaxWalkSpeed = bIsAiming ? WalkSpeed * AimSpeedMultiplier : WalkSpeed;
}

float ACastleCharacter::GetCurrentFOV() const
{
	return FirstPersonCamera ? FirstPersonCamera->FieldOfView : HipFOV;
}

void ACastleCharacter::UpdateAimFOV(float DeltaSeconds)
{
	if (!FirstPersonCamera)
	{
		return;
	}

	const float TargetFOV = bIsAiming ? AimFOV : HipFOV;
	const float Current = FirstPersonCamera->FieldOfView;
	if (FMath::IsNearlyEqual(Current, TargetFOV, 0.01f))
	{
		return;
	}

	if (AimBlendSeconds <= 0.f)
	{
		FirstPersonCamera->SetFieldOfView(TargetFOV);
		return;
	}

	// Constant rate rather than an exponential ease, so the blend really takes AimBlendSeconds.
	const float Step = FMath::Abs(HipFOV - AimFOV) / AimBlendSeconds * DeltaSeconds;
	FirstPersonCamera->SetFieldOfView(FMath::FInterpConstantTo(Current, TargetFOV, 1.f, Step));
}

void ACastleCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateAimFOV(DeltaSeconds);
}

void ACastleCharacter::Input_CrouchToggle(const FInputActionValue& /*Value*/)
{
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Crouch();
	}
}

bool ACastleCharacter::IsLockedOutByTakedown() const
{
	return TakedownComponent && TakedownComponent->IsPerformingTakedown();
}

void ACastleCharacter::Input_Fire(const FInputActionValue& /*Value*/)
{
	if (IsLockedOutByTakedown())
	{
		return;
	}

	UWeaponComponent* Weapon = GetWeaponComponent();
	if (Weapon && Weapon->Fire())
	{
		// A gunshot is the loudest thing in the level; every guard in range goes Alerted.
		MakeNoise(GunshotNoiseLoudness, this, GetActorLocation());
	}
}

void ACastleCharacter::Input_Reload(const FInputActionValue& /*Value*/)
{
	if (UWeaponComponent* Weapon = GetWeaponComponent())
	{
		Weapon->Reload();
	}
}

void ACastleCharacter::Input_Takedown(const FInputActionValue& /*Value*/)
{
	if (TakedownComponent)
	{
		TakedownComponent->TryTakedown();
	}
}

void ACastleCharacter::Input_Interact(const FInputActionValue& /*Value*/)
{
	if (InteractionComponent)
	{
		InteractionComponent->TryInteract();
	}

	OnInteractPressed();
}

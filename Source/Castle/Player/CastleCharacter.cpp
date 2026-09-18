// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/CastleCharacter.h"

#include "Camera/CameraComponent.h"
#include "Castle.h"
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

ACastleCharacter::ACastleCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	// First-person: the controller drives the camera, not the mesh.
	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, EyeHeightOffset));
	FirstPersonCamera->bUsePawnControlRotation = true;

	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
	TakedownComponent = CreateDefaultSubobject<UTakedownComponent>(TEXT("TakedownComponent"));

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
}

void ACastleCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = WalkSpeed;
	}
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
	return FindComponentByClass<UWeaponComponent>();
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
	if (MoveInput.IsNearlyZero() || !Controller)
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
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = SprintSpeed;
	}
}

void ACastleCharacter::Input_SprintCompleted(const FInputActionValue& /*Value*/)
{
	bIsSprinting = false;
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = WalkSpeed;
	}
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

void ACastleCharacter::Input_Fire(const FInputActionValue& /*Value*/)
{
	if (UWeaponComponent* Weapon = GetWeaponComponent())
	{
		Weapon->Fire();
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
	OnInteractPressed();
}

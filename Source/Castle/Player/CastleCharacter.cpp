// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/CastleCharacter.h"

#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "Castle.h"
#include "CastleGameMode.h"
#include "CastlePlayerController.h"
#include "Combat/HealthComponent.h"
#include "Combat/TakedownComponent.h"
#include "Combat/WeaponComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Player/InventoryComponent.h"
#include "Player/LocomotionAnim.h"
#include "Settings/CastleSettingsSubsystem.h"
#include "Components/PawnNoiseEmitterComponent.h"
#include "TimerManager.h"
#include "World/InteractionComponent.h"

ACastleCharacter::ACastleCharacter()
{
	// Ticks to blend the aim FOV and swap the body's idle/walk sequence.
	PrimaryActorTick.bCanEverTick = true;

	// Third person: the controller turns the camera boom, and the body turns to face where it
	// is going rather than where the camera is looking.
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	// TODO(stage2): placeholder rig; the camera task replaces it. Do not tune these here.
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetCapsuleComponent());
	CameraBoom->TargetArmLength = CameraBoomLength;
	CameraBoom->SocketOffset = CameraBoomSocketOffset;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = CameraLagSpeed;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	FollowCamera->SetFieldOfView(HipFOV);

	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
	TakedownComponent = CreateDefaultSubobject<UTakedownComponent>(TEXT("TakedownComponent"));
	InteractionComponent = CreateDefaultSubobject<UInteractionComponent>(TEXT("InteractionComponent"));
	NoiseEmitter = CreateDefaultSubobject<UPawnNoiseEmitterComponent>(TEXT("NoiseEmitter"));

	// The player starts on Hands; the inventory arms the component with whatever slot is active.
	WeaponComponent = CreateDefaultSubobject<UWeaponComponent>(TEXT("WeaponComponent"));
	WeaponComponent->bHasWeapon = false;

	InventoryComponent = CreateDefaultSubobject<UInventoryComponent>(TEXT("InventoryComponent"));

	// The whole body is visible, to the owner as well: in third person it is what the player
	// looks at. It sits at the template's standard offset under the capsule, facing +X.
	if (USkeletalMeshComponent* BodyMesh = GetMesh())
	{
		BodyMesh->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -96.f), FRotator(0.f, -90.f, 0.f));
		BodyMesh->SetOwnerNoSee(false);
		BodyMesh->bCastDynamicShadow = true;
		BodyMesh->SetCastShadow(true);
	}

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = WalkSpeed;
		Movement->NavAgentProps.bCanCrouch = true;
		Movement->bOrientRotationToMovement = true;
		Movement->JumpZVelocity = 480.f;
		Movement->AirControl = 0.35f;
	}

	// Lets the player crouch under and through geometry without the capsule popping.
	GetCapsuleComponent()->SetCapsuleSize(34.f, 88.f);

	// The Pawn profile ignores Visibility, so bullets need their own channel to land on the
	// player at all - without this the thugs' shots went straight through.
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

	if (FollowCamera)
	{
		FollowCamera->SetFieldOfView(HipFOV);
	}

	if (HealthComponent)
	{
		HealthComponent->OnDeath.AddDynamic(this, &ACastleCharacter::HandleDeath);
	}

	UpdateBodyLocomotion();

	BindToSettingsSubsystem();

	// Thugs hear the player through AISense_Hearing; MakeNoise on a fixed beat is enough
	// resolution for a stealth game and costs nothing per frame.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			NoiseTimerHandle, this, &ACastleCharacter::EmitMovementNoise, NoiseIntervalSeconds, true);
	}
}

void ACastleCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// BeginPlay may have run before the game instance had its subsystems; possession is the
	// second, reliable chance to pick the player's sensitivity up.
	BindToSettingsSubsystem();
}

void ACastleCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFromSettingsSubsystem();

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
	// The ring lives in the inventory now; doors and tests still ask the pawn.
	return InventoryComponent && InventoryComponent->HasKeycard(KeycardId);
}

bool ACastleCharacter::GiveKeycard(FName KeycardId)
{
	return InventoryComponent && InventoryComponent->GiveKeycard(KeycardId);
}

TSet<FName> ACastleCharacter::GetKeycards() const
{
	return InventoryComponent ? InventoryComponent->GetKeycards() : TSet<FName>();
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
	if (Slot1Action)
	{
		EnhancedInput->BindAction(Slot1Action, ETriggerEvent::Started, this, &ACastleCharacter::Input_Slot1);
	}
	if (Slot2Action)
	{
		EnhancedInput->BindAction(Slot2Action, ETriggerEvent::Started, this, &ACastleCharacter::Input_Slot2);
	}
	if (Slot3Action)
	{
		EnhancedInput->BindAction(Slot3Action, ETriggerEvent::Started, this, &ACastleCharacter::Input_Slot3);
	}
	if (SlotScrollAction)
	{
		// Triggered, not Started: the wheel is an axis and every notch is its own value.
		EnhancedInput->BindAction(SlotScrollAction, ETriggerEvent::Triggered, this, &ACastleCharacter::Input_SlotScroll);
	}
	if (InventoryAction)
	{
		EnhancedInput->BindAction(InventoryAction, ETriggerEvent::Started, this, &ACastleCharacter::Input_Inventory);
	}
}

void ACastleCharacter::Input_Slot1(const FInputActionValue& /*Value*/)
{
	if (InventoryComponent)
	{
		InventoryComponent->SelectSlot(EHotbarSlot::Hands);
	}
}

void ACastleCharacter::Input_Slot2(const FInputActionValue& /*Value*/)
{
	if (InventoryComponent)
	{
		InventoryComponent->SelectSlot(EHotbarSlot::Bow);
	}
}

void ACastleCharacter::Input_Slot3(const FInputActionValue& /*Value*/)
{
	if (InventoryComponent)
	{
		InventoryComponent->SelectSlot(EHotbarSlot::Reserved);
	}
}

void ACastleCharacter::Input_SlotScroll(const FInputActionValue& Value)
{
	const float Axis = Value.Get<float>();
	if (!InventoryComponent || FMath::IsNearlyZero(Axis))
	{
		return;
	}

	if (Axis > 0.f)
	{
		InventoryComponent->SelectNextSlot();
	}
	else
	{
		InventoryComponent->SelectPreviousSlot();
	}
}

void ACastleCharacter::Input_Inventory(const FInputActionValue& /*Value*/)
{
	if (ACastlePlayerController* PC = Cast<ACastlePlayerController>(GetController()))
	{
		PC->ToggleInventory();
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

void ACastleCharacter::BindToSettingsSubsystem()
{
	UCastleSettingsSubsystem* SettingsSubsystem = UCastleSettingsSubsystem::Get(this);
	if (!SettingsSubsystem)
	{
		// No game instance: an automation world. LookSensitivity is the fallback.
		return;
	}

	SettingsLookSensitivity = SettingsSubsystem->GetLookSensitivity();
	bHasSettingsLookSensitivity = true;

	if (!SettingsSubsystem->OnSettingsChanged.IsAlreadyBound(this, &ACastleCharacter::HandleSettingsChanged))
	{
		SettingsSubsystem->OnSettingsChanged.AddDynamic(this, &ACastleCharacter::HandleSettingsChanged);
	}
}

void ACastleCharacter::UnbindFromSettingsSubsystem()
{
	if (UCastleSettingsSubsystem* SettingsSubsystem = UCastleSettingsSubsystem::Get(this))
	{
		SettingsSubsystem->OnSettingsChanged.RemoveDynamic(this, &ACastleCharacter::HandleSettingsChanged);
	}
}

void ACastleCharacter::HandleSettingsChanged(FCastleSettings NewSettings)
{
	// Live, so the slider can be felt while the pause menu is still open.
	SettingsLookSensitivity = NewSettings.LookSensitivity;
	bHasSettingsLookSensitivity = true;
}

float ACastleCharacter::GetEffectiveLookSensitivity() const
{
	const float Base = bHasSettingsLookSensitivity ? SettingsLookSensitivity : LookSensitivity;
	return bIsAiming ? Base * AimLookMultiplier : Base;
}

FVector2D ACastleCharacter::ComputeLookDelta(FVector2D RawInput, bool bAiming) const
{
	const float Base = bHasSettingsLookSensitivity ? SettingsLookSensitivity : LookSensitivity;
	return RawInput * (bAiming ? Base * AimLookMultiplier : Base);
}

void ACastleCharacter::Input_Look(const FInputActionValue& Value)
{
	const FVector2D LookInput = ComputeLookDelta(Value.Get<FVector2D>(), bIsAiming);

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
	return FollowCamera ? FollowCamera->FieldOfView : HipFOV;
}

void ACastleCharacter::UpdateAimFOV(float DeltaSeconds)
{
	if (!FollowCamera)
	{
		return;
	}

	const float TargetFOV = bIsAiming ? AimFOV : HipFOV;
	const float Current = FollowCamera->FieldOfView;
	if (FMath::IsNearlyEqual(Current, TargetFOV, 0.01f))
	{
		return;
	}

	if (AimBlendSeconds <= 0.f)
	{
		FollowCamera->SetFieldOfView(TargetFOV);
		return;
	}

	// Constant rate rather than an exponential ease, so the blend really takes AimBlendSeconds.
	const float Step = FMath::Abs(HipFOV - AimFOV) / AimBlendSeconds * DeltaSeconds;
	FollowCamera->SetFieldOfView(FMath::FInterpConstantTo(Current, TargetFOV, 1.f, Step));
}

void ACastleCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateAimFOV(DeltaSeconds);
	UpdateBodyLocomotion();
}

void ACastleCharacter::UpdateBodyLocomotion()
{
	UAnimSequence* Wanted = GetVelocity().Size2D() > WalkAnimSpeedThreshold ? WalkAnim : IdleAnim;
	CastleLocomotion::PlayIfChanged(GetMesh(), Wanted, CurrentLocomotionAnim);
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
	if (!Weapon || !Weapon->Fire())
	{
		return;
	}

	if (Weapon->IsMelee())
	{
		// A punch is quiet: it is the stealth option that does not bring the block down on you.
		return;
	}

	// TODO(stage2): replaced by bow. The hitscan is only kept because thugs still shoot; the
	// player has no ranged definition to fire it with any more.
	// A gunshot is the loudest thing in the level; every thug in range goes Alerted.
	MakeNoise(GunshotNoiseLoudness, this, GetActorLocation());
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

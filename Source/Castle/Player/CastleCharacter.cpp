// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/CastleCharacter.h"

#include "Camera/CameraComponent.h"
#include "Castle.h"
#include "CastleGameMode.h"
#include "Combat/HealthComponent.h"
#include "Combat/TakedownComponent.h"
#include "Combat/WeaponComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Settings/CastleSettingsSubsystem.h"
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

	// --- view model ---------------------------------------------------------------------------
	ArmsMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ArmsMesh"));
	ArmsMesh->SetupAttachment(FirstPersonCamera);
	ArmsMesh->SetRelativeLocationAndRotation(ArmsRelativeLocation, ArmsRelativeRotation);
	ArmsMesh->SetOnlyOwnerSee(true);
	ArmsMesh->SetCastShadow(false);
	ArmsMesh->bCastDynamicShadow = false;
	ArmsMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// Off unless bUseArmsMesh: the full-body mannequin wraps the camera in its own torso.
	ArmsMesh->SetVisibility(false);
	ArmsMesh->SetHiddenInGame(true);
	ArmsMesh->SetComponentTickEnabled(false);

	// The pistol is the whole view model. It hangs off the camera, not off the arms, so its
	// offsets are read directly in camera space: X forward, Y right, Z up.
	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(FirstPersonCamera);
	WeaponMesh->SetRelativeLocationAndRotation(WeaponRelativeLocation, WeaponRelativeRotation);
	WeaponMesh->SetOnlyOwnerSee(true);
	WeaponMesh->SetCastShadow(false);
	WeaponMesh->bCastDynamicShadow = false;
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetHiddenInGame(true);

	MuzzleFlash = CreateDefaultSubobject<UPointLightComponent>(TEXT("MuzzleFlash"));
	MuzzleFlash->SetupAttachment(WeaponMesh);
	// In the pistol's own space the barrel runs along +Y and ends at y = 20.9, so the flash
	// sits just past the muzzle rather than inside the slide.
	MuzzleFlash->SetRelativeLocation(FVector(0.f, 23.f, 3.f));
	MuzzleFlash->SetIntensity(6000.f);
	MuzzleFlash->SetAttenuationRadius(600.f);
	MuzzleFlash->SetLightColor(FLinearColor(1.f, 0.78f, 0.42f));
	MuzzleFlash->SetCastShadows(false);
	MuzzleFlash->SetMobility(EComponentMobility::Movable);
	MuzzleFlash->SetVisibility(false);

	// The UE4 mannequin is a whole body; hiding a bone hides its children, so these three
	// hide the legs and the head and leave the arms.
	HiddenViewModelBones = { FName(TEXT("thigh_l")), FName(TEXT("thigh_r")), FName(TEXT("neck_01")) };

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

	InitialiseViewModel();

	BindToSettingsSubsystem();

	// Guards hear the player through AISense_Hearing; MakeNoise on a fixed beat is enough
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
		World->GetTimerManager().ClearTimer(MuzzleFlashTimerHandle);
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
	UpdateViewModel(DeltaSeconds);
}

void ACastleCharacter::InitialiseViewModel()
{
	if (!ArmsMesh)
	{
		return;
	}

	const bool bArmsActive = bUseArmsMesh && ArmsMesh->GetSkeletalMeshAsset() != nullptr;

	ArmsMesh->SetVisibility(bArmsActive);
	ArmsMesh->SetHiddenInGame(!bArmsActive);
	ArmsMesh->SetComponentTickEnabled(bArmsActive);

	if (bArmsActive)
	{
		for (const FName& BoneName : HiddenViewModelBones)
		{
			if (ArmsMesh->GetBoneIndex(BoneName) != INDEX_NONE)
			{
				ArmsMesh->HideBoneByName(BoneName, EPhysBodyOp::PBO_None);
			}
		}
	}

	if (WeaponMesh)
	{
		// With no arms the pistol is parented straight to the camera, so WeaponRelativeLocation
		// is read in camera space. With arms on, hand_r carries it instead.
		if (bArmsActive)
		{
			const FName Socket = (ArmsMesh->DoesSocketExist(WeaponSocketName) ? WeaponSocketName : NAME_None);
			WeaponMesh->AttachToComponent(
				ArmsMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
		}
		else if (FirstPersonCamera)
		{
			WeaponMesh->AttachToComponent(
				FirstPersonCamera, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		}
		WeaponMesh->SetRelativeLocationAndRotation(WeaponRelativeLocation, WeaponRelativeRotation);
	}

	RefreshViewModelForWeapon();
}

void ACastleCharacter::RefreshViewModelForWeapon()
{
	const UWeaponComponent* Weapon = GetWeaponComponent();
	const bool bArmed = Weapon && Weapon->HasWeapon();
	bViewModelArmed = bArmed;

	if (WeaponMesh)
	{
		WeaponMesh->SetHiddenInGame(!bArmed);
	}

	if (!bUseArmsMesh || !ArmsMesh || !ArmsMesh->GetSkeletalMeshAsset())
	{
		return;
	}

	UAnimSequence* Pose = bArmed ? ArmsPistolIdleAnim : ArmsIdleAnim;
	if (!Pose)
	{
		return;
	}

	ArmsMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	ArmsMesh->PlayAnimation(Pose, /*bLooping=*/true);
}

float ACastleCharacter::GetRecoilAlpha() const
{
	if (RecoilElapsed < 0.f)
	{
		return 0.f;
	}

	if (RecoilElapsed < RecoilKickSeconds)
	{
		return RecoilKickSeconds > 0.f ? RecoilElapsed / RecoilKickSeconds : 1.f;
	}

	const float ReturnElapsed = RecoilElapsed - RecoilKickSeconds;
	if (RecoilReturnSeconds <= 0.f || ReturnElapsed >= RecoilReturnSeconds)
	{
		return 0.f;
	}

	return 1.f - ReturnElapsed / RecoilReturnSeconds;
}

FVector ACastleCharacter::GetViewModelOffset() const
{
	const float RecoilAlpha = GetRecoilAlpha();
	// Aiming slides the pistol from its hip pose to the centred one, so the sights rise to the
	// crosshair. Everything else is added on top of wherever that lands.
	FVector Offset = (WeaponAimLocation - WeaponRelativeLocation) * AimOffsetAlpha;
	Offset.X -= RecoilKickDistance * RecoilAlpha;

	const UWeaponComponent* Weapon = GetWeaponComponent();
	// No reload animation yet, so the arms drop out of frame and come back instead.
	if (Weapon && Weapon->IsReloading() && !ArmsReloadAnim)
	{
		Offset.Z -= ReloadDipDistance;
	}

	// Bob is a sine on the phase, scaled by how fast the pawn is actually moving.
	const float SpeedAlpha = SprintSpeed > 0.f
		? FMath::Clamp(GetVelocity().Size2D() / SprintSpeed, 0.f, 1.f) : 0.f;
	Offset.Z += FMath::Sin(SwayPhase) * SwayAmplitude * SpeedAlpha;
	Offset.Y += FMath::Sin(SwayPhase * 0.5f) * SwayAmplitude * 0.5f * SpeedAlpha;

	return Offset;
}

void ACastleCharacter::UpdateViewModel(float DeltaSeconds)
{
	const UWeaponComponent* Weapon = GetWeaponComponent();
	if (Weapon && Weapon->HasWeapon() != bViewModelArmed)
	{
		RefreshViewModelForWeapon();
	}

	if (RecoilElapsed >= 0.f)
	{
		RecoilElapsed += DeltaSeconds;
		if (RecoilElapsed > RecoilKickSeconds + RecoilReturnSeconds)
		{
			RecoilElapsed = -1.f;
		}
	}

	const float SpeedAlpha = SprintSpeed > 0.f
		? FMath::Clamp(GetVelocity().Size2D() / SprintSpeed, 0.f, 1.f) : 0.f;
	SwayPhase = FMath::Fmod(SwayPhase + DeltaSeconds * SwayCyclesPerSecond * 2.f * PI * SpeedAlpha, 2.f * PI);

	const float AimStep = AimBlendSeconds > 0.f ? DeltaSeconds / AimBlendSeconds : 1.f;
	AimOffsetAlpha = FMath::Clamp(AimOffsetAlpha + (bIsAiming ? AimStep : -AimStep), 0.f, 1.f);

	const FVector Offset = GetViewModelOffset();
	const float RecoilPitch = RecoilKickPitchDegrees * GetRecoilAlpha();

	// The pistol is the view model, so recoil, dip and sway are written onto it. The arms, when
	// they exist at all, ride along behind it.
	if (WeaponMesh && WeaponMesh->GetAttachParent() == FirstPersonCamera)
	{
		FRotator Rotation = WeaponRelativeRotation;
		Rotation.Pitch += RecoilPitch;
		WeaponMesh->SetRelativeLocationAndRotation(WeaponRelativeLocation + Offset, Rotation);
	}

	if (bUseArmsMesh && ArmsMesh)
	{
		FRotator Rotation = ArmsRelativeRotation;
		Rotation.Pitch += RecoilPitch;
		ArmsMesh->SetRelativeLocationAndRotation(ArmsRelativeLocation + Offset, Rotation);
	}
}

void ACastleCharacter::PlayFireFeedback()
{
	RecoilElapsed = 0.f;

	if (bUseArmsMesh && ArmsMesh && ArmsFireAnim && ArmsMesh->GetSkeletalMeshAsset())
	{
		ArmsMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		ArmsMesh->PlayAnimation(ArmsFireAnim, /*bLooping=*/false);
	}

	if (!MuzzleFlash)
	{
		return;
	}

	MuzzleFlash->SetVisibility(true);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			MuzzleFlashTimerHandle, this, &ACastleCharacter::EndMuzzleFlash, MuzzleFlashSeconds, false);
	}
}

void ACastleCharacter::EndMuzzleFlash()
{
	if (MuzzleFlash)
	{
		MuzzleFlash->SetVisibility(false);
	}

	// A fire animation is one-shot; drop back to the idle pose it interrupted.
	if (ArmsFireAnim)
	{
		RefreshViewModelForWeapon();
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
		PlayFireFeedback();
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

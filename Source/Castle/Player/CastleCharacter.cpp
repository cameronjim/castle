// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/CastleCharacter.h"

#include "Animation/AnimInstance.h"
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
#include "Player/GrappleComponent.h"
#include "Player/InventoryComponent.h"
#include "Player/LocomotionAnim.h"
#include "Player/ParkourComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Settings/CastleSettingsSubsystem.h"
#include "Components/PawnNoiseEmitterComponent.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"
#include "World/InteractionComponent.h"

static TAutoConsoleVariable<int32> CVarCastleDebugMovement(
	TEXT("castle.DebugMovement"),
	0,
	TEXT("1 shows the player's gait, ground speed and fall height on the HUD, for reading playtest screenshots."),
	ECVF_Default);

namespace CastleGasp
{
	/** The Game Animation Sample's S_PlayerInputState variable on SandboxCharacter_CMC. */
	static const FName InputStateName(TEXT("CharacterInputState"));

	/** SandboxCharacter_CMC's analog-stick flag: true when the stick is past its walk range. */
	static const FName FullMovementInputName(TEXT("FullMovementInput"));

	static FStructProperty* FindInputState(const UClass* Class)
	{
		return Class ? FindFProperty<FStructProperty>(Class, InputStateName) : nullptr;
	}

	/**
	 * A member of the user-defined struct by the name its author gave it. The struct's real
	 * property names carry a GUID suffix (WantsToSprint_1_840C...), which is why this matches the
	 * authored name and not the FName.
	 */
	static FBoolProperty* FindFlag(const FStructProperty* InputState, FName FlagName)
	{
		if (!InputState || !InputState->Struct)
		{
			return nullptr;
		}
		const FString Wanted = FlagName.ToString();
		for (TFieldIterator<FBoolProperty> It(InputState->Struct); It; ++It)
		{
			if (It->GetAuthoredName() == Wanted)
			{
				return *It;
			}
		}
		return nullptr;
	}
}

ACastleCharacter::ACastleCharacter()
{
	// Ticks to blend the aim FOV and swap the body's idle/walk sequence.
	PrimaryActorTick.bCanEverTick = true;

	// Third person: the controller turns the camera boom, and the body turns to face where it
	// is going rather than where the camera is looking.
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	// The arm sweeps a small sphere on the Camera channel and pulls in when it hits, so backing
	// into a tenement puts the camera at the wall rather than inside it.
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetCapsuleComponent());
	CameraBoom->TargetArmLength = HipCamera.ArmLength;
	CameraBoom->SocketOffset = HipCamera.SocketOffset;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = CameraLagSpeed;
	CameraBoom->bEnableCameraRotationLag = true;
	CameraBoom->CameraRotationLagSpeed = CameraRotationLagSpeed;
	CameraBoom->bDoCollisionTest = true;
	CameraBoom->ProbeSize = CameraProbeSize;
	CameraBoom->ProbeChannel = ECC_Camera;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	FollowCamera->SetFieldOfView(HipCamera.FieldOfView);

	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
	TakedownComponent = CreateDefaultSubobject<UTakedownComponent>(TEXT("TakedownComponent"));
	InteractionComponent = CreateDefaultSubobject<UInteractionComponent>(TEXT("InteractionComponent"));
	NoiseEmitter = CreateDefaultSubobject<UPawnNoiseEmitterComponent>(TEXT("NoiseEmitter"));

	// The player starts on Hands; the inventory arms the component with whatever slot is active.
	WeaponComponent = CreateDefaultSubobject<UWeaponComponent>(TEXT("WeaponComponent"));
	WeaponComponent->bHasWeapon = false;

	InventoryComponent = CreateDefaultSubobject<UInventoryComponent>(TEXT("InventoryComponent"));
	GrappleComponent = CreateDefaultSubobject<UGrappleComponent>(TEXT("GrappleComponent"));
	ParkourComponent = CreateDefaultSubobject<UParkourComponent>(TEXT("ParkourComponent"));

	// The whole body is visible, to the owner as well: in third person it is what the player
	// looks at. Feet on the bottom of the capsule, facing +X.
	static constexpr float CapsuleHalfHeight = 88.f;
	if (USkeletalMeshComponent* BodyMesh = GetMesh())
	{
		BodyMesh->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -CapsuleHalfHeight), FRotator(0.f, -90.f, 0.f));
		BodyMesh->SetOwnerNoSee(false);
		BodyMesh->bCastDynamicShadow = true;
		BodyMesh->SetCastShadow(true);
	}

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = WalkSpeed;
		Movement->MaxWalkSpeedCrouched = CrouchSpeed;
		Movement->NavAgentProps.bCanCrouch = true;
		Movement->bCanWalkOffLedgesWhenCrouching = true;
		Movement->bOrientRotationToMovement = true;
		Movement->bUseControllerDesiredRotation = false;
		Movement->RotationRate = FRotator(0.f, TurnRateDegrees, 0.f);
		// sqrt(2 g h) for a 90 cm apex at standard gravity; BeginPlay recomputes it from JumpHeight.
		Movement->JumpZVelocity = 420.f;
		Movement->AirControl = JumpAirControl;
		Movement->SetCrouchedHalfHeight(CrouchedCapsuleHalfHeight);
	}

	GetCapsuleComponent()->SetCapsuleSize(34.f, CapsuleHalfHeight);

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

	ApplyTuningToComponents();
	UpdateMaxWalkSpeed();
	UpdateCamera(0.f);

	if (HealthComponent)
	{
		HealthComponent->OnDeath.AddDynamic(this, &ACastleCharacter::HandleDeath);
	}

	UpdateBodyLocomotion();
	SilenceForeignCameras();

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

	// The sample's graph may switch its own camera rig on as it is possessed.
	SilenceForeignCameras();
	ApplyCameraPitchLimits();
}

void ACastleCharacter::ApplyCameraPitchLimits()
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	APlayerCameraManager* CameraManager = PC ? PC->PlayerCameraManager.Get() : nullptr;
	if (CameraManager && (CameraManager->ViewPitchMin != CameraPitchMin || CameraManager->ViewPitchMax != CameraPitchMax))
	{
		CameraManager->ViewPitchMin = CameraPitchMin;
		CameraManager->ViewPitchMax = CameraPitchMax;
	}
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

	// Super bound the Blueprint input events after SetupPlayerInputComponent ran; drop them here.
	DropBlueprintInputBindings();
	AddDefaultMappingContext();
	SilenceForeignCameras();
	ApplyCameraPitchLimits();
}

void ACastleCharacter::DropBlueprintInputBindings()
{
	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInput || NativeActionBindingCount == INDEX_NONE || !UsesGaspLocomotion())
	{
		return;
	}

	int32 Dropped = 0;
	for (int32 Index = EnhancedInput->GetActionEventBindings().Num() - 1; Index >= NativeActionBindingCount; --Index)
	{
		Dropped += EnhancedInput->RemoveActionEventBinding(Index) ? 1 : 0;
	}
	// Raw key events (the sample's mouse wheel and d-pad camera controls); we bind none of our own.
	Dropped += EnhancedInput->KeyBindings.Num();
	EnhancedInput->KeyBindings.Reset();

	DroppedBlueprintInputBindings = Dropped;
	UE_LOG(LogCastle, Log, TEXT("%s: dropped %d Blueprint input binding(s); the sandbox graph reads CharacterInputState instead."),
		*GetNameSafe(this), Dropped);
}

void ACastleCharacter::SilenceForeignCameras()
{
	if (!UsesGaspLocomotion())
	{
		return;
	}

	TInlineComponentArray<UActorComponent*> Components(this);
	for (UActorComponent* Component : Components)
	{
		if (!Component || Component == FollowCamera || !Component->IsActive())
		{
			continue;
		}
		// The sample's GameplayCamera component (GameplayCameras plugin, which this module does
		// not link) is matched by class name; it owns and drives a cine camera it spawns.
		const bool bCamera = Component->IsA<UCameraComponent>()
			|| Component->GetClass()->GetName().Contains(TEXT("GameplayCamera"));
		if (!bCamera)
		{
			continue;
		}
		Component->Deactivate();
		if (!bLoggedForeignCamera)
		{
			bLoggedForeignCamera = true;
			UE_LOG(LogCastle, Log, TEXT("%s: switched off the sample's camera %s (%s); the spring arm is the view."),
				*GetNameSafe(this), *Component->GetName(), *Component->GetClass()->GetName());
		}
	}
}

bool ACastleCharacter::UsesGaspLocomotion() const
{
	return CastleGasp::FindInputState(GetClass()) != nullptr;
}

bool ACastleCharacter::IsBodyDrivenByAnimBlueprint() const
{
	const USkeletalMeshComponent* Body = GetMesh();
	return Body && Body->GetAnimationMode() == EAnimationMode::AnimationBlueprint && Body->AnimClass != nullptr;
}

bool ACastleCharacter::GetGaspInputFlag(FName FlagName) const
{
	const FStructProperty* InputState = CastleGasp::FindInputState(GetClass());
	const FBoolProperty* Flag = CastleGasp::FindFlag(InputState, FlagName);
	return Flag && Flag->GetPropertyValue_InContainer(InputState->ContainerPtrToValuePtr<void>(this));
}

void ACastleCharacter::SyncGaspInputState()
{
	FStructProperty* InputState = CastleGasp::FindInputState(GetClass());
	if (!InputState)
	{
		return;
	}

	void* State = InputState->ContainerPtrToValuePtr<void>(this);
	auto SetFlag = [InputState, State](const TCHAR* Name, bool bValue)
	{
		if (FBoolProperty* Flag = CastleGasp::FindFlag(InputState, FName(Name)))
		{
			Flag->SetPropertyValue_InContainer(State, bValue);
		}
	};

	SetFlag(TEXT("WantsToSprint"), CurrentGait == ECastleGait::Sprint);
	SetFlag(TEXT("WantsToWalk"), CurrentGait == ECastleGait::Walk || bIsAiming);
	SetFlag(TEXT("WantsToStrafe"), bIsAiming);
	SetFlag(TEXT("WantsToAim"), bIsAiming);
	SetFlag(TEXT("WantsToCrouch"), bIsCrouched || bIsSliding);

	if (FBoolProperty* FullInput = FindFProperty<FBoolProperty>(GetClass(), CastleGasp::FullMovementInputName))
	{
		FullInput->SetPropertyValue_InContainer(this, CurrentGait != ECastleGait::Walk);
	}
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
	if (GrappleAction)
	{
		EnhancedInput->BindAction(GrappleAction, ETriggerEvent::Started, this, &ACastleCharacter::Input_Grapple);
	}

	// Everything bound after this point is a Blueprint's; see DropBlueprintInputBindings.
	NativeActionBindingCount = EnhancedInput->GetActionEventBindings().Num();
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

void ACastleCharacter::Input_Grapple(const FInputActionValue& /*Value*/)
{
	if (GrappleComponent && !IsLockedOutByTakedown() && !IsTraversing())
	{
		GrappleComponent->TryFire();
	}
}

bool ACastleCharacter::IsZipping() const
{
	return GrappleComponent && GrappleComponent->IsZipping();
}

bool ACastleCharacter::IsTraversing() const
{
	return ParkourComponent && ParkourComponent->IsBusy();
}

void ACastleCharacter::NotifyGrappleLanded()
{
	// A zip ends on its landing point: no fall, so no damage and no roll. The roll placeholder
	// halved the speed on arrival, which made chaining off a landing feel like wading.
	FallApexZ = GetActorLocation().Z;
	ApplyLanding(0.f);
	UpdateMaxWalkSpeed();
}

void ACastleCharacter::Input_Move(const FInputActionValue& Value)
{
	const FVector2D MoveInput = Value.Get<FVector2D>();
	const bool bParkourLock = ParkourComponent && ParkourComponent->IsLockingInput();
	if (MoveInput.IsNearlyZero() || !Controller || IsLockedOutByTakedown() || IsZipping() || bParkourLock)
	{
		return;
	}

	// Keyboard diagonals come in at 1.41; the gait only cares how hard the stick is pushed.
	MoveInputMagnitude = FMath::Min(MoveInput.Size(), 1.f);
	bMoveInputThisFrame = true;
	UpdateMaxWalkSpeed();

	// A slide keeps the direction it started in; UpdateSlide does the pushing.
	if (bIsSliding)
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
	UE_LOG(LogCastle, Verbose, TEXT("%s: sprint start at %.0f cm/s"), *GetNameSafe(this), GetVelocity().Size2D());
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
	UE_LOG(LogCastle, Verbose, TEXT("%s: sprint stop at %.0f cm/s"), *GetNameSafe(this), GetVelocity().Size2D());
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
	ApplyRotationMode();

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
	ApplyRotationMode();

	if (UWeaponComponent* Weapon = GetWeaponComponent())
	{
		Weapon->SetAiming(false);
	}
}

void ACastleCharacter::ApplyTuningToComponents()
{
	if (CameraBoom)
	{
		CameraBoom->CameraLagSpeed = CameraLagSpeed;
		CameraBoom->CameraRotationLagSpeed = CameraRotationLagSpeed;
		CameraBoom->ProbeSize = CameraProbeSize;
	}

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!Movement)
	{
		return;
	}

	Movement->RotationRate = FRotator(0.f, TurnRateDegrees, 0.f);
	Movement->AirControl = JumpAirControl;
	Movement->MaxWalkSpeedCrouched = CrouchSpeed;
	Movement->SetCrouchedHalfHeight(CrouchedCapsuleHalfHeight);

	// Apex h = v^2 / 2g, so the designer tunes the height and the velocity follows gravity.
	const float Gravity = FMath::Abs(Movement->GetGravityZ());
	if (Gravity > KINDA_SMALL_NUMBER && JumpHeight > 0.f)
	{
		Movement->JumpZVelocity = FMath::Sqrt(2.f * Gravity * JumpHeight);
	}
	ApplyRotationMode();
}

void ACastleCharacter::ApplyRotationMode()
{
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->bOrientRotationToMovement = !bIsAiming;
		Movement->bUseControllerDesiredRotation = bIsAiming;
	}
}

ECastleGait ACastleCharacter::SelectGait(float InputMagnitude, float InputHeldSeconds, bool bWantsSprint,
	bool bCrouched, bool bSliding, bool bAiming) const
{
	if (bSliding)
	{
		return ECastleGait::Slide;
	}
	if (bCrouched)
	{
		return ECastleGait::Crouch;
	}
	if (bWantsSprint)
	{
		return ECastleGait::Sprint;
	}
	if (bAiming)
	{
		return ECastleGait::Walk;
	}
	if (InputMagnitude >= FullInputThreshold)
	{
		return ECastleGait::Run;
	}
	if (InputMagnitude >= LightInputThreshold && InputHeldSeconds > RunAfterHeldSeconds)
	{
		return ECastleGait::Run;
	}
	return ECastleGait::Walk;
}

float ACastleCharacter::GetGaitSpeed(ECastleGait Gait) const
{
	switch (Gait)
	{
	case ECastleGait::Run:
		return RunSpeed;
	case ECastleGait::Sprint:
		return SprintSpeed;
	case ECastleGait::Crouch:
		return CrouchSpeed;
	case ECastleGait::Slide:
		return SlideSpeed;
	case ECastleGait::Walk:
	default:
		return WalkSpeed;
	}
}

void ACastleCharacter::UpdateMaxWalkSpeed()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!Movement)
	{
		return;
	}

	CurrentGait = SelectGait(MoveInputMagnitude, MoveInputHeldSeconds, bIsSprinting, bIsCrouched, bIsSliding, bIsAiming);
	float Speed = GetGaitSpeed(CurrentGait);
	if (CurrentGait == ECastleGait::Slide && SlideSeconds > 0.f)
	{
		const float Progress = 1.f - FMath::Clamp(SlideRemaining / SlideSeconds, 0.f, 1.f);
		Speed = FMath::Lerp(SlideSpeed, CrouchSpeed, Progress);
	}
	if (LandingRecoverRemaining > 0.f)
	{
		Speed *= LandingSpeedMultiplier;
	}

	SyncGaspInputState();
	if (UsesGaspLocomotion())
	{
		// TODO(stage2): reconcile. The sandbox graph writes MaxWalkSpeed, crouch speed and
		// acceleration from its own Walk/Run/Sprint/Crouch speed curves before the movement
		// component ticks; writing ours as well would make the two fight frame to frame.
		if (!bLoggedGaspSpeedOwnership)
		{
			bLoggedGaspSpeedOwnership = true;
			UE_LOG(LogCastle, Log, TEXT("%s: the Game Animation Sample graph sets movement speeds; Castle gait speeds are not applied."),
				*GetNameSafe(this));
		}
		return;
	}

	// Crouch and slide move on the crouched speed; everything else on the walking one.
	const bool bLow = CurrentGait == ECastleGait::Crouch || CurrentGait == ECastleGait::Slide;
	Movement->MaxWalkSpeed = bLow ? RunSpeed : Speed;
	Movement->MaxWalkSpeedCrouched = bLow ? Speed : CrouchSpeed;
}

FCastleCameraTargets ACastleCharacter::ComputeCameraTargets(bool bAiming, float Pitch) const
{
	if (bAiming)
	{
		return AimCamera;
	}
	FCastleCameraTargets Targets = HipCamera;
	const float Span = LookUpPitchFull - LookUpPitchStart;
	const float Alpha = Span > 0.f
		? FMath::Clamp((Pitch - LookUpPitchStart) / Span, 0.f, 1.f)
		: (Pitch >= LookUpPitchFull ? 1.f : 0.f);
	Targets.ArmLength = FMath::Lerp(HipCamera.ArmLength, LookUpArmLength, Alpha);
	Targets.SocketOffset.Z = FMath::Lerp(HipCamera.SocketOffset.Z, LookUpSocketZ, Alpha);
	Targets.PivotLift = FMath::Lerp(HipCamera.PivotLift, LookUpPivotLift, Alpha);
	return Targets;
}

float ACastleCharacter::GetCurrentFOV() const
{
	return FollowCamera ? FollowCamera->FieldOfView : HipCamera.FieldOfView;
}

void ACastleCharacter::UpdateCamera(float DeltaSeconds)
{
	// Constant rate, so the blend really takes AimBlendSeconds; the smoothstep only shapes it.
	const float Target = bIsAiming ? 1.f : 0.f;
	AimAlpha = AimBlendSeconds > 0.f
		? FMath::FInterpConstantTo(AimAlpha, Target, DeltaSeconds, 1.f / AimBlendSeconds)
		: Target;

	const float Pitch = Controller ? FRotator::NormalizeAxis(Controller->GetControlRotation().Pitch) : 0.f;
	FCastleCameraTargets Blend = FCastleCameraTargets::Lerp(
		ComputeCameraTargets(false, Pitch), ComputeCameraTargets(true, Pitch), FMath::SmoothStep(0.f, 1.f, AimAlpha));

	// The roll placeholder: the camera sinks and comes back up over the recovery.
	if (LandingRecoverRemaining > 0.f && LandingRecoverSeconds > 0.f)
	{
		const float Phase = 1.f - LandingRecoverRemaining / LandingRecoverSeconds;
		Blend.SocketOffset.Z -= LandingCameraDip * FMath::Sin(PI * Phase);
	}

	if (CameraBoom)
	{
		CameraBoom->TargetArmLength = Blend.ArmLength;
		CameraBoom->SocketOffset = Blend.SocketOffset;
		CameraBoom->TargetOffset = FVector(0.f, 0.f, Blend.PivotLift);
	}
	if (FollowCamera)
	{
		FollowCamera->SetFieldOfView(Blend.FieldOfView);
	}
}

void ACastleCharacter::UpdateMoveInputTiming(float DeltaSeconds)
{
	if (bMoveInputThisFrame)
	{
		MoveInputHeldSeconds += DeltaSeconds;
	}
	else
	{
		MoveInputHeldSeconds = 0.f;
		MoveInputMagnitude = 0.f;
	}
	bMoveInputThisFrame = false;
}

void ACastleCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateMoveInputTiming(DeltaSeconds);
	UpdateSlide(DeltaSeconds);
	UpdateFalling(DeltaSeconds);
	UpdateMaxWalkSpeed();
	UpdateCamera(DeltaSeconds);
	UpdateBodyVisibilityForCamera();
	UpdateBodyLocomotion();
	SilenceForeignCameras();
	ApplyCameraPitchLimits();
}

void ACastleCharacter::UpdateBodyVisibilityForCamera()
{
	USkeletalMeshComponent* Body = GetMesh();
	if (!Body || !CameraBoom || !FollowCamera)
	{
		return;
	}

	// The arm places the lens after this tick, so this reads last frame's; a frame late is fine.
	const float LensDistance = FVector::Dist(FollowCamera->GetComponentLocation(), CameraBoom->GetComponentLocation());
	const bool bTooClose = LensDistance < CameraHideBodyDistance;
	if (Body->bOwnerNoSee != bTooClose)
	{
		Body->SetOwnerNoSee(bTooClose);
	}
}

void ACastleCharacter::UpdateBodyLocomotion()
{
	// An AnimBP (BP_Kate's motion matching) owns the body; single-node playback would replace it.
	if (IsBodyDrivenByAnimBlueprint())
	{
		return;
	}

	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	const float Speed = GetVelocity().Size2D();

	UAnimSequence* Wanted = IdleAnim;
	if (Movement && (Movement->IsFalling() || Movement->IsFlying()) && FallAnim)
	{
		Wanted = FallAnim;
	}
	else if (Speed > RunAnimSpeedThreshold && RunAnim)
	{
		Wanted = RunAnim;
	}
	else if (Speed > WalkAnimSpeedThreshold)
	{
		Wanted = WalkAnim;
	}
	CastleLocomotion::PlayIfChanged(GetMesh(), Wanted, CurrentLocomotionAnim);
}

void ACastleCharacter::Input_CrouchToggle(const FInputActionValue& /*Value*/)
{
	// Crouch lets go of a ledge.
	if (ParkourComponent && ParkourComponent->IsHanging())
	{
		ParkourComponent->DropFromHang();
		return;
	}
	// The slide stands itself up; a second press mid-slide is not a crouch. Nor is one mid-zip.
	if (bIsSliding || IsZipping() || IsTraversing())
	{
		return;
	}

	if (bIsCrouched)
	{
		UnCrouch();
	}
	else if (!(bIsSprinting && StartSlide()))
	{
		Crouch();
	}
	UpdateMaxWalkSpeed();
}

bool ACastleCharacter::StartSlide()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!Movement || bIsSliding || !bIsSprinting || !Movement->IsMovingOnGround())
	{
		return false;
	}

	const float Speed = GetVelocity().Size2D();
	if (Speed < SlideMinSpeed)
	{
		return false;
	}

	bIsSliding = true;
	SlideRemaining = SlideSeconds;
	SlideDirection = GetVelocity().GetSafeNormal2D();
	PreSlideGroundFriction = Movement->GroundFriction;
	Movement->GroundFriction = SlideGroundFriction;

	// Straight to the movement component, so the capsule is low this frame rather than next tick.
	Movement->bWantsToCrouch = true;
	Movement->Crouch();
	UpdateMaxWalkSpeed();

	UE_LOG(LogCastle, Log, TEXT("%s: slide start at %.0f cm/s, capsule half-height %.0f"),
		*GetNameSafe(this), Speed, GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight());
	return true;
}

void ACastleCharacter::UpdateSlide(float DeltaSeconds)
{
	if (!bIsSliding)
	{
		return;
	}

	SlideRemaining -= DeltaSeconds;
	AddMovementInput(SlideDirection, 1.f);
	if (SlideRemaining <= 0.f)
	{
		EndSlide();
	}
}

void ACastleCharacter::EndSlide()
{
	if (!bIsSliding)
	{
		return;
	}

	bIsSliding = false;
	SlideRemaining = 0.f;
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->GroundFriction = PreSlideGroundFriction;
		// UnCrouch checks for headroom; under a low ceiling the character stays crouched.
		Movement->bWantsToCrouch = false;
		Movement->UnCrouch();
	}
	UpdateMaxWalkSpeed();

	UE_LOG(LogCastle, Log, TEXT("%s: slide end at %.0f cm/s, crouched=%d"),
		*GetNameSafe(this), GetVelocity().Size2D(), bIsCrouched ? 1 : 0);
}

void ACastleCharacter::Jump()
{
	if (IsZipping())
	{
		return;
	}
	if (ParkourComponent)
	{
		// Hanging, jump climbs. Mid-move it does nothing. Otherwise an obstacle ahead turns it
		// into a vault, mantle or ledge grab, and only open ground gets a plain jump.
		if (ParkourComponent->IsHanging())
		{
			ParkourComponent->ClimbFromHang();
			return;
		}
		if (ParkourComponent->IsBusy())
		{
			return;
		}
		EndSlide();
		if (ParkourComponent->TryParkour(false))
		{
			return;
		}
	}
	EndSlide();
	Super::Jump();
}

void ACastleCharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);

	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (Movement && Movement->IsFalling())
	{
		// Measured from the top of the arc, so a jump off a roof counts its rise as well.
		FallApexZ = GetActorLocation().Z;
		EndSlide();
	}
}

void ACastleCharacter::UpdateFalling(float DeltaSeconds)
{
	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (Movement && Movement->IsFalling())
	{
		FallApexZ = FMath::Max(FallApexZ, GetActorLocation().Z);
	}

	if (LandingRecoverRemaining > 0.f)
	{
		LandingRecoverRemaining = FMath::Max(0.f, LandingRecoverRemaining - DeltaSeconds);
	}
}

float ACastleCharacter::GetCurrentFallHeight() const
{
	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!Movement || !Movement->IsFalling())
	{
		return 0.f;
	}
	return FMath::Max(0.f, FMath::Max(FallApexZ, GetActorLocation().Z) - GetActorLocation().Z);
}

void ACastleCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	const float Z = GetActorLocation().Z;
	ApplyLanding(FMath::Max(0.f, FMath::Max(FallApexZ, Z) - Z));
}

float ACastleCharacter::ComputeFallDamageFraction(float FallHeight) const
{
	if (FallHeight < FallDamageMinHeight)
	{
		return 0.f;
	}

	const float Span = FallDamageMaxHeight - FallDamageMinHeight;
	const float Alpha = Span > 0.f ? FMath::Clamp((FallHeight - FallDamageMinHeight) / Span, 0.f, 1.f) : 1.f;
	return FMath::Lerp(FallDamageMinFraction, FallDamageMaxFraction, Alpha);
}

void ACastleCharacter::ApplyLanding(float FallHeight)
{
	LastFallHeight = FallHeight;
	if (FallHeight >= 50.f)
	{
		UE_LOG(LogCastle, Log, TEXT("%s: landed from %.0f cm at %.0f cm/s"),
			*GetNameSafe(this), FallHeight, GetVelocity().Size2D());
	}

	if (FallHeight > RollHeight)
	{
		// TODO(stage2): the parkour step swaps this for a real roll (moving) or stumble (standing).
		LandingRecoverRemaining = LandingRecoverSeconds;
	}

	const float Fraction = ComputeFallDamageFraction(FallHeight);
	if (Fraction > 0.f && HealthComponent && HealthComponent->IsAlive())
	{
		// Soft by design: a fall takes a chunk, never the last point.
		const float Damage = FMath::Min(Fraction * HealthComponent->GetMaxHealth(),
			HealthComponent->GetCurrentHealth() - 1.f);
		if (Damage > 0.f)
		{
			HealthComponent->ApplyDamage(Damage, nullptr);
			UE_LOG(LogCastle, Log, TEXT("%s: fall damage %.1f (%.0f%% of max) from %.0f cm, health now %.1f"),
				*GetNameSafe(this), Damage, Fraction * 100.f, FallHeight, HealthComponent->GetCurrentHealth());
		}
	}
	UpdateMaxWalkSpeed();
}

bool ACastleCharacter::IsMovementDebugEnabled()
{
	return CVarCastleDebugMovement.GetValueOnGameThread() != 0;
}

FString ACastleCharacter::GetMovementDebugText() const
{
	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	const float Speed = GetVelocity().Size2D();

	const TCHAR* State = TEXT("walk");
	if (Movement && Movement->IsFalling())
	{
		State = TEXT("air");
	}
	else if (Speed < 10.f && CurrentGait != ECastleGait::Slide)
	{
		State = bIsCrouched ? TEXT("crouch") : TEXT("idle");
	}
	else
	{
		switch (CurrentGait)
		{
		case ECastleGait::Run: State = TEXT("run"); break;
		case ECastleGait::Sprint: State = TEXT("sprint"); break;
		case ECastleGait::Crouch: State = TEXT("crouch"); break;
		case ECastleGait::Slide: State = TEXT("slide"); break;
		default: break;
		}
	}

	return FString::Printf(TEXT("%s  %.0f cm/s  fall %.0f cm  last landing %.0f cm%s"),
		State, Speed, GetCurrentFallHeight(), LastFallHeight, IsRecoveringFromLanding() ? TEXT("  (roll)") : TEXT(""));
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

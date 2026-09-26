// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/CastleCharacter.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "Castle.h"
#include "CastleGameMode.h"
#include "Combat/BowComponent.h"
#include "CastlePlayerController.h"
#include "Combat/HealthComponent.h"
#include "Combat/MeleeComponent.h"
#include "Combat/TakedownComponent.h"
#include "Combat/WeaponComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/RootMotionSource.h"
#include "GameFramework/SpringArmComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "World/ThugCharacter.h"
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

	// Hands only, the melee fallback while no bow is owned; the bow does the shooting.
	WeaponComponent = CreateDefaultSubobject<UWeaponComponent>(TEXT("WeaponComponent"));
	WeaponComponent->bHasWeapon = false;
	BowComponent = CreateDefaultSubobject<UBowComponent>(TEXT("BowComponent"));

	// Bow strikes. Numbers from docs/plans/02-prototype.md step 7: a quick jab and a slow heavy
	// that puts a thug on the floor. The lunge stands in for an attack animation (none yet).
	MeleeComponent = CreateDefaultSubobject<UMeleeComponent>(TEXT("MeleeComponent"));
	LightAttack.Name = FName(TEXT("light"));
	LightAttack.Damage = 15.f;
	LightAttack.WindupSeconds = 0.1f;
	LightAttack.RecoverSeconds = 0.2f;
	LightAttack.Range = 120.f;
	LightAttack.Radius = 35.f;
	LightAttack.bStagger = true;
	LightAttack.LungeDistance = 20.f;
	HeavyAttack = LightAttack;
	HeavyAttack.Name = FName(TEXT("heavy"));
	HeavyAttack.Damage = 35.f;
	HeavyAttack.WindupSeconds = 0.6f;
	HeavyAttack.RecoverSeconds = 0.3f;
	HeavyAttack.bKnockdown = true;
	HeavyAttack.LungeDistance = 40.f;
	HeavyAttack.LungeSeconds = 0.15f;

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
		HealthComponent->OnStaggered.AddDynamic(this, &ACastleCharacter::HandleStaggered);
		HealthComponent->OnHealthChanged.AddDynamic(this, &ACastleCharacter::HandleHealthChanged);
	}
	if (MeleeComponent)
	{
		MeleeComponent->OnAttackLanded.AddDynamic(this, &ACastleCharacter::HandleMeleeLanded);
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
		if (World->GetTimerManager().IsTimerActive(HitStopTimerHandle))
		{
			World->GetTimerManager().ClearTimer(HitStopTimerHandle);
			EndHitStop();
		}
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
	// Drawing forces the aim, and the aim walks: that is the sample's share of the draw slow-down.
	SetFlag(TEXT("WantsToWalk"), CurrentGait == ECastleGait::Walk || bIsAiming || IsDrawingBow());
	SetFlag(TEXT("WantsToStrafe"), bIsAiming);
	SetFlag(TEXT("WantsToAim"), bIsAiming);
	SetFlag(TEXT("WantsToCrouch"), bIsCrouched || bIsSliding);
	// The sample has no attack input today; if a later version adds one, it is fed from here.
	SetFlag(TEXT("WantsToAttack"), IsMeleeAttacking());

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
		EnhancedInput->BindAction(CrouchAction, ETriggerEvent::Completed, this, &ACastleCharacter::Input_CrouchReleased);
	}
	if (FireAction)
	{
		// Hold to draw, let go to loose.
		EnhancedInput->BindAction(FireAction, ETriggerEvent::Started, this, &ACastleCharacter::Input_FirePressed);
		EnhancedInput->BindAction(FireAction, ETriggerEvent::Completed, this, &ACastleCharacter::Input_FireReleased);
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
	if (Slot4Action)
	{
		EnhancedInput->BindAction(Slot4Action, ETriggerEvent::Started, this, &ACastleCharacter::Input_Slot4);
	}
	if (Slot5Action)
	{
		EnhancedInput->BindAction(Slot5Action, ETriggerEvent::Started, this, &ACastleCharacter::Input_Slot5);
	}
	if (Slot6Action)
	{
		EnhancedInput->BindAction(Slot6Action, ETriggerEvent::Started, this, &ACastleCharacter::Input_Slot6);
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
	if (MeleeAction)
	{
		// Tap for the light, hold for the heavy: the release decides which, or the hold running out.
		EnhancedInput->BindAction(MeleeAction, ETriggerEvent::Started, this, &ACastleCharacter::Input_MeleePressed);
		EnhancedInput->BindAction(MeleeAction, ETriggerEvent::Completed, this, &ACastleCharacter::Input_MeleeReleased);
	}

	// Everything bound after this point is a Blueprint's; see DropBlueprintInputBindings.
	NativeActionBindingCount = EnhancedInput->GetActionEventBindings().Num();
}

void ACastleCharacter::Input_Slot1(const FInputActionValue& /*Value*/)
{
	if (InventoryComponent)
	{
		InventoryComponent->SelectArrowSlot(1);
	}
}

void ACastleCharacter::Input_Slot2(const FInputActionValue& /*Value*/)
{
	if (InventoryComponent)
	{
		InventoryComponent->SelectArrowSlot(2);
	}
}

void ACastleCharacter::Input_Slot3(const FInputActionValue& /*Value*/)
{
	if (InventoryComponent)
	{
		InventoryComponent->SelectArrowSlot(3);
	}
}

void ACastleCharacter::Input_Slot4(const FInputActionValue& /*Value*/)
{
	if (InventoryComponent)
	{
		InventoryComponent->SelectArrowSlot(4);
	}
}

void ACastleCharacter::Input_Slot5(const FInputActionValue& /*Value*/)
{
	if (InventoryComponent)
	{
		InventoryComponent->SelectArrowSlot(5);
	}
}

void ACastleCharacter::Input_Slot6(const FInputActionValue& /*Value*/)
{
	if (InventoryComponent)
	{
		InventoryComponent->SelectArrowSlot(6);
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
		InventoryComponent->SelectNextArrowSlot();
	}
	else
	{
		InventoryComponent->SelectPreviousArrowSlot();
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

	// Remembered even when the move itself is locked out, so a dodge out of a swing goes where the
	// stick points now.
	const FRotator InputYaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
	LastMoveWorldDirection = (FRotationMatrix(InputYaw).GetUnitAxis(EAxis::X) * MoveInput.Y
		+ FRotationMatrix(InputYaw).GetUnitAxis(EAxis::Y) * MoveInput.X).GetSafeNormal2D();

	// A swing, a dodge or a stagger owns the body for its length.
	if (IsMeleeAttacking() || IsDodging() || IsStaggered())
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

	// You cannot sprint down the sights; the aim (and any draw) drops before the speed goes up.
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
	bAimInputHeld = true;
	StartAim();
}

void ACastleCharacter::Input_AimCompleted(const FInputActionValue& /*Value*/)
{
	bAimInputHeld = false;
	// A bow still drawn keeps the aim until it is released.
	if (!IsDrawingBow())
	{
		StopAim();
	}
}

bool ACastleCharacter::IsDrawingBow() const
{
	return BowComponent && BowComponent->IsDrawing();
}

void ACastleCharacter::NotifyBowDrawStarted()
{
	if (bIsSprinting)
	{
		bIsSprinting = false;
	}
	StartAim();
	UpdateMaxWalkSpeed();
}

void ACastleCharacter::NotifyBowDrawEnded()
{
	if (!bAimInputHeld && !bStoppingAim)
	{
		StopAim();
	}
	UpdateMaxWalkSpeed();
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
	// Losing the aim (a sprint, a zip) lets the string down; nothing is fired.
	if (IsDrawingBow())
	{
		TGuardValue<bool> Guard(bStoppingAim, true);
		BowComponent->CancelDraw();
	}

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
	if (IsDrawingBow())
	{
		Speed *= DrawWalkSpeedMultiplier;
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

	// A hit on her throws the lens about for a moment; decaying, two unrelated frequencies.
	const float Shake = GetHitShakeAlpha();
	if (Shake > 0.f)
	{
		const float Time = (HitShakeSeconds - HitShakeRemaining) * 60.f;
		Blend.SocketOffset.Y += HitShakeAmplitude * Shake * FMath::Sin(Time * 1.7f);
		Blend.SocketOffset.Z += HitShakeAmplitude * Shake * FMath::Cos(Time * 2.3f);
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
	UpdateCrouchTap(DeltaSeconds);
	UpdateMeleeHold(DeltaSeconds);
	UpdateDodge(DeltaSeconds);
	UpdateHitReactions(DeltaSeconds);
	UpdateLowHealthPostProcess();
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
	// Mid-zip it lets go of the line.
	if (IsZipping())
	{
		UE_LOG(LogCastle, Log, TEXT("%s: crouch cancels the zip"), *GetNameSafe(this));
		GrappleComponent->CancelZip();
		return;
	}
	// Moving toward a roof or landing edge, it goes over to the hang.
	if (ParkourComponent && !bIsSliding && !IsTraversing() && MoveInputMagnitude > 0.f
		&& ParkourComponent->TryDropToHang(LastMoveWorldDirection, TEXT("crouch toward the edge")))
	{
		bCrouchTapPending = false;
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
	else if (bIsSprinting)
	{
		if (!StartSlide())
		{
			Crouch();
		}
	}
	else if (MoveInputMagnitude > 0.f)
	{
		// Moving, not sprinting: a tap dodges and a hold crouches, so wait for the release.
		bCrouchTapPending = true;
		CrouchTapHeldSeconds = 0.f;
		return;
	}
	else
	{
		Crouch();
	}
	UpdateMaxWalkSpeed();
}

void ACastleCharacter::Input_CrouchReleased(const FInputActionValue& /*Value*/)
{
	if (!bCrouchTapPending)
	{
		return;
	}
	bCrouchTapPending = false;
	TryDodge(LastMoveWorldDirection);
}

void ACastleCharacter::UpdateCrouchTap(float DeltaSeconds)
{
	if (!bCrouchTapPending)
	{
		return;
	}
	CrouchTapHeldSeconds += DeltaSeconds;
	if (CrouchTapHeldSeconds < DodgeTapSeconds)
	{
		return;
	}
	bCrouchTapPending = false;
	if (!bIsCrouched && !bIsSliding && !IsZipping() && !IsTraversing())
	{
		Crouch();
		UpdateMaxWalkSpeed();
	}
}

// --- Melee ------------------------------------------------------------------------------------

bool ACastleCharacter::IsMeleeAttacking() const
{
	return MeleeComponent && MeleeComponent->IsAttacking();
}

void ACastleCharacter::Input_MeleePressed(const FInputActionValue& /*Value*/)
{
	bMeleeHeld = true;
	MeleeHeldSeconds = 0.f;
}

void ACastleCharacter::Input_MeleeReleased(const FInputActionValue& /*Value*/)
{
	if (!bMeleeHeld)
	{
		return;
	}
	bMeleeHeld = false;
	StartLightAttack();
}

void ACastleCharacter::UpdateMeleeHold(float DeltaSeconds)
{
	if (!bMeleeHeld)
	{
		return;
	}
	MeleeHeldSeconds += DeltaSeconds;
	if (MeleeHeldSeconds >= HeavyHoldSeconds)
	{
		bMeleeHeld = false;
		StartHeavyAttack();
	}
}

bool ACastleCharacter::StartLightAttack()
{
	return StartMelee(LightAttack);
}

bool ACastleCharacter::StartHeavyAttack()
{
	return StartMelee(HeavyAttack);
}

bool ACastleCharacter::StartMelee(const FCastleMeleeAttack& Attack)
{
	if (!MeleeComponent || MeleeComponent->IsAttacking() || IsLockedOutByTakedown() || IsZipping()
		|| IsTraversing() || IsDodging() || IsStaggered() || IsDrawingBow())
	{
		return false;
	}

	// Face the thug she means to hit; with nobody close, the way the camera looks.
	FVector Facing = Controller ? FRotator(0.f, Controller->GetControlRotation().Yaw, 0.f).Vector() : GetActorForwardVector();
	if (const AActor* Target = FindSoftLockTarget())
	{
		Facing = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	}
	if (!Facing.IsNearlyZero())
	{
		SetActorRotation(FRotator(0.f, Facing.Rotation().Yaw, 0.f));
	}

	EndSlide();
	return MeleeComponent->StartAttack(Attack);
}

AActor* ACastleCharacter::FindSoftLockTarget() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	const FVector Forward = Controller
		? FRotator(0.f, Controller->GetControlRotation().Yaw, 0.f).Vector() : GetActorForwardVector();
	const float MinDot = FMath::Cos(FMath::DegreesToRadians(SoftLockAngleDegrees));
	AActor* Best = nullptr;
	float BestDistance = SoftLockRange;
	for (TActorIterator<AThugCharacter> It(World); It; ++It)
	{
		const UHealthComponent* Health = It->GetHealthComponent();
		if (!Health || !Health->IsAlive())
		{
			continue;
		}
		const FVector To = It->GetActorLocation() - GetActorLocation();
		const float Distance = To.Size2D();
		if (Distance < BestDistance && FVector::DotProduct(To.GetSafeNormal2D(), Forward) >= MinDot
			&& FMath::Abs(To.Z) < 200.f)
		{
			Best = *It;
			BestDistance = Distance;
		}
	}
	return Best;
}

void ACastleCharacter::HandleMeleeLanded(AActor* /*HitActor*/, float /*DamageDealt*/, FName /*AttackName*/)
{
	// Hit stop: the world nearly stops for two frames so the contact reads. The timer runs in
	// dilated time, hence the multiply.
	UWorld* World = GetWorld();
	if (!World || HitStopSeconds <= 0.f)
	{
		return;
	}
	UGameplayStatics::SetGlobalTimeDilation(World, HitStopTimeDilation);
	World->GetTimerManager().SetTimer(HitStopTimerHandle, this, &ACastleCharacter::EndHitStop,
		HitStopSeconds * HitStopTimeDilation, false);
}

void ACastleCharacter::EndHitStop()
{
	if (UWorld* World = GetWorld())
	{
		UGameplayStatics::SetGlobalTimeDilation(World, 1.f);
	}
}

// --- Dodge ------------------------------------------------------------------------------------

bool ACastleCharacter::TryDodge(FVector WorldDirection)
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	const FVector Direction = WorldDirection.GetSafeNormal2D();
	if (!Movement || Direction.IsNearlyZero() || bIsSprinting || IsDodging() || DodgeCooldownRemaining > 0.f
		|| IsLockedOutByTakedown() || IsZipping() || IsTraversing() || IsStaggered())
	{
		return false;
	}
	// On the ground only; a test world's character has no floor, so falling is the one refusal.
	if (Movement->IsFalling() || Movement->IsFlying())
	{
		return false;
	}

	if (MeleeComponent)
	{
		MeleeComponent->CancelAttack();
	}
	StopAim();
	if (bIsCrouched)
	{
		UnCrouch();
	}

	DodgeRemaining = DodgeSeconds;
	DodgeInvulnerableRemaining = DodgeInvulnerableSeconds;
	DodgeCooldownRemaining = DodgeCooldownSeconds;
	if (HealthComponent && !HealthComponent->IsInvulnerable() && DodgeInvulnerableSeconds > 0.f)
	{
		HealthComponent->SetInvulnerable(true);
		bDodgeOwnsInvulnerability = true;
	}

	// A root motion force: it overrides the gait speed the sample's graph writes, so the dash
	// covers exactly DodgeDistance, and it stops dead at the end instead of sliding on.
	TSharedPtr<FRootMotionSource_ConstantForce> Dash = MakeShared<FRootMotionSource_ConstantForce>();
	Dash->InstanceName = FName(TEXT("Dodge"));
	Dash->AccumulateMode = ERootMotionAccumulateMode::Override;
	Dash->Priority = 5;
	Dash->Force = Direction * (DodgeDistance / DodgeSeconds);
	Dash->Duration = DodgeSeconds;
	Dash->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::ClampVelocity;
	Dash->FinishVelocityParams.ClampVelocity = WalkSpeed;
	Movement->ApplyRootMotionSource(Dash);

	UE_LOG(LogCastle, Log, TEXT("%s: dodge %.0f cm toward %s, invulnerable %.2f s."), *GetNameSafe(this),
		DodgeDistance, *Direction.ToCompactString(), DodgeInvulnerableSeconds);
	return true;
}

void ACastleCharacter::UpdateDodge(float DeltaSeconds)
{
	DodgeCooldownRemaining = FMath::Max(0.f, DodgeCooldownRemaining - DeltaSeconds);
	DodgeRemaining = FMath::Max(0.f, DodgeRemaining - DeltaSeconds);
	if (DodgeInvulnerableRemaining <= 0.f)
	{
		return;
	}
	DodgeInvulnerableRemaining = FMath::Max(0.f, DodgeInvulnerableRemaining - DeltaSeconds);
	if (DodgeInvulnerableRemaining <= 0.f && bDodgeOwnsInvulnerability)
	{
		bDodgeOwnsInvulnerability = false;
		if (HealthComponent)
		{
			HealthComponent->SetInvulnerable(false);
		}
	}
}

// --- Hit reactions ----------------------------------------------------------------------------

void ACastleCharacter::HandleStaggered(UHealthComponent* /*Health*/, AActor* DamageInstigator)
{
	if (!HealthComponent || !HealthComponent->IsAlive())
	{
		return;
	}
	StaggerRemaining = PlayerStaggerSeconds;
	bMeleeHeld = false;
	if (MeleeComponent)
	{
		MeleeComponent->CancelAttack();
	}
	if (IsDrawingBow())
	{
		BowComponent->CancelDraw();
	}
	FVector Away = DamageInstigator ? (GetActorLocation() - DamageInstigator->GetActorLocation()).GetSafeNormal2D()
		: -GetActorForwardVector();
	if (Away.IsNearlyZero())
	{
		Away = -GetActorForwardVector();
	}
	LaunchCharacter(Away * PlayerStaggerShove, true, false);
	UE_LOG(LogCastle, Log, TEXT("%s: staggered by %s, health %.1f."), *GetNameSafe(this),
		*GetNameSafe(DamageInstigator), HealthComponent->GetCurrentHealth());
}

void ACastleCharacter::HandleHealthChanged(UHealthComponent* /*Health*/, float /*NewHealth*/, float Delta, AActor* DamageInstigator)
{
	// Someone else hurting her shakes the camera; a fall does not (it has the landing dip).
	if (Delta < 0.f && DamageInstigator && DamageInstigator != this)
	{
		HitShakeRemaining = HitShakeSeconds;
	}
}

void ACastleCharacter::UpdateHitReactions(float DeltaSeconds)
{
	StaggerRemaining = FMath::Max(0.f, StaggerRemaining - DeltaSeconds);
	HitShakeRemaining = FMath::Max(0.f, HitShakeRemaining - DeltaSeconds);
}

float ACastleCharacter::ComputeLowHealthAlpha(float HealthPercent) const
{
	if (LowHealthThreshold <= 0.f)
	{
		return 0.f;
	}
	return FMath::Clamp((LowHealthThreshold - HealthPercent) / LowHealthThreshold, 0.f, 1.f);
}

void ACastleCharacter::UpdateLowHealthPostProcess()
{
	if (!FollowCamera || !HealthComponent)
	{
		return;
	}
	const float Alpha = HealthComponent->IsAlive() ? ComputeLowHealthAlpha(HealthComponent->GetHealthPercent()) : 1.f;
	if (FMath::IsNearlyEqual(Alpha, LastLowHealthAlpha, 0.005f))
	{
		return;
	}
	LastLowHealthAlpha = Alpha;

	// Only override while it shows, so at full health the level's own post process is untouched.
	FPostProcessSettings& Settings = FollowCamera->PostProcessSettings;
	const bool bOn = Alpha > 0.f;
	const float Saturation = FMath::Lerp(1.f, LowHealthSaturation, Alpha);
	Settings.bOverride_ColorSaturation = bOn;
	Settings.ColorSaturation = FVector4(Saturation, Saturation, Saturation, 1.f);
	Settings.bOverride_VignetteIntensity = bOn;
	Settings.VignetteIntensity = FMath::Lerp(0.4f, LowHealthVignette, Alpha);
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
		// The jump key lets go of the line; the catch rule still applies on the way down.
		UE_LOG(LogCastle, Log, TEXT("%s: jump cancels the zip"), *GetNameSafe(this));
		GrappleComponent->CancelZip();
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
		// Nothing ahead, an edge behind: down to the hang on it.
		if (ParkourComponent->TryDropToHang(-GetActorForwardVector(), TEXT("jump facing away from the edge")))
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
		// Not rising: stepped off an edge, let go of a hang, or let go of a zip.
		bControlledDrop = Movement->Velocity.Z <= 0.f
			&& (PrevMovementMode == MOVE_Walking || PrevMovementMode == MOVE_NavWalking || PrevMovementMode == MOVE_Flying);
		if (bControlledDrop)
		{
			UE_LOG(LogCastle, Log, TEXT("%s: controlled drop from %s at feet %.0f"), *GetNameSafe(this),
				PrevMovementMode == MOVE_Flying ? TEXT("a hang or a zip") : TEXT("an edge"),
				GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		}
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
	else if (bControlledDrop && FallHeight > ControlledDropDipHeight)
	{
		LandingRecoverRemaining = LandingRecoverSeconds;
		UE_LOG(LogCastle, Log, TEXT("%s: controlled drop landed from %.0f cm: landing dip"), *GetNameSafe(this), FallHeight);
	}
	bControlledDrop = false;

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

void ACastleCharacter::Input_FirePressed(const FInputActionValue& /*Value*/)
{
	if (IsLockedOutByTakedown() || IsMeleeAttacking() || IsDodging() || IsStaggered())
	{
		return;
	}

	// With a bow, left click draws. Without one it is Hands: a punch, which is quiet, the stealth
	// option that does not bring the block down on you.
	const bool bHasBow = InventoryComponent && InventoryComponent->HasBow();
	if (bHasBow && BowComponent)
	{
		if (!IsZipping() && !IsTraversing())
		{
			BowComponent->StartDraw();
		}
		return;
	}

	if (UWeaponComponent* Weapon = GetWeaponComponent())
	{
		Weapon->Fire();
	}
}

void ACastleCharacter::Input_FireReleased(const FInputActionValue& /*Value*/)
{
	if (BowComponent && BowComponent->IsDrawing())
	{
		BowComponent->ReleaseDraw();
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

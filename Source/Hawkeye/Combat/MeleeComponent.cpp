// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/MeleeComponent.h"

#include "Hawkeye.h"
#include "Audio/HawkeyeAudioSubsystem.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Combat/AnimNotifyState_ComboWindow.h"
#include "Combat/AnimNotifyState_HitWindow.h"
#include "Combat/CombatAnimPlayback.h"
#include "Combat/HealthComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "MotionWarpingComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "World/ThugCharacter.h"

UMeleeComponent::UMeleeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UMeleeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AdvanceAttack(DeltaTime);
}

bool UMeleeComponent::CanStartAttack() const
{
	return Phase == EMeleePhase::Idle || (bComboWindowOpen && Phase == EMeleePhase::Recover);
}

bool UMeleeComponent::StartAttack(const FHawkeyeMeleeAttack& Attack)
{
	if (!CanStartAttack())
	{
		return false;
	}

	// Chaining out of a combo window: the old clip's windows are forgotten, and the clip itself is cut
	// by the new one (or blended out below if this swing has none).
	UAnimMontage* OldMontage = CurrentMontage;
	const TWeakObjectPtr<UAnimInstance> OldInstance = MontageInstance;
	ResetClipState();
	++SwingSerial;

	CurrentAttack = Attack;
	SwingDirection = PendingDirection;
	PendingDirection = FVector::ZeroVector;
	SwingTarget = PendingTarget;
	PendingTarget = nullptr;
	Phase = EMeleePhase::Windup;
	PhaseRemaining = Attack.WindupSeconds;

	UE_LOG(LogHawkeye, Log, TEXT("%s: %s swing winds up (%.2f s, %.0f damage)."),
		*GetNameSafe(GetOwner()), *Attack.Name.ToString(), Attack.WindupSeconds, Attack.Damage);
	if (Attack.WindupSeconds >= TelegraphMinWindup && GetOwner())
	{
		UHawkeyeAudioSubsystem::PlayAt(this, WindupSound, GetOwner()->GetActorLocation(), TEXT("melee telegraph"));
	}

	OnAttackWindup.Broadcast(Attack.Name, Attack.WindupSeconds);
	const bool bClip = TryPlaySwingClip(Attack);
	if (!bClip && OldMontage)
	{
		HawkeyeCombatAnim::Stop(OldInstance.Get(), OldMontage, 0.15f);
	}
	if (!bClipMovesOwner)
	{
		ApplyLunge(Attack);
	}

	// A zero wind-up lands on the frame it starts, not a tick later.
	if (!bHitFromNotify && PhaseRemaining <= 0.f)
	{
		AdvanceAttack(0.f);
	}
	return true;
}

bool UMeleeComponent::TryPlaySwingClip(const FHawkeyeMeleeAttack& Attack)
{
	if (bForceNotifyTimingForTest)
	{
		bForceNotifyTimingForTest = false;
		bHitFromNotify = true;
		bComboFromNotify = true;
		ClipTimeRemaining = 5.f;
		return true;
	}
	UAnimMontage* Montage = UCombatAnimSet::Resolve(AnimSet, Attack.AnimRole);
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	if (!Montage || !Mesh)
	{
		return false;
	}

	float HitStart = 0.f;
	float HitEnd = 0.f;
	float ComboStart = 0.f;
	float ComboEnd = 0.f;
	const bool bHasHit = HawkeyeCombatAnim::FindNotifyWindow(Montage, UAnimNotifyState_HitWindow::StaticClass(), HitStart, HitEnd);
	const bool bHasCombo = HawkeyeCombatAnim::FindNotifyWindow(Montage, UAnimNotifyState_ComboWindow::StaticClass(), ComboStart,
		ComboEnd);
	const float Rate = (bFitClipToWindup && bHasHit) ? HawkeyeCombatAnim::FitRate(HitStart, Attack.WindupSeconds) : 1.f;

	CurrentMontage = Montage;
	bClipMovesOwner = HawkeyeCombatAnim::DrivesRootMotion(Mesh, Montage);
	UpdateWarpTarget();
	UAnimInstance* Instance = HawkeyeCombatAnim::Play(Mesh, Montage, Rate);
	if (!Instance)
	{
		ResetClipState();
		UE_LOG(LogHawkeye, Verbose, TEXT("%s: %s clip %s did not play; procedural swing."), *GetNameSafe(GetOwner()),
			*Attack.Name.ToString(), *GetNameSafe(Montage));
		return false;
	}
	MontageInstance = Instance;
	bHitFromNotify = bHasHit;
	bComboFromNotify = bHasCombo;
	ClipTimeRemaining = Montage->GetPlayLength() / Rate + ClipTimeoutPadding;

	FOnMontageEnded Ended = FOnMontageEnded::CreateWeakLambda(this,
		[this, Serial = SwingSerial](UAnimMontage*, bool bInterrupted)
		{
			if (Serial == SwingSerial)
			{
				NotifyMontageEnded(bInterrupted);
			}
		});
	Instance->Montage_SetEndDelegate(Ended, Montage);
	UE_LOG(LogHawkeye, Log, TEXT("%s: %s plays %s at %.2fx (hit %s, combo %s, %s)."), *GetNameSafe(GetOwner()),
		*Attack.Name.ToString(), *Montage->GetName(), Rate, bHasHit ? TEXT("from the clip") : TEXT("on the timer"),
		bHasCombo ? TEXT("from the clip") : TEXT("on the timer"), bClipMovesOwner ? TEXT("root motion") : TEXT("lunge"));
	return true;
}

UMotionWarpingComponent* UMeleeComponent::FindOrAddMotionWarping() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}
	if (UMotionWarpingComponent* Existing = Owner->FindComponentByClass<UMotionWarpingComponent>())
	{
		return Existing;
	}
	// The sample's character already has one (its traversal warps); a second would warp every clip twice.
	UMotionWarpingComponent* Created = NewObject<UMotionWarpingComponent>(Owner, TEXT("CombatMotionWarping"));
	Created->RegisterComponent();
	return Created;
}

void UMeleeComponent::UpdateWarpTarget() const
{
	const AActor* Owner = GetOwner();
	if (!bClipMovesOwner || !Owner)
	{
		return;
	}
	UMotionWarpingComponent* Warp = FindOrAddMotionWarping();
	if (!Warp)
	{
		return;
	}
	const AActor* Target = SwingTarget.Get();
	if (!Target)
	{
		Warp->RemoveWarpTarget(WarpTargetName);
		return;
	}
	FVector Toward = (Target->GetActorLocation() - Owner->GetActorLocation()).GetSafeNormal2D();
	if (Toward.IsNearlyZero())
	{
		Toward = GetSwingDirection();
	}
	FVector Stand = Target->GetActorLocation() - Toward * WarpStandOffDistance;
	Stand.Z = Owner->GetActorLocation().Z;
	Warp->AddOrUpdateWarpTargetFromLocationAndRotation(WarpTargetName, Stand, Toward.Rotation());
}

void UMeleeComponent::ResetClipState()
{
	CurrentMontage = nullptr;
	MontageInstance = nullptr;
	bHitFromNotify = false;
	bComboFromNotify = false;
	bHitWindowOpen = false;
	bComboWindowOpen = false;
	bSwingResolved = false;
	bClipMovesOwner = false;
	ClipTimeRemaining = 0.f;
}

bool UMeleeComponent::IsCurrentClip(const UAnimSequenceBase* Animation) const
{
	return !CurrentMontage || !Animation || Animation == CurrentMontage;
}

void UMeleeComponent::NotifyHitWindowBegin(const UAnimSequenceBase* Animation)
{
	if (!bHitFromNotify || Phase != EMeleePhase::Windup || !IsCurrentClip(Animation))
	{
		return;
	}
	Phase = EMeleePhase::Recover;
	PhaseRemaining = CurrentAttack.RecoverSeconds;
	bHitWindowOpen = true;
	if (const AActor* Owner = GetOwner())
	{
		UHawkeyeAudioSubsystem::PlayAt(this, SwingSound, Owner->GetActorLocation(), TEXT("melee swing"));
	}
	bSwingResolved = ResolveSweep();
}

void UMeleeComponent::NotifyHitWindowEnd(const UAnimSequenceBase* Animation)
{
	if (!bHitWindowOpen || !IsCurrentClip(Animation))
	{
		return;
	}
	bHitWindowOpen = false;
	if (!bSwingResolved)
	{
		bSwingResolved = true;
		UE_LOG(LogHawkeye, Verbose, TEXT("%s: %s swing hit nothing."), *GetNameSafe(GetOwner()), *CurrentAttack.Name.ToString());
		OnAttackMissed.Broadcast(CurrentAttack.Name);
	}
}

void UMeleeComponent::NotifyComboWindowBegin(const UAnimSequenceBase* Animation)
{
	if (!bComboFromNotify || Phase == EMeleePhase::Idle || bComboWindowOpen || !IsCurrentClip(Animation))
	{
		return;
	}
	// A window that opens before the hit window closed settles the hit first: the chain needs to know.
	if (bHitWindowOpen)
	{
		NotifyHitWindowEnd(Animation);
	}
	if (Phase == EMeleePhase::Windup)
	{
		return;
	}
	bComboWindowOpen = true;
	OnComboWindowChanged.Broadcast(true);
}

void UMeleeComponent::NotifyComboWindowEnd(const UAnimSequenceBase* Animation)
{
	if (IsCurrentClip(Animation))
	{
		CloseComboWindow();
	}
}

void UMeleeComponent::CloseComboWindow()
{
	if (!bComboWindowOpen)
	{
		return;
	}
	bComboWindowOpen = false;
	OnComboWindowChanged.Broadcast(false);
}

void UMeleeComponent::NotifyMontageEnded(bool bInterrupted)
{
	if (Phase == EMeleePhase::Idle)
	{
		ResetClipState();
		return;
	}
	if (bHitFromNotify || bComboFromNotify)
	{
		EndClipSwing(bInterrupted);
		return;
	}
	CurrentMontage = nullptr;
	MontageInstance = nullptr;
}

void UMeleeComponent::EndClipSwing(bool bInterrupted)
{
	if (Phase == EMeleePhase::Windup && bHitFromNotify)
	{
		UE_LOG(LogHawkeye, Log, TEXT("%s: %s clip ended before its hit window (%s)."), *GetNameSafe(GetOwner()),
			*CurrentAttack.Name.ToString(), bInterrupted ? TEXT("cut") : TEXT("no window reached"));
	}
	if (bHitWindowOpen)
	{
		NotifyHitWindowEnd(nullptr);
	}
	const bool bWasComboOpen = bComboWindowOpen;
	const bool bChainedByClip = bComboFromNotify;
	ResetClipState();
	Phase = EMeleePhase::Idle;
	PhaseRemaining = 0.f;
	// Listeners holding the chain open for this clip's window let go, whether or not it ever opened.
	if (bWasComboOpen || bChainedByClip)
	{
		OnComboWindowChanged.Broadcast(false);
	}
}

bool UMeleeComponent::AdvanceClipSwing(float DeltaSeconds)
{
	const bool bWindupFromClip = Phase == EMeleePhase::Windup && bHitFromNotify;
	const bool bRecoverFromClip = Phase == EMeleePhase::Recover && bComboFromNotify;
	if (bHitWindowOpen && !bSwingResolved)
	{
		bSwingResolved = ResolveSweep();
	}
	if (!bWindupFromClip && !bRecoverFromClip)
	{
		return false;
	}
	ClipTimeRemaining -= FMath::Max(DeltaSeconds, 0.f);
	if (ClipTimeRemaining <= 0.f)
	{
		UE_LOG(LogHawkeye, Warning, TEXT("%s: %s clip %s never closed its windows; the swing ends."), *GetNameSafe(GetOwner()),
			*CurrentAttack.Name.ToString(), *GetNameSafe(CurrentMontage));
		HawkeyeCombatAnim::Stop(MontageInstance.Get(), CurrentMontage, 0.15f);
		EndClipSwing(true);
	}
	return true;
}

void UMeleeComponent::CancelAttack()
{
	if (Phase == EMeleePhase::Windup)
	{
		UE_LOG(LogHawkeye, Log, TEXT("%s: %s swing interrupted."), *GetNameSafe(GetOwner()), *CurrentAttack.Name.ToString());
	}
	if (CurrentMontage)
	{
		HawkeyeCombatAnim::Stop(MontageInstance.Get(), CurrentMontage, 0.15f);
	}
	const bool bChainedByClip = bComboFromNotify && Phase != EMeleePhase::Idle;
	const bool bWasComboOpen = bComboWindowOpen;
	ResetClipState();
	++SwingSerial;
	Phase = EMeleePhase::Idle;
	PhaseRemaining = 0.f;
	if (bWasComboOpen || bChainedByClip)
	{
		OnComboWindowChanged.Broadcast(false);
	}
}

void UMeleeComponent::AdvanceAttack(float DeltaSeconds)
{
	if (Phase == EMeleePhase::Idle)
	{
		return;
	}
	if (AdvanceClipSwing(DeltaSeconds))
	{
		return;
	}

	PhaseRemaining -= DeltaSeconds;
	if (Phase == EMeleePhase::Windup && PhaseRemaining <= KINDA_SMALL_NUMBER)
	{
		// Whatever the wind-up overshot comes off the recovery, so a big step lands on time.
		const float Overshoot = -FMath::Min(PhaseRemaining, 0.f);
		Phase = EMeleePhase::Recover;
		PhaseRemaining = CurrentAttack.RecoverSeconds - Overshoot;
		Strike();
	}
	if (Phase == EMeleePhase::Recover && PhaseRemaining <= KINDA_SMALL_NUMBER)
	{
		Phase = EMeleePhase::Idle;
		PhaseRemaining = 0.f;
	}
}

FVector UMeleeComponent::GetSwingDirection() const
{
	if (!SwingDirection.IsNearlyZero())
	{
		return SwingDirection;
	}
	return GetOwner() ? GetOwner()->GetActorForwardVector().GetSafeNormal2D() : FVector::ForwardVector;
}

AActor* UMeleeComponent::FindTarget(const FHawkeyeMeleeAttack& Attack) const
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return nullptr;
	}

	const FVector Start = Owner->GetActorLocation();
	const FVector End = Start + GetSwingDirection() * Attack.Range;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(HawkeyeMelee), /*bTraceComplex=*/false, Owner);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Pawn);

	TArray<FHitResult> Hits;
	World->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity, Objects,
		FCollisionShape::MakeSphere(Attack.Radius), Params);

	for (const FHitResult& Hit : Hits)
	{
		AActor* Candidate = Hit.GetActor();
		if (!Candidate || Candidate == Owner || (!IgnoreTag.IsNone() && Candidate->ActorHasTag(IgnoreTag)))
		{
			continue;
		}
		const UHealthComponent* Health = Candidate->FindComponentByClass<UHealthComponent>();
		if (Health && Health->IsAlive())
		{
			return Candidate;
		}
	}
	return nullptr;
}

void UMeleeComponent::Strike()
{
	AActor* Owner = GetOwner();
	if (Owner)
	{
		UHawkeyeAudioSubsystem::PlayAt(this, SwingSound, Owner->GetActorLocation(), TEXT("melee swing"));
	}
	if (!ResolveSweep())
	{
		UE_LOG(LogHawkeye, Verbose, TEXT("%s: %s swing hit nothing."), *GetNameSafe(Owner), *CurrentAttack.Name.ToString());
		OnAttackMissed.Broadcast(CurrentAttack.Name);
	}
}

bool UMeleeComponent::ResolveSweep()
{
	AActor* Owner = GetOwner();
	AActor* Target = FindTarget(CurrentAttack);
	UHealthComponent* Health = Target ? Target->FindComponentByClass<UHealthComponent>() : nullptr;
	if (!Health)
	{
		return false;
	}

	// A knockdown replaces the stagger: the body going over is the reaction. The heavy's shield takes a
	// light strike from the front outright; a heavy gets through it (and breaks his guard instead).
	AThugCharacter* Thug = Cast<AThugCharacter>(Target);
	if (Thug && Owner && !CurrentAttack.bKnockdown
		&& Thug->TryBlock(Owner, Owner->GetActorLocation() - Thug->GetActorLocation(), CurrentAttack.Name.ToString()))
	{
		++BlockedCount;
		OnAttackMissed.Broadcast(CurrentAttack.Name);
		return true;
	}
	const bool bKnockdown = CurrentAttack.bKnockdown && Thug;
	const float Dealt = Health->ApplyMeleeDamage(CurrentAttack.Damage, Owner, CurrentAttack.bStagger && !bKnockdown);
	if (Dealt <= 0.f)
	{
		UE_LOG(LogHawkeye, Log, TEXT("%s: %s swing on %s did nothing (invulnerable or dead)."),
			*GetNameSafe(Owner), *CurrentAttack.Name.ToString(), *GetNameSafe(Target));
		OnAttackMissed.Broadcast(CurrentAttack.Name);
		return true;
	}

	if (bKnockdown && Health->IsAlive())
	{
		Thug->Knockdown(Owner);
	}
	if (CurrentAttack.KnockbackDistance > 0.f && Health->IsAlive())
	{
		ApplyKnockback(Target, CurrentAttack);
	}

	const bool bHeavySound = CurrentAttack.bKnockdown && !HeavyHitSound.IsNull();
	UHawkeyeAudioSubsystem::PlayAt(this, bHeavySound ? HeavyHitSound : HitSound, Target->GetActorLocation(),
		bHeavySound ? TEXT("melee heavy hit") : TEXT("melee hit"));
	UE_LOG(LogHawkeye, Log, TEXT("%s: %s swing hit %s for %.1f (health %.1f)."),
		*GetNameSafe(Owner), *CurrentAttack.Name.ToString(), *GetNameSafe(Target), Dealt, Health->GetCurrentHealth());
	OnAttackLanded.Broadcast(Target, Dealt, CurrentAttack.Name);
	return true;
}

void UMeleeComponent::ApplyLunge(const FHawkeyeMeleeAttack& Attack) const
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Movement || Attack.LungeDistance <= 0.f || !Movement->IsMovingOnGround() || !GetWorld())
	{
		return;
	}

	// A root motion force rather than a launch: it overrides the gait speed the sample's graph
	// writes, and ends exactly LungeSeconds later without sliding on.
	TSharedPtr<FRootMotionSource_ConstantForce> Lunge = MakeShared<FRootMotionSource_ConstantForce>();
	Lunge->InstanceName = FName(TEXT("MeleeLunge"));
	Lunge->AccumulateMode = ERootMotionAccumulateMode::Override;
	Lunge->Priority = 4;
	Lunge->Force = GetSwingDirection() * (Attack.LungeDistance / Attack.LungeSeconds);
	Lunge->Duration = Attack.LungeSeconds;
	Lunge->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::SetVelocity;
	Lunge->FinishVelocityParams.SetVelocity = FVector::ZeroVector;
	Movement->ApplyRootMotionSource(Lunge);
}

FVector UMeleeComponent::ComputeKnockbackForce(const FVector& From, const FVector& To, float Distance, float Seconds)
{
	FVector Away = (To - From).GetSafeNormal2D();
	if (Away.IsNearlyZero())
	{
		Away = FVector::ForwardVector;
	}
	return Away * (FMath::Max(Distance, 0.f) / FMath::Max(Seconds, 0.01f));
}

void UMeleeComponent::ApplyKnockback(AActor* Target, const FHawkeyeMeleeAttack& Attack) const
{
	const AActor* Owner = GetOwner();
	ACharacter* Character = Cast<ACharacter>(Target);
	UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Owner || !Movement || Attack.KnockbackDistance <= 0.f)
	{
		return;
	}
	// A root motion force, like the lunge and the dodge: it beats whatever gait the target's graph asks
	// for, covers exactly the distance, and stops dead at the end.
	TSharedPtr<FRootMotionSource_ConstantForce> Shove = MakeShared<FRootMotionSource_ConstantForce>();
	Shove->InstanceName = FName(TEXT("MeleeKnockback"));
	Shove->AccumulateMode = ERootMotionAccumulateMode::Override;
	Shove->Priority = 6;
	Shove->Force = ComputeKnockbackForce(Owner->GetActorLocation(), Target->GetActorLocation(), Attack.KnockbackDistance,
		Attack.KnockbackSeconds);
	Shove->Duration = Attack.KnockbackSeconds;
	Shove->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::SetVelocity;
	Shove->FinishVelocityParams.SetVelocity = FVector::ZeroVector;
	Movement->ApplyRootMotionSource(Shove);
	UE_LOG(LogHawkeye, Log, TEXT("%s: %s knocks %s back %.0f cm."), *GetNameSafe(Owner), *Attack.Name.ToString(),
		*GetNameSafe(Target), Attack.KnockbackDistance);
}

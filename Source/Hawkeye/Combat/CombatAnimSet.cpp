// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/CombatAnimSet.h"

#include "Animation/AnimMontage.h"
#include "Hawkeye.h"

const TSoftObjectPtr<UAnimMontage>* UCombatAnimSet::FindSlot(ECombatAnimRole Role) const
{
	switch (Role)
	{
	case ECombatAnimRole::Light1: return &Light1;
	case ECombatAnimRole::Light2: return &Light2;
	case ECombatAnimRole::Light3: return &Light3;
	case ECombatAnimRole::Heavy: return &Heavy;
	case ECombatAnimRole::Kick: return &Kick;
	case ECombatAnimRole::Parry: return &Parry;
	case ECombatAnimRole::DodgeForward: return &DodgeForward;
	case ECombatAnimRole::DodgeBack: return &DodgeBack;
	case ECombatAnimRole::DodgeLeft: return &DodgeLeft;
	case ECombatAnimRole::DodgeRight: return &DodgeRight;
	case ECombatAnimRole::HitFront: return &HitFront;
	case ECombatAnimRole::HitBack: return &HitBack;
	case ECombatAnimRole::HitLeft: return &HitLeft;
	case ECombatAnimRole::HitRight: return &HitRight;
	case ECombatAnimRole::Knockdown: return &Knockdown;
	case ECombatAnimRole::GetUp: return &GetUp;
	case ECombatAnimRole::FinisherAttacker: return &FinisherAttacker;
	case ECombatAnimRole::FinisherVictim: return &FinisherVictim;
	case ECombatAnimRole::FinisherBow: return &FinisherBow;
	case ECombatAnimRole::BowDraw: return &BowDraw;
	case ECombatAnimRole::BowAimIdle: return &BowAimIdle;
	case ECombatAnimRole::BowFire: return &BowFire;
	case ECombatAnimRole::BowNock: return &BowNock;
	default: return nullptr;
	}
}

TSoftObjectPtr<UAnimMontage> UCombatAnimSet::GetMontage(ECombatAnimRole Role) const
{
	const TSoftObjectPtr<UAnimMontage>* Slot = FindSlot(Role);
	if (Slot && !Slot->IsNull())
	{
		return *Slot;
	}
	// The chain's ender reads as well as a kick as a third punch, so a set with only a kick uses it.
	if (Role == ECombatAnimRole::Light3)
	{
		return Kick;
	}
	return TSoftObjectPtr<UAnimMontage>();
}

UAnimMontage* UCombatAnimSet::ResolveMontage(ECombatAnimRole Role) const
{
	const TSoftObjectPtr<UAnimMontage> Soft = GetMontage(Role);
	if (Soft.IsNull())
	{
		return nullptr;
	}
	UAnimMontage* Montage = Soft.LoadSynchronous();
	if (!Montage)
	{
		UE_LOG(LogHawkeye, Warning, TEXT("%s: the %s montage %s does not load; using the procedural fallback."), *GetName(),
			*UEnum::GetValueAsString(Role), *Soft.ToString());
	}
	return Montage;
}

UAnimMontage* UCombatAnimSet::Resolve(const UCombatAnimSet* Set, ECombatAnimRole Role)
{
	return Set ? Set->ResolveMontage(Role) : nullptr;
}

int32 UCombatAnimSet::CountAssigned() const
{
	int32 Count = 0;
	for (uint8 Value = static_cast<uint8>(ECombatAnimRole::Light1); Value <= static_cast<uint8>(ECombatAnimRole::BowNock); ++Value)
	{
		const TSoftObjectPtr<UAnimMontage>* Slot = FindSlot(static_cast<ECombatAnimRole>(Value));
		Count += (Slot && !Slot->IsNull()) ? 1 : 0;
	}
	return Count;
}

void UCombatAnimSet::Preload()
{
	Preloaded.Reset();
	for (uint8 Value = static_cast<uint8>(ECombatAnimRole::Light1); Value <= static_cast<uint8>(ECombatAnimRole::BowNock); ++Value)
	{
		const TSoftObjectPtr<UAnimMontage>* Slot = FindSlot(static_cast<ECombatAnimRole>(Value));
		if (Slot && !Slot->IsNull())
		{
			if (UAnimMontage* Montage = Slot->LoadSynchronous())
			{
				Preloaded.AddUnique(Montage);
			}
		}
	}
	UE_LOG(LogHawkeye, Log, TEXT("%s: %d combat montages loaded of %d assigned."), *GetName(), Preloaded.Num(), CountAssigned());
}

ECombatAnimRole UCombatAnimSet::HitRoleFor(EHawkeyeHitDirection Side)
{
	switch (Side)
	{
	case EHawkeyeHitDirection::Back: return ECombatAnimRole::HitBack;
	case EHawkeyeHitDirection::Left: return ECombatAnimRole::HitLeft;
	case EHawkeyeHitDirection::Right: return ECombatAnimRole::HitRight;
	default: return ECombatAnimRole::HitFront;
	}
}

ECombatAnimRole UCombatAnimSet::DodgeRoleFor(EHawkeyeHitDirection Side)
{
	switch (Side)
	{
	case EHawkeyeHitDirection::Back: return ECombatAnimRole::DodgeBack;
	case EHawkeyeHitDirection::Left: return ECombatAnimRole::DodgeLeft;
	case EHawkeyeHitDirection::Right: return ECombatAnimRole::DodgeRight;
	default: return ECombatAnimRole::DodgeForward;
	}
}

FPrimaryAssetId UCombatAnimSet::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("CombatAnimSet"), GetFName());
}

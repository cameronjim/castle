// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Combat/StrikePose.h"
#include "CombatAnimSet.generated.h"

class UAnimMontage;

/** What a combat clip is for. A character's UCombatAnimSet has one montage slot per role. */
UENUM(BlueprintType)
enum class ECombatAnimRole : uint8
{
	None,
	/** The light chain: first, second, third (the ender). */
	Light1,
	Light2,
	Light3,
	/** Kate's held strike; a thug's bat, bash and slow swing. */
	Heavy,
	/** Stands in for Light3 when the set has no Light3 clip. */
	Kick,
	/** Kate's parry strike. */
	Parry,
	DodgeForward,
	DodgeBack,
	DodgeLeft,
	DodgeRight,
	/** Hit reactions, by the side the blow came from. */
	HitFront,
	HitBack,
	HitLeft,
	HitRight,
	/** Goes down and holds the last frame; GetUp stands him back up. */
	Knockdown,
	GetUp,
	FinisherAttacker,
	FinisherVictim,
	/** The finisher with the bow up: the sweep. */
	FinisherBow,
	/** Upper body: the draw, the held aim (loops), the release, the next arrow onto the string. */
	BowDraw,
	BowAimIdle,
	BowFire,
	BowNock
};

/**
 * One character's combat clips (claude-docs/gameplay-semantics.md, "Combat animation clips").
 * Filled by Tools/Editor/create_combat_anims.py from the AM_<Role>_<Variant> montages the import
 * script made; every slot may be empty, and an empty slot means the procedural fallback.
 */
UCLASS(BlueprintType)
class HAWKEYE_API UCombatAnimSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** The soft montage for Role, with the one data fallback (Light3 uses Kick when it has none). */
	TSoftObjectPtr<UAnimMontage> GetMontage(ECombatAnimRole Role) const;

	/** GetMontage(Role), loaded. Null for an empty slot or one that does not load. */
	UFUNCTION(BlueprintCallable, Category = "Combat Anim")
	UAnimMontage* ResolveMontage(ECombatAnimRole Role) const;

	/** Set->ResolveMontage(Role), or null with no set. */
	static UAnimMontage* Resolve(const UCombatAnimSet* Set, ECombatAnimRole Role);

	/** True when Role (after its fallback) names a montage, loaded or not. */
	UFUNCTION(BlueprintPure, Category = "Combat Anim")
	bool HasMontage(ECombatAnimRole Role) const { return !GetMontage(Role).IsNull(); }

	/** Roles that name a montage, not counting fallbacks. */
	UFUNCTION(BlueprintPure, Category = "Combat Anim")
	int32 CountAssigned() const;

	/** Loads every assigned montage now and keeps it, so the first strike does not hitch. */
	void Preload();

	/** The hit reaction for a blow from Side. */
	static ECombatAnimRole HitRoleFor(EHawkeyeHitDirection Side);

	/** The dodge for a dash toward Side of the one dodging (Front is a dash forward). */
	static ECombatAnimRole DodgeRoleFor(EHawkeyeHitDirection Side);

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Anim|Strikes")
	TSoftObjectPtr<UAnimMontage> Light1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Anim|Strikes")
	TSoftObjectPtr<UAnimMontage> Light2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Anim|Strikes")
	TSoftObjectPtr<UAnimMontage> Light3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Anim|Strikes")
	TSoftObjectPtr<UAnimMontage> Heavy;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Anim|Strikes")
	TSoftObjectPtr<UAnimMontage> Kick;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Anim|Strikes")
	TSoftObjectPtr<UAnimMontage> Parry;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Anim|Dodge")
	TSoftObjectPtr<UAnimMontage> DodgeForward;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Anim|Dodge")
	TSoftObjectPtr<UAnimMontage> DodgeBack;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Anim|Dodge")
	TSoftObjectPtr<UAnimMontage> DodgeLeft;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Anim|Dodge")
	TSoftObjectPtr<UAnimMontage> DodgeRight;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Anim|Hit")
	TSoftObjectPtr<UAnimMontage> HitFront;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Anim|Hit")
	TSoftObjectPtr<UAnimMontage> HitBack;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Anim|Hit")
	TSoftObjectPtr<UAnimMontage> HitLeft;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Anim|Hit")
	TSoftObjectPtr<UAnimMontage> HitRight;

	/** Should hold its last frame (the import script turns auto blend-out off for it). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Anim|Hit")
	TSoftObjectPtr<UAnimMontage> Knockdown;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Anim|Hit")
	TSoftObjectPtr<UAnimMontage> GetUp;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Anim|Finisher")
	TSoftObjectPtr<UAnimMontage> FinisherAttacker;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Anim|Finisher")
	TSoftObjectPtr<UAnimMontage> FinisherVictim;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Anim|Finisher")
	TSoftObjectPtr<UAnimMontage> FinisherBow;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Anim|Bow")
	TSoftObjectPtr<UAnimMontage> BowDraw;

	/** Should loop (the import script makes its one section loop). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Anim|Bow")
	TSoftObjectPtr<UAnimMontage> BowAimIdle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Anim|Bow")
	TSoftObjectPtr<UAnimMontage> BowFire;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Anim|Bow")
	TSoftObjectPtr<UAnimMontage> BowNock;

private:
	/** The slot for Role, no fallback. Null for None. */
	const TSoftObjectPtr<UAnimMontage>* FindSlot(ECombatAnimRole Role) const;

	/** What Preload loaded, held so the soft references stay resident. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAnimMontage>> Preloaded;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Combat/CombatAnimPlayback.h"
#include "Combat/CombatAnimSet.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Player/HawkeyeCharacter.h"
#include "Tests/AutomationCommon.h"
#include "World/ThugCharacter.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * The combat clips on the real characters in the district: whatever clips the anim sets carry
 * (none on a fresh clone, where every check is skipped with a note) play on the instance and slot
 * claude-docs/animation.md says: Kate's full-body clips on the sample's AnimBP, her bow clips on the
 * post-process graph's UpperBody slot, a thug's clips on the post-process graph's DefaultSlot, an
 * archer's bow clips on his UpperBody slot. It reads the slot weight after a few frames, which is
 * the graph actually blending the clip in, not only the montage being started.
 *
 * Like the other smoke tests it loads the map; the rule-level tests are in CombatAnimTest.cpp.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHawkeyeSmokeCombatClipsPlay, "Hawkeye.Smoke.CombatClipsPlay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace HawkeyeCombatClipSmoke
{
	struct FPlayed
	{
		FString Who;
		TWeakObjectPtr<UAnimInstance> Instance;
		TWeakObjectPtr<UAnimMontage> Montage;
		FName Slot;
		bool bExpectMain = false;
	};

	static TArray<FPlayed> Played;

	static UWorld* FindGameWorld()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.World() && (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE))
			{
				return Context.World();
			}
		}
		return nullptr;
	}

	/** Plays Role from Set on Mesh and remembers where, or notes that there is no clip for it. */
	static void Play(FAutomationTestBase& Test, const FString& Who, USkeletalMeshComponent* Mesh, const UCombatAnimSet* Set,
		ECombatAnimRole Role, bool bExpectMain)
	{
		UAnimMontage* Montage = UCombatAnimSet::Resolve(Set, Role);
		if (!Montage || !Mesh)
		{
			Test.AddInfo(FString::Printf(TEXT("%s has no %s clip; its fallback is covered by CombatAnimTest."), *Who,
				*UEnum::GetValueAsString(Role)));
			return;
		}
		// Headless nothing is rendered; make sure the pose still updates so the slot weight can be read.
		Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		UAnimInstance* Instance = HawkeyeCombatAnim::Play(Mesh, Montage);
		Test.TestNotNull(*FString::Printf(TEXT("%s plays %s"), *Who, *Montage->GetName()), Instance);
		if (!Instance)
		{
			return;
		}
		const bool bOnMain = Instance == Mesh->GetAnimInstance();
		Test.TestEqual(*FString::Printf(TEXT("%s's %s plays on the %s instance"), *Who, *Montage->GetName(),
			bExpectMain ? TEXT("main") : TEXT("post-process")), bOnMain, bExpectMain);
		FPlayed Entry;
		Entry.Who = Who;
		Entry.Instance = Instance;
		Entry.Montage = Montage;
		Entry.Slot = Montage->SlotAnimTracks.Num() > 0 ? Montage->SlotAnimTracks[0].SlotName : NAME_None;
		Entry.bExpectMain = bExpectMain;
		Played.Add(Entry);
	}
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FHawkeyeStartCombatClips, FAutomationTestBase*, Test);

bool FHawkeyeStartCombatClips::Update()
{
	using namespace HawkeyeCombatClipSmoke;
	Played.Reset();
	UWorld* World = FindGameWorld();
	if (!World)
	{
		Test->AddError(TEXT("No game world after opening the district."));
		return true;
	}
	AHawkeyeCharacter* Kate = nullptr;
	for (TActorIterator<AHawkeyeCharacter> It(World); It; ++It)
	{
		if (It->IsPlayerControlled())
		{
			Kate = *It;
		}
	}
	if (Test->TestNotNull(TEXT("Kate is in the district"), Kate))
	{
		Test->TestNotNull(TEXT("She wears an anim set"), Kate->GetCombatAnimSet());
		Play(*Test, TEXT("Kate"), Kate->GetMesh(), Kate->GetCombatAnimSet(), ECombatAnimRole::HitFront, true);
		Play(*Test, TEXT("Kate"), Kate->GetMesh(), Kate->GetCombatAnimSet(), ECombatAnimRole::BowDraw, false);
	}
	bool bThug = false;
	bool bArcher = false;
	for (TActorIterator<AThugCharacter> It(World); It; ++It)
	{
		const bool bIsArcher = It->ActorHasTag(FName(TEXT("ArcherPair")));
		if (bIsArcher && !bArcher)
		{
			bArcher = true;
			Play(*Test, TEXT("An archer"), It->GetMesh(), It->CombatAnimSet, ECombatAnimRole::BowDraw, false);
		}
		else if (!bIsArcher && !bThug && !It->IsHeavy())
		{
			bThug = true;
			Play(*Test, TEXT("A thug"), It->GetMesh(), It->CombatAnimSet, ECombatAnimRole::HitFront, false);
		}
	}
	Test->TestTrue(TEXT("A thug and an archer were found"), bThug && bArcher);
	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FHawkeyeCheckCombatClips, FAutomationTestBase*, Test);

bool FHawkeyeCheckCombatClips::Update()
{
	using namespace HawkeyeCombatClipSmoke;
	for (const FPlayed& Entry : Played)
	{
		UAnimInstance* Instance = Entry.Instance.Get();
		UAnimMontage* Montage = Entry.Montage.Get();
		if (!Instance || !Montage)
		{
			Test->AddError(FString::Printf(TEXT("%s's clip went away."), *Entry.Who));
			continue;
		}
		const float Weight = Instance->GetSlotMontageGlobalWeight(Entry.Slot);
		Test->TestTrue(*FString::Printf(TEXT("%s's %s is blended in through the %s slot (weight %.2f)"), *Entry.Who,
			*Montage->GetName(), *Entry.Slot.ToString(), Weight), Weight > 0.f);
		HawkeyeCombatAnim::Stop(Instance, Montage, 0.f);
	}
	Played.Reset();
	return true;
}

bool FHawkeyeSmokeCombatClipsPlay::RunTest(const FString& Parameters)
{
	AutomationOpenMap(TEXT("/Game/Maps/L_District_EastVillage"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.f));
	ADD_LATENT_AUTOMATION_COMMAND(FHawkeyeStartCombatClips(this));
	// Past the 0.1 s blend in, inside the shortest clip (the 0.6 s hit reactions).
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.25f));
	ADD_LATENT_AUTOMATION_COMMAND(FHawkeyeCheckCombatClips(this));
	return true;
}

#endif

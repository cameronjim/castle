// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifyQueue.h"
#include "Combat/AnimNotifyState_ComboWindow.h"
#include "Combat/AnimNotifyState_HitWindow.h"
#include "Combat/BowComponent.h"
#include "Combat/BowDefinition.h"
#include "Combat/BowIKAnimInstance.h"
#include "Combat/CombatAnimPlayback.h"
#include "Combat/CombatAnimSet.h"
#include "Combat/HealthComponent.h"
#include "Combat/MeleeComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Misc/AutomationTest.h"
#include "Player/HawkeyeCharacter.h"
#include "Player/InventoryComponent.h"
#include "Tests/HawkeyeTestUtils.h"
#include "World/ThugCharacter.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Combat clips (claude-docs/gameplay-semantics.md, "Combat animation clips"): the anim set's roles and
 * its one fallback, the swing timed by ANS_HitWindow and ANS_ComboWindow, the procedural swing, dash,
 * reactions and bow kept exactly when no clip plays, and the bow IK anchored on a clip's grip. A test
 * world has no anim instance, so no montage ever plays here: the notify path is driven through the
 * notify classes themselves, which is what a playing montage would call.
 */
namespace HawkeyeCombatAnimTest
{
	static UAnimMontage* MakeMontage(FName Slot = HawkeyeCombatAnim::FullBodySlot)
	{
		UAnimMontage* Montage = NewObject<UAnimMontage>(GetTransientPackage());
		Montage->SlotAnimTracks.AddDefaulted();
		Montage->SlotAnimTracks[0].SlotName = Slot;
		return Montage;
	}

	static AHawkeyeAimTestCharacter* SpawnKate(const FHawkeyeTestWorld& TestWorld)
	{
		return Cast<AHawkeyeAimTestCharacter>(
			TestWorld.SpawnActor(AHawkeyeAimTestCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator));
	}

	static AThugCharacter* SpawnThug(const FHawkeyeTestWorld& TestWorld, const FVector& At)
	{
		AThugCharacter* Thug = Cast<AThugCharacter>(TestWorld.SpawnActor(AThugCharacter::StaticClass(), At,
			FRotator(0.f, 180.f, 0.f)));
		if (Thug)
		{
			Thug->Weapon = EThugWeapon::Fists;
		}
		return Thug;
	}

	/** A set with every strike, dodge, reaction and bow role filled by montages that can never play here. */
	static UCombatAnimSet* MakeFullSet()
	{
		UCombatAnimSet* Set = NewObject<UCombatAnimSet>(GetTransientPackage());
		for (TSoftObjectPtr<UAnimMontage>* Slot : { &Set->Light1, &Set->Light2, &Set->Light3, &Set->Heavy, &Set->Parry,
				&Set->DodgeForward, &Set->DodgeBack, &Set->DodgeLeft, &Set->DodgeRight, &Set->HitFront, &Set->HitBack,
				&Set->HitLeft, &Set->HitRight, &Set->Knockdown, &Set->GetUp, &Set->FinisherAttacker, &Set->FinisherVictim })
		{
			*Slot = MakeMontage();
		}
		for (TSoftObjectPtr<UAnimMontage>* Slot : { &Set->BowDraw, &Set->BowAimIdle, &Set->BowFire, &Set->BowNock })
		{
			*Slot = MakeMontage(HawkeyeCombatAnim::UpperBodySlot);
		}
		return Set;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHawkeyeCombatAnimSetResolution, "Hawkeye.CombatAnim.SetResolutionAndFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHawkeyeCombatAnimSetResolution::RunTest(const FString& Parameters)
{
	using namespace HawkeyeCombatAnimTest;

	UCombatAnimSet* Set = NewObject<UCombatAnimSet>(GetTransientPackage());
	TestEqual(TEXT("An empty set names nothing"), Set->CountAssigned(), 0);
	TestNull(TEXT("An empty role resolves to nothing"), Set->ResolveMontage(ECombatAnimRole::Light1));
	TestNull(TEXT("No set resolves to nothing"), UCombatAnimSet::Resolve(nullptr, ECombatAnimRole::Light1));
	TestNull(TEXT("None is never a clip"), Set->ResolveMontage(ECombatAnimRole::None));

	UAnimMontage* Jab = MakeMontage();
	UAnimMontage* Kick = MakeMontage();
	Set->Light1 = Jab;
	Set->Kick = Kick;
	TestTrue(TEXT("Light1 resolves to its montage"), Set->ResolveMontage(ECombatAnimRole::Light1) == Jab);
	TestNull(TEXT("Light2 is still empty: no borrowing from Light1"), Set->ResolveMontage(ECombatAnimRole::Light2));
	TestTrue(TEXT("Light3 with no clip of its own uses Kick"), Set->ResolveMontage(ECombatAnimRole::Light3) == Kick);
	TestTrue(TEXT("HasMontage sees the fallback"), Set->HasMontage(ECombatAnimRole::Light3));
	TestEqual(TEXT("Two slots assigned (the fallback is not a slot)"), Set->CountAssigned(), 2);
	UAnimMontage* Ender = MakeMontage();
	Set->Light3 = Ender;
	TestTrue(TEXT("Its own Light3 wins over Kick"), Set->ResolveMontage(ECombatAnimRole::Light3) == Ender);
	Set->Preload();
	TestTrue(TEXT("Resolve through the static helper"), UCombatAnimSet::Resolve(Set, ECombatAnimRole::Light1) == Jab);

	TestTrue(TEXT("A blow from behind is HitBack"), UCombatAnimSet::HitRoleFor(EHawkeyeHitDirection::Back) == ECombatAnimRole::HitBack);
	TestTrue(TEXT("From the left is HitLeft"), UCombatAnimSet::HitRoleFor(EHawkeyeHitDirection::Left) == ECombatAnimRole::HitLeft);
	TestTrue(TEXT("From the front is HitFront"), UCombatAnimSet::HitRoleFor(EHawkeyeHitDirection::Front) == ECombatAnimRole::HitFront);
	TestTrue(TEXT("A dash to the right is DodgeRight"), UCombatAnimSet::DodgeRoleFor(EHawkeyeHitDirection::Right) == ECombatAnimRole::DodgeRight);
	TestTrue(TEXT("A dash back is DodgeBack"), UCombatAnimSet::DodgeRoleFor(EHawkeyeHitDirection::Back) == ECombatAnimRole::DodgeBack);

	TestTrue(TEXT("An UpperBody slot is an upper-body montage"),
		HawkeyeCombatAnim::IsUpperBodyMontage(MakeMontage(HawkeyeCombatAnim::UpperBodySlot)));
	TestFalse(TEXT("DefaultSlot is full body"), HawkeyeCombatAnim::IsUpperBodyMontage(Jab));
	TestNull(TEXT("A mesh with no anim instance has nowhere to play"), HawkeyeCombatAnim::PickInstance(
		NewObject<USkeletalMeshComponent>(GetTransientPackage()), Jab));
	TestEqual(TEXT("A 1.2 s clip fitted to 0.4 s plays at 3x"), HawkeyeCombatAnim::FitRate(1.2f, 0.4f), 3.f, 0.001f);
	TestEqual(TEXT("Fitting clamps at 4x"), HawkeyeCombatAnim::FitRate(10.f, 0.1f), 4.f, 0.001f);
	TestEqual(TEXT("Nothing to fit plays at 1x"), HawkeyeCombatAnim::FitRate(0.f, 0.4f), 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHawkeyeCombatAnimNotifyTiming, "Hawkeye.CombatAnim.NotifyWindowsTimeTheSwing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHawkeyeCombatAnimNotifyTiming::RunTest(const FString& Parameters)
{
	using namespace HawkeyeCombatAnimTest;

	FHawkeyeTestWorld TestWorld;
	AHawkeyeAimTestCharacter* Kate = SpawnKate(TestWorld);
	AThugCharacter* Thug = SpawnThug(TestWorld, FVector(100.f, 0.f, 0.f));
	if (!Kate || !Thug)
	{
		AddError(TEXT("Failed to spawn Kate and a thug."));
		return false;
	}
	UMeleeComponent* Melee = Kate->GetMeleeComponent();
	UHealthComponent* Health = Thug->GetHealthComponent();
	Health->SetMaxHealth(500.f, true);
	USkeletalMeshComponent* Body = Kate->GetMesh();
	UAnimNotifyState_HitWindow* HitWindow = NewObject<UAnimNotifyState_HitWindow>(GetTransientPackage());
	UAnimNotifyState_ComboWindow* ComboWindow = NewObject<UAnimNotifyState_ComboWindow>(GetTransientPackage());
	const FAnimNotifyEventReference Event;

	// The hit waits for the window, however long the wind-up timer says.
	Melee->ForceNotifyTimingForTest();
	TestTrue(TEXT("The first light starts"), Kate->StartLightAttack());
	TestTrue(TEXT("Its hit is timed by the clip"), Melee->IsHitFromNotify());
	Melee->AdvanceAttack(1.f);
	TestTrue(TEXT("A second past its 0.1 s wind-up it is still winding up"), Melee->IsWindingUp());
	TestEqual(TEXT("And nothing has landed"), Health->GetCurrentHealth(), 500.f, 0.01f);
	HitWindow->NotifyBegin(Body, nullptr, 0.1f, Event);
	TestEqual(TEXT("ANS_HitWindow opening lands the 15"), Health->GetCurrentHealth(), 485.f, 0.01f);
	TestEqual(TEXT("And the swing is recovering"), Melee->GetPhase(), EMeleePhase::Recover);
	HitWindow->NotifyEnd(Body, nullptr, Event);

	// The chain holds open until the combo window closes; a press before it opens waits for it.
	TestTrue(TEXT("The chain is open"), Kate->GetCombo().IsChainOpen());
	TestTrue(TEXT("Pressed before the combo window, the next light is held"), Kate->StartLightAttack());
	TestFalse(TEXT("It has not started"), Melee->CanStartAttack());
	Kate->AdvanceMeleeFlow(1.f);
	TestTrue(TEXT("A second later the chain is still open (no 0.35 s timer)"), Kate->GetCombo().IsChainOpen());
	TestEqual(TEXT("Still the first light"), Melee->GetCurrentAttack().Name, FName(TEXT("light")));
	ComboWindow->NotifyBegin(Body, nullptr, 0.3f, Event);
	TestTrue(TEXT("ANS_ComboWindow lets a swing start"), Melee->CanStartAttack());
	Melee->ForceNotifyTimingForTest();
	Kate->AdvanceMeleeFlow(0.01f);
	TestEqual(TEXT("The held press goes the moment the window opens: the chain's second"),
		Melee->GetCurrentAttack().Name, FName(TEXT("light2")));
	HitWindow->NotifyBegin(Body, nullptr, 0.1f, Event);
	TestEqual(TEXT("It lands its 15"), Health->GetCurrentHealth(), 470.f, 0.01f);
	HitWindow->NotifyEnd(Body, nullptr, Event);

	// Left alone, the window closing ends the chain.
	ComboWindow->NotifyBegin(Body, nullptr, 0.3f, Event);
	TestTrue(TEXT("The chain is open for the third"), Kate->GetCombo().IsChainOpen());
	ComboWindow->NotifyEnd(Body, nullptr, Event);
	TestFalse(TEXT("ANS_ComboWindow closing shuts the chain"), Kate->GetCombo().IsChainOpen());
	TestEqual(TEXT("The next light is the first again"), Kate->GetCombo().GetNextStep(), 0);
	Melee->NotifyMontageEnded(false);
	TestFalse(TEXT("The clip ending ends the swing"), Melee->IsAttacking());

	// A window that finds nobody is a miss when it closes, not when it opens; sweeping goes on meanwhile.
	Thug->SetActorLocation(FVector(1000.f, 0.f, 0.f));
	Melee->ForceNotifyTimingForTest();
	TestTrue(TEXT("A light at nobody"), Kate->StartLightAttack());
	HitWindow->NotifyBegin(Body, nullptr, 0.2f, Event);
	TestTrue(TEXT("The window is open"), Melee->IsHitWindowOpen());
	Thug->SetActorLocation(FVector(100.f, 0.f, 0.f));
	Melee->AdvanceAttack(0.02f);
	TestEqual(TEXT("He stepped into the open window and took it"), Health->GetCurrentHealth(), 455.f, 0.01f);
	HitWindow->NotifyEnd(Body, nullptr, Event);
	Melee->NotifyMontageEnded(false);

	Thug->SetActorLocation(FVector(1000.f, 0.f, 0.f));
	Melee->ForceNotifyTimingForTest();
	TestTrue(TEXT("Another light at nobody"), Kate->StartLightAttack());
	HitWindow->NotifyBegin(Body, nullptr, 0.2f, Event);
	HitWindow->NotifyEnd(Body, nullptr, Event);
	TestFalse(TEXT("The window closed on nobody"), Melee->IsHitWindowOpen());
	Thug->SetActorLocation(FVector(100.f, 0.f, 0.f));
	Melee->AdvanceAttack(0.02f);
	TestEqual(TEXT("After it closes nothing more is swept: he takes nothing"), Health->GetCurrentHealth(), 455.f, 0.01f);
	TestEqual(TEXT("It was a miss: the chain starts over"), Kate->GetCombo().GetNextStep(), 0);
	Melee->NotifyMontageEnded(true);
	TestFalse(TEXT("Idle again"), Melee->IsAttacking());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHawkeyeCombatAnimAbsentKeepsProcedural, "Hawkeye.CombatAnim.NoClipKeepsProceduralBehaviour",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHawkeyeCombatAnimAbsentKeepsProcedural::RunTest(const FString& Parameters)
{
	using namespace HawkeyeCombatAnimTest;

	FHawkeyeTestWorld TestWorld;
	AHawkeyeAimTestCharacter* Kate = SpawnKate(TestWorld);
	AThugCharacter* Thug = SpawnThug(TestWorld, FVector(100.f, 0.f, 0.f));
	if (!Kate || !Thug)
	{
		AddError(TEXT("Failed to spawn Kate and a thug."));
		return false;
	}
	// Every role filled, but no anim instance to play on: exactly the game before clips.
	UCombatAnimSet* Set = MakeFullSet();
	Kate->SetCombatAnimSet(Set);
	Kate->ApplyCombatAnimSet();
	Thug->SetCombatAnimSet(Set);
	Thug->ApplyCombatAnimSet();
	UMeleeComponent* Melee = Kate->GetMeleeComponent();
	UHealthComponent* Health = Thug->GetHealthComponent();
	Health->SetMaxHealth(500.f, true);
	TestTrue(TEXT("The melee component has the set"), Melee->GetAnimSet() == Set);
	TestTrue(TEXT("So does the bow"), Kate->GetBowComponent()->GetAnimSet() == Set);

	TestTrue(TEXT("A light starts"), Kate->StartLightAttack());
	TestNull(TEXT("Its clip did not play"), Melee->GetCurrentMontage());
	TestFalse(TEXT("So the timers time it"), Melee->IsHitFromNotify() || Melee->IsComboFromNotify());
	TestTrue(TEXT("And the strike pose is the arms"), Kate->GetStrikePose().IsActive());
	Melee->AdvanceAttack(0.09f);
	TestEqual(TEXT("Nothing at 0.09 s"), Health->GetCurrentHealth(), 500.f, 0.01f);
	Melee->AdvanceAttack(0.02f);
	TestEqual(TEXT("15 at 0.1 s, as before clips"), Health->GetCurrentHealth(), 485.f, 0.01f);
	Melee->AdvanceAttack(0.2f);
	Kate->AdvanceMeleeFlow(0.2f);
	TestFalse(TEXT("Over at 0.3 s"), Melee->IsAttacking());
	Kate->AdvanceMeleeFlow(0.2f);
	TestFalse(TEXT("The 0.35 s chain timer still runs"), Kate->GetCombo().IsChainOpen());

	TestTrue(TEXT("A heavy starts"), Kate->StartHeavyAttack());
	Melee->AdvanceAttack(0.61f);
	TestTrue(TEXT("It knocks him down"), Thug->IsKnockedDown());
	TestFalse(TEXT("Not through a clip"), Thug->IsKnockdownFromClip());
	Thug->UpdateKnockdown(Thug->KnockdownSeconds + 0.1f);
	TestFalse(TEXT("Up after KnockdownSeconds"), Thug->IsKnockedDown());

	Melee->AdvanceAttack(1.f);
	Kate->TestTickDodge(2.f);
	TestTrue(TEXT("A dodge starts with every dodge role filled"), Kate->TryDodge(FVector(0.f, 1.f, 0.f)));
	TestTrue(TEXT("The dash runs"), Kate->IsDodging());
	TestTrue(TEXT("Invulnerable as before"), Kate->GetHealthComponent()->IsInvulnerable());
	Kate->TestTickDodge(2.f);

	Thug->StaggerFor(Kate, 0.5f);
	TestTrue(TEXT("A thug's hit reaction still staggers him"), Thug->IsStaggered());

	UBowComponent* Bow = Kate->GetBowComponent();
	Kate->GetInventoryComponent()->GiveBow(NewObject<UBowDefinition>(Kate));
	Bow->SetTestTimeSeconds(10.0);
	TestTrue(TEXT("A draw starts"), Bow->StartDraw());
	TestFalse(TEXT("With no bow clip playing"), Bow->IsBowClipPlaying());
	Bow->CancelDraw();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHawkeyeBowIKClipAnchor, "Hawkeye.BowIK.ClipHoldsBowArmStringOnGrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHawkeyeBowIKClipAnchor::RunTest(const FString& Parameters)
{
	using namespace HawkeyeCombatAnimTest;

	const FBowHandIKSettings Settings;
	const FVector Grip(120.f, -30.f, 150.f);
	const FRotator Aim(10.f, 90.f, 0.f);
	const FBowHandIKTargets OnGrip = UHawkeyeBowIKAnimInstance::ComputeHandTargetsOnGrip(Grip, Aim, 1.f, Settings);
	TestTrue(TEXT("The bow hand is exactly where the clip put the bow"), OnGrip.BowHand.Equals(Grip, 0.01));
	TestTrue(TEXT("The string hand keeps its full-draw place relative to the grip, along the aim"),
		(OnGrip.StringHand - OnGrip.BowHand).Equals(Aim.RotateVector(Settings.StringHandFullOffset - Settings.GripOffset), 0.01));

	FHawkeyeTestWorld TestWorld;
	AHawkeyeAimTestCharacter* Kate = SpawnKate(TestWorld);
	UBowComponent* Bow = Kate ? Kate->GetBowComponent() : nullptr;
	if (!TestNotNull(TEXT("Kate with a bow component"), Bow))
	{
		return false;
	}
	UBowDefinition* Definition = NewObject<UBowDefinition>(Kate);
	Kate->GetInventoryComponent()->GiveBow(Definition);
	Bow->SetTestTimeSeconds(10.0);
	TestTrue(TEXT("Draws"), Bow->StartDraw());
	Bow->SetTestTimeSeconds(10.0 + Definition->FullDrawSeconds);

	UHawkeyeBowIKAnimInstance* Procedural = NewObject<UHawkeyeBowIKAnimInstance>(Kate->GetMesh());
	UHawkeyeBowIKAnimInstance* Clip = NewObject<UHawkeyeBowIKAnimInstance>(Kate->GetMesh());
	for (int32 Step = 0; Step < 3; ++Step)
	{
		Procedural->UpdateFromBowWithClip(Bow, 0.075f, false);
		Clip->UpdateFromBowWithClip(Bow, 0.075f, true);
	}
	TestEqual(TEXT("Without a clip the bow hand is on the IK"), Procedural->LeftArmAlpha, 1.f, 0.001f);
	TestEqual(TEXT("No clip, no clip alpha"), Procedural->GetClipAlpha(), 0.f);
	TestEqual(TEXT("A clip takes the bow arm over in 0.15 s"), Clip->GetClipAlpha(), 1.f, 0.001f);
	TestEqual(TEXT("So the bow hand's IK is off"), Clip->LeftArmAlpha, 0.f, 0.001f);
	TestEqual(TEXT("The string hand stays on the IK"), Clip->RightArmAlpha, 1.f, 0.001f);
	TestTrue(TEXT("The procedural spine turns side-on"), Procedural->SpineTwist.Yaw > Settings.SideOnDegrees - 0.1f);
	TestEqual(TEXT("Under a clip only the turn toward the aim is added (none here)"), Clip->SpineTwist.Yaw, 0.0, 0.01);
	TestEqual(TEXT("And the neck is not turned back"), Clip->NeckTwist.Yaw, 0.0, 0.01);
	Bow->CancelDraw();
	return true;
}

#endif

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/SkeletalMeshComponent.h"
#include "InputMappingContext.h"
#include "Materials/MaterialInterface.h"
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"
#include "Player/CastleCharacter.h"
#include "Tests/CastleTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Kate's locomotion comes from Epic's Game Animation Sample (Tools/Editor/import_gasp.py). These
 * are the one deliberate exception to "tests never load Content": the point is that the copied
 * sample assets load and are wired the way create_blueprints.py says, so they load BP_Kate, the
 * sample's character and AnimBP, and IMC_Sandbox. Everything else stays content-free.
 */
namespace CastleGaspTest
{
	static const TCHAR* KateClassPath = TEXT("/Game/Blueprints/Player/BP_Kate.BP_Kate_C");
	static const TCHAR* SandboxMappingPath = TEXT("/Game/Input/IMC_Sandbox.IMC_Sandbox");

	static UClass* LoadKateClass()
	{
		return StaticLoadClass(ACastleCharacter::StaticClass(), nullptr, KateClassPath);
	}
}

/**
 * The runtime modules of every plugin the copied assets import from. Enabling a plugin in
 * Castle.uproject is what loads its modules; a missing one means a sample asset loads with a
 * missing class.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleGaspPluginsEnabled, "Castle.Gasp.PluginsEnabled",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleGaspPluginsEnabled::RunTest(const FString& Parameters)
{
	const TCHAR* Modules[] = {
		TEXT("PoseSearch"), TEXT("Chooser"), TEXT("BlendStack"), TEXT("AnimationWarpingRuntime"),
		TEXT("MotionWarping"), TEXT("CurveExpression"), TEXT("DrawDebugLibrary"), TEXT("Mover"),
		TEXT("MovieSceneAnimMixer"), TEXT("GameplayCameras"), TEXT("SmartObjectsModule"),
		TEXT("StateTreeModule"), TEXT("GameplayStateTreeModule"), TEXT("GameplayInteractionsModule"),
		TEXT("PBIK"),
	};
	for (const TCHAR* Module : Modules)
	{
		TestTrue(FString::Printf(TEXT("%s is loaded"), Module), FModuleManager::Get().IsModuleLoaded(Module));
	}
	return true;
}

/** BP_Kate -> SandboxCharacter_CMC -> BP_CastleCharacter -> ACastleCharacter. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleGaspKateParentChain, "Castle.Gasp.KateParentChain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleGaspKateParentChain::RunTest(const FString& Parameters)
{
	UClass* Kate = CastleGaspTest::LoadKateClass();
	if (!TestNotNull(TEXT("BP_Kate_C loads"), Kate))
	{
		return false;
	}

	const TCHAR* Expected[] = { TEXT("BP_Kate_C"), TEXT("SandboxCharacter_CMC_C"), TEXT("BP_CastleCharacter_C"),
		TEXT("CastleCharacter") };
	const UClass* Class = Kate;
	for (const TCHAR* Name : Expected)
	{
		if (!TestNotNull(FString::Printf(TEXT("%s is in the chain"), Name), Class))
		{
			return false;
		}
		TestEqual(TEXT("Parent chain"), Class->GetName(), FString(Name));
		Class = Class->GetSuperClass();
	}

	const ACastleCharacter* Defaults = GetDefault<ACastleCharacter>(Kate);
	TestTrue(TEXT("BP_Kate carries the sample's CharacterInputState"), Defaults->UsesGaspLocomotion());
	TestFalse(TEXT("A plain ACastleCharacter does not"),
		GetDefault<ACastleCharacter>(ACastleAimTestCharacter::StaticClass())->UsesGaspLocomotion());
	return true;
}

/** The UEFN mannequin, the motion-matching AnimBP, and the purple suit on BP_Kate's body. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleGaspKateBody, "Castle.Gasp.KateBody",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleGaspKateBody::RunTest(const FString& Parameters)
{
	UClass* Kate = CastleGaspTest::LoadKateClass();
	const ACastleCharacter* Defaults = Kate ? GetDefault<ACastleCharacter>(Kate) : nullptr;
	const USkeletalMeshComponent* Body = Defaults ? Defaults->GetMesh() : nullptr;
	if (!TestNotNull(TEXT("BP_Kate has a body"), Body))
	{
		return false;
	}

	TestEqual(TEXT("Mesh"), GetNameSafe(Body->GetSkeletalMeshAsset()), FString(TEXT("SKM_UEFN_Mannequin")));
	TestEqual(TEXT("AnimBP"), GetNameSafe(Body->AnimClass.Get()), FString(TEXT("SandboxCharacter_CMC_ABP_C")));
	TestTrue(TEXT("Animated by the AnimBP"), Body->GetAnimationMode() == EAnimationMode::AnimationBlueprint);
	TestEqual(TEXT("Slot 0 wears the suit"), GetNameSafe(Body->GetMaterial(0)), FString(TEXT("M_KateSuit")));
	return true;
}

/** IMC_Sandbox is pushed by the sample's graph on possession; it must map nothing. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleGaspSandboxMappingEmpty, "Castle.Gasp.SandboxMappingEmpty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleGaspSandboxMappingEmpty::RunTest(const FString& Parameters)
{
	const UInputMappingContext* Sandbox = LoadObject<UInputMappingContext>(nullptr, CastleGaspTest::SandboxMappingPath);
	if (!TestNotNull(TEXT("IMC_Sandbox loads"), Sandbox))
	{
		return false;
	}
	TestEqual(TEXT("IMC_Sandbox has no mappings"), Sandbox->GetMappings().Num(), 0);
	return true;
}

/**
 * Our gait, crouch and aim reach the sample's CharacterInputState. Spawns the real BP_Kate, so the
 * sample's construction and BeginPlay run as well.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleGaspInputStateBridge, "Castle.Gasp.InputStateBridge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleGaspInputStateBridge::RunTest(const FString& Parameters)
{
	UClass* KateClass = CastleGaspTest::LoadKateClass();
	if (!TestNotNull(TEXT("BP_Kate_C loads"), KateClass))
	{
		return false;
	}

	const FCastleTestWorld TestWorld;
	ACastleCharacter* Kate = Cast<ACastleCharacter>(TestWorld.SpawnActor(KateClass, FVector::ZeroVector, FRotator::ZeroRotator));
	if (!TestNotNull(TEXT("BP_Kate spawned"), Kate))
	{
		return false;
	}

	TestTrue(TEXT("The AnimBP owns the body"), Kate->IsBodyDrivenByAnimBlueprint());

	Kate->SyncGaspInputState();
	TestFalse(TEXT("Standing: no sprint"), Kate->GetGaspInputFlag(TEXT("WantsToSprint")));
	TestFalse(TEXT("Standing: no aim"), Kate->GetGaspInputFlag(TEXT("WantsToAim")));
	TestFalse(TEXT("Standing: no crouch"), Kate->GetGaspInputFlag(TEXT("WantsToCrouch")));

	Kate->StartAim();
	TestTrue(TEXT("Aiming: WantsToAim"), Kate->GetGaspInputFlag(TEXT("WantsToAim")));
	TestTrue(TEXT("Aiming: WantsToStrafe"), Kate->GetGaspInputFlag(TEXT("WantsToStrafe")));
	TestTrue(TEXT("Aiming: WantsToWalk"), Kate->GetGaspInputFlag(TEXT("WantsToWalk")));

	Kate->StopAim();
	TestFalse(TEXT("Aim released: WantsToAim clears"), Kate->GetGaspInputFlag(TEXT("WantsToAim")));
	TestFalse(TEXT("Aim released: WantsToStrafe clears"), Kate->GetGaspInputFlag(TEXT("WantsToStrafe")));
	return true;
}

/** Without the sample's struct nothing changes: the clip switch and our speeds stay in charge. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleGaspNativeCharacterUnaffected, "Castle.Gasp.NativeCharacterUnaffected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCastleGaspNativeCharacterUnaffected::RunTest(const FString& Parameters)
{
	const FCastleTestWorld TestWorld;
	ACastleAimTestCharacter* Character = Cast<ACastleAimTestCharacter>(
		TestWorld.SpawnActor(ACastleAimTestCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator));
	if (!TestNotNull(TEXT("Character spawned"), Character))
	{
		return false;
	}

	TestFalse(TEXT("No GASP bridge"), Character->UsesGaspLocomotion());
	TestFalse(TEXT("No AnimBP on the body"), Character->IsBodyDrivenByAnimBlueprint());
	Character->SyncGaspInputState();
	TestFalse(TEXT("Flags read false"), Character->GetGaspInputFlag(TEXT("WantsToSprint")));
	Character->TestSetMoveInput(1.f, 0.f);
	TestEqual(TEXT("Our run speed still reaches the movement component"), Character->MaxWalkSpeed(),
		Character->TestRunSpeed());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

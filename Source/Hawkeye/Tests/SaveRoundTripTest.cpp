// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/BowComponent.h"
#include "Combat/HealthComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "HawkeyePlayerController.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Mission/MissionObjective.h"
#include "Mission/MissionSubsystem.h"
#include "Partner/HawkeyePartnerController.h"
#include "Player/HawkeyeCharacter.h"
#include "Player/InventoryComponent.h"
#include "Save/HawkeyeSaveSubsystem.h"
#include "SpudSubsystem.h"
#include "Tests/AutomationCommon.h"
#include "Tests/PartnerScreenshots.h"
#include "Tests/SaveScreenshots.h"
#include "UnrealClient.h"
#include "World/InteractionComponent.h"
#include "World/Safehouse.h"
#include "World/ThugCharacter.h"

#include "Tests/HawkeyeShots.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace HawkeyeSaveRoundTrip
{
	static const TCHAR* Slot = TEXT("HawkeyeCampaignRoundTrip");
	static constexpr float PositionTolerance = 50.f;

	/** What the round trip wrote, to compare with what comes back. */
	struct FState
	{
		TWeakObjectPtr<UWorld> SavedWorld;
		FVector KateLocation = FVector::ZeroVector;
		float Health = 0.f;
		int32 ArrowsBefore = 0;
		TArray<int32> ArrowCounts;
		TArray<FName> Completed;
		FName DeadThug;
		int32 SavesBefore = 0;
	};

	static UWorld* FindWorld()
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

	static AHawkeyePlayerController* FindController(UWorld* World)
	{
		return World ? Cast<AHawkeyePlayerController>(World->GetFirstPlayerController()) : nullptr;
	}

	static AHawkeyeCharacter* FindPlayer(UWorld* World)
	{
		const AHawkeyePlayerController* PC = FindController(World);
		return PC ? Cast<AHawkeyeCharacter>(PC->GetPawn()) : nullptr;
	}

	/** The first safehouse (ch01_east_7th); the district has two. */
	static ASafehouse* FindSafehouse(UWorld* World)
	{
		for (TActorIterator<ASafehouse> It(World); It; ++It)
		{
			if (It->SafehouseId == TEXT("ch01_east_7th"))
			{
				return *It;
			}
		}
		return nullptr;
	}

	/** Puts the player Distance in front of the safehouse door, facing it. */
	static bool PlaceBeforeDoor(UWorld* World, float Distance, float Pitch)
	{
		AHawkeyePlayerController* PC = FindController(World);
		AHawkeyeCharacter* Player = FindPlayer(World);
		const ASafehouse* Safehouse = FindSafehouse(World);
		if (!PC || !Player || !Safehouse)
		{
			return false;
		}
		const FVector Out = Safehouse->GetActorForwardVector();
		const FVector Spot = Safehouse->GetActorLocation() + Out * Distance + FVector(0.f, 0.f, 100.f);
		const FRotator Facing(0.f, (-Out).Rotation().Yaw, 0.f);
		Player->TeleportTo(Spot, Facing);
		PC->SetControlRotation(FRotator(Pitch, Facing.Yaw, 0.f));
		return true;
	}

	static FString ShotPath(const FString& FileName)
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Screenshots") / TEXT("Kate") / FileName);
	}

	static void TakeShot(FAutomationTestBase* Test, const FString& FileName)
	{
		const FString FullPath = ShotPath(FileName);
		HawkeyeShots::Request(Test, FullPath, /*bShowUI=*/true);
	}

	static TArray<int32> ReadArrowCounts(const UInventoryComponent* Inventory)
	{
		TArray<int32> Counts;
		for (int32 QuiverSlot = 1; QuiverSlot <= 7; ++QuiverSlot)
		{
			Counts.Add(Inventory ? Inventory->GetArrowCount(QuiverSlot) : -1);
		}
		return Counts;
	}
}

/**
 * The save contract end to end, in the standalone game: complete reach_roof, loose three arrows,
 * take 20 damage, kill a thug, stand by the safehouse, save; then load through the subsystem (a
 * full SPUD load: the map reloads and is restored) and check the objective, the quiver, the
 * health, the dead thug and Kate's position (within 50 cm). Uses its own slot.
 *
 *   UnrealEditor-Cmd.exe Hawkeye.uproject -game -windowed -ResX=1280 -ResY=720 -unattended
 *       -nosplash -log -ExecCmds="Automation RunTests Hawkeye.Save.RoundTrip; Quit"
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHawkeyeSaveRoundTrip, "Hawkeye.Save.RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FHawkeyeSaveRoundTrip::RunTest(const FString& Parameters)
{
	using namespace HawkeyeSaveRoundTrip;
	if (GIsEditor)
	{
		AddInfo(TEXT("The round trip reloads the district; run it from the standalone game (-game)."));
		return true;
	}

	TSharedRef<FState> State = MakeShared<FState>();
	AutomationOpenMap(TEXT("/Game/Maps/L_District_EastVillage"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(6.f));

	// Set up: our own slot, the partner still, Kate by the safehouse, reach_roof done (which autosaves).
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State]()
	{
		UWorld* World = FindWorld();
		UHawkeyeSaveSubsystem* Save = UHawkeyeSaveSubsystem::Get(World);
		AHawkeyeCharacter* Kate = FindPlayer(World);
		UMissionSubsystem* Missions = UMissionSubsystem::Get(World);
		if (!Save || !Kate || !Missions || !PlaceBeforeDoor(World, 400.f, -10.f))
		{
			AddError(TEXT("No save subsystem, player, mission or safehouse in the district."));
			return true;
		}
		Save->SlotNameOverride = Slot;
		IFileManager::Get().Delete(*USpudSubsystem::GetSaveGameFilePath(Slot), false, true, true);
		HawkeyeFreezePartner(World);
		State->SavesBefore = Save->GetSaveCount();
		TestTrue(TEXT("reach_roof completes"), Missions->CompleteObjective(TEXT("reach_roof")));
		TestEqual(TEXT("Completing an objective autosaved"), Save->GetSaveCount(), State->SavesBefore + 1);
		State->ArrowsBefore = Kate->GetInventoryComponent()->GetArrowCount(1);
		Kate->GetInventoryComponent()->SelectArrowSlot(1);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.5f));

	for (int32 Shot = 0; Shot < 3; ++Shot)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([]()
		{
			if (AHawkeyeCharacter* Kate = FindPlayer(FindWorld()))
			{
				Kate->StartAim();
				Kate->GetBowComponent()->StartDraw();
			}
			return true;
		}));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.8f));
		ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this]()
		{
			AHawkeyeCharacter* Kate = FindPlayer(FindWorld());
			TestTrue(TEXT("An arrow looses"), Kate && Kate->GetBowComponent()->ReleaseDraw());
			return true;
		}));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.6f));
	}

	// 20 damage, a dead thug, a moment to settle, then the save.
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State]()
	{
		UWorld* World = FindWorld();
		AHawkeyeCharacter* Kate = FindPlayer(World);
		if (!Kate)
		{
			return true;
		}
		Kate->StopAim();
		TestEqual(TEXT("Three arrows gone from the quiver"), Kate->GetInventoryComponent()->GetArrowCount(1), State->ArrowsBefore - 3);
		Kate->GetHealthComponent()->ApplyDamage(20.f, nullptr);
		for (TActorIterator<AThugCharacter> It(World); It; ++It)
		{
			if (It->GetHealthComponent() && It->GetHealthComponent()->IsAlive())
			{
				It->GetHealthComponent()->ApplyDamage(10000.f, Kate);
				State->DeadThug = It->GetFName();
				break;
			}
		}
		TestFalse(TEXT("A thug was killed"), State->DeadThug.IsNone());
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State]()
	{
		UWorld* World = FindWorld();
		AHawkeyeCharacter* Kate = FindPlayer(World);
		UHawkeyeSaveSubsystem* Save = UHawkeyeSaveSubsystem::Get(World);
		if (!Kate || !Save)
		{
			return true;
		}
		State->KateLocation = Kate->GetActorLocation();
		State->Health = Kate->GetHealthComponent()->GetCurrentHealth();
		State->ArrowCounts = ReadArrowCounts(Kate->GetInventoryComponent());
		State->Completed = UMissionSubsystem::Get(World)->GetTracker()->GetCompletedObjectiveIds();
		TestTrue(TEXT("The campaign saves"), Save->SaveCampaign(TEXT("round trip")));
		AddInfo(FString::Printf(TEXT("Saved: Kate at %s, %.0f health, arrows %d, thug %s dead."), *State->KateLocation.ToString(),
			State->Health, State->ArrowCounts[0], *State->DeadThug.ToString()));
		State->SavedWorld = World;
		TestTrue(TEXT("The load starts"), Save->LoadCampaign());
		return true;
	}));

	// The load swaps the world out; wait for the restored one.
	ADD_LATENT_AUTOMATION_COMMAND(FUntilCommand([State]()
	{
		UWorld* World = FindWorld();
		const UHawkeyeSaveSubsystem* Save = UHawkeyeSaveSubsystem::Get(World);
		return World && World != State->SavedWorld.Get() && Save && !Save->IsLoading() && FindPlayer(World);
	}, [this]()
	{
		AddError(TEXT("The load did not finish within 45 s."));
		return true;
	}, 45.f));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));

	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State]()
	{
		UWorld* World = FindWorld();
		AHawkeyeCharacter* Kate = FindPlayer(World);
		UMissionSubsystem* Missions = UMissionSubsystem::Get(World);
		UHawkeyeSaveSubsystem* Save = UHawkeyeSaveSubsystem::Get(World);
		if (!Kate || !Missions || !Save)
		{
			AddError(TEXT("No player or mission after the load."));
			return true;
		}
		const UMissionObjective* ReachRoof = Missions->GetTracker()->FindObjective(TEXT("reach_roof"));
		TestTrue(TEXT("reach_roof is still complete"), ReachRoof && ReachRoof->IsCompleted());
		TestEqual(TEXT("The same objectives are complete"), Missions->GetTracker()->GetCompletedObjectiveIds(), State->Completed);
		TestEqual(TEXT("The quiver came back slot for slot"), ReadArrowCounts(Kate->GetInventoryComponent()), State->ArrowCounts);
		TestEqual(TEXT("Health came back"), Kate->GetHealthComponent()->GetCurrentHealth(), State->Health, 0.01f);
		const float Offset = FVector::Dist(Kate->GetActorLocation(), State->KateLocation);
		TestTrue(FString::Printf(TEXT("Kate is where she was saved (%.1f cm off)"), Offset), Offset <= PositionTolerance);
		bool bFoundThug = false;
		for (TActorIterator<AThugCharacter> It(World); It; ++It)
		{
			if (It->GetFName() == State->DeadThug)
			{
				bFoundThug = true;
				TestFalse(TEXT("The dead thug is still dead"), It->GetHealthComponent()->IsAlive());
				TestTrue(TEXT("And lying down"), It->IsLimp());
			}
		}
		TestTrue(TEXT("The dead thug is in the reloaded map"), bFoundThug);
		AddInfo(FString::Printf(TEXT("Loaded: Kate at %s (%.1f cm off), %.0f health, arrows %d."), *Kate->GetActorLocation().ToString(),
			Offset, Kate->GetHealthComponent()->GetCurrentHealth(), Kate->GetInventoryComponent()->GetArrowCount(1)));
		IFileManager::Get().Delete(*USpudSubsystem::GetSaveGameFilePath(Slot), false, true, true);
		Save->SlotNameOverride.Reset();
		return true;
	}));
	// Fails the test for any capture that did not reach the disk.
	ADD_LATENT_AUTOMATION_COMMAND(FHawkeyeWaitForShots(this));
	return true;
}

// --- The save pass of the Kate screenshots ---------------------------------------------------------

void HawkeyeAddSaveShots(FAutomationTestBase* Test)
{
	using namespace HawkeyeSaveRoundTrip;

	// Back to Kate if the partner pass left the player on Clint, and nothing left invulnerable.
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Test]()
	{
		UWorld* World = FindWorld();
		AHawkeyePlayerController* PC = FindController(World);
		AHawkeyeCharacter* Player = FindPlayer(World);
		if (PC && Player && Player->GetCharacterName().ToString() != TEXT("Kate"))
		{
			PC->bAllowSwitchingOverride = true;
			PC->SwitchCharacter();
		}
		HawkeyeFreezePartner(World);
		if (!PlaceBeforeDoor(World, 600.f, -6.f))
		{
			Test->AddError(TEXT("No safehouse (City_Safehouse) in the district."));
		}
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Test]() { TakeShot(Test, TEXT("safehouse_door.png")); return true; }));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));

	// Walk-in: stand in the entry zone and press Interact through the real interaction component.
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([]() { PlaceBeforeDoor(FindWorld(), 110.f, -6.f); return true; }));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Test]()
	{
		UWorld* World = FindWorld();
		AHawkeyePlayerController* PC = FindController(World);
		AHawkeyeCharacter* Kate = FindPlayer(World);
		UInteractionComponent* Interaction = Kate ? Kate->GetInteractionComponent() : nullptr;
		if (!PC || !Interaction)
		{
			Test->AddError(TEXT("No player to walk into the safehouse."));
			return true;
		}
		Kate->GetHealthComponent()->ApplyDamage(30.f, nullptr);
		Interaction->RefreshFocus();
		Test->TestTrue(TEXT("Interact (E) at the door opens the safehouse"), Interaction->TryInteract() && PC->IsSafehouseMenuOpen());
		Test->TestEqual(TEXT("Entering heals to full"), Kate->GetHealthComponent()->GetHealthPercent(), 1.f);
		const UHawkeyeSaveSubsystem* Save = UHawkeyeSaveSubsystem::Get(World);
		Test->TestTrue(TEXT("The safehouse is discovered"), Save && Save->IsSafehouseDiscovered(TEXT("ch01_east_7th")));
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.7f));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Test]() { TakeShot(Test, TEXT("safehouse_menu.png")); return true; }));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));

	// The main menu, as Quit to menu shows it over the paused district.
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([]()
	{
		if (AHawkeyePlayerController* PC = FindController(FindWorld()))
		{
			PC->CloseSafehouseMenu();
			PC->ShowMainMenu();
		}
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.7f));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Test]()
	{
		const AHawkeyePlayerController* PC = FindController(FindWorld());
		Test->TestTrue(TEXT("The main menu is up"), PC && PC->IsMainMenuOpen());
		TakeShot(Test, TEXT("main_menu.png"));
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));

	// A real death with no revive to be had: the fade to black, caught halfway.
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([]()
	{
		UWorld* World = FindWorld();
		if (AHawkeyePlayerController* PC = FindController(World))
		{
			PC->HideMainMenu();
		}
		for (TActorIterator<AHawkeyePartnerController> It(World); It; ++It)
		{
			It->SetLeader(nullptr);
		}
		PlaceBeforeDoor(World, 500.f, -6.f);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([]()
	{
		if (AHawkeyeCharacter* Kate = FindPlayer(FindWorld()))
		{
			Kate->GetHealthComponent()->SetInvulnerable(false);
			Kate->GetHealthComponent()->SetCannotDie(false);
			Kate->GetHealthComponent()->ApplyDamage(10000.f, nullptr);
		}
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.75f));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Test]() { TakeShot(Test, TEXT("death_fade.png")); return true; }));
	// Long enough for the fade to finish and the last save to load behind it.
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(8.f));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Test]()
	{
		const AHawkeyeCharacter* Kate = FindPlayer(FindWorld());
		Test->TestTrue(TEXT("After the death the last save loaded, alive"), Kate && Kate->GetHealthComponent()->IsAlive());
		return true;
	}));
}

#endif

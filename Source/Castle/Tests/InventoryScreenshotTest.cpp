// Copyright Epic Games, Inc. All Rights Reserved.

#include "CastlePlayerController.h"
#include "Combat/WeaponDefinition.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Player/InventoryComponent.h"
#include "Tests/AutomationCommon.h"
#include "UnrealClient.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Reference shots of the hotbar and the inventory screen, written to Saved/Screenshots/UI/.
 * Look-at-them tools, not assertions, like the room shots in ScreenshotTest.cpp:
 *
 *   Castle.Screenshot.Hotbar    hotbar.png    the HUD with the pistol drawn in slot 2
 *   Castle.Screenshot.Inventory inventory.png the Tab screen over a paused game
 *
 * Needs a real RHI, so it is an explicit no-op in the normal -nullrhi suite:
 *
 *   UnrealEditor-Cmd.exe Castle.uproject -ExecCmds="Automation RunTests Castle.Screenshot; Quit"
 *       -unattended -nosplash -nop4 -stdout
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleScreenshotHotbar, "Castle.Screenshot.Hotbar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastleScreenshotInventory, "Castle.Screenshot.Inventory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace CastleUiShot
{
	/** The pistol the hotbar should be showing. Loaded, because this shot is about content. */
	static const TCHAR* PistolPath = TEXT("/Game/Blueprints/Weapons/DA_Weapon_Pistol.DA_Weapon_Pistol");

	static FString UiPath(const FString& FileName)
	{
		return FPaths::ConvertRelativePathToFull(
			FPaths::ProjectSavedDir() / TEXT("Screenshots") / TEXT("UI") / FileName);
	}

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

	static ACastlePlayerController* FindController()
	{
		UWorld* World = FindWorld();
		return World ? Cast<ACastlePlayerController>(World->GetFirstPlayerController()) : nullptr;
	}

	static UInventoryComponent* FindInventory()
	{
		const ACastlePlayerController* PC = FindController();
		const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		return Pawn ? Pawn->FindComponentByClass<UInventoryComponent>() : nullptr;
	}

	/** True when this process cannot render, in which case the shots are skipped. */
	static bool SkipWithoutRHI(FAutomationTestBase& Test)
	{
		if (FApp::CanEverRender())
		{
			return false;
		}
		Test.AddInfo(TEXT("No RHI: skipping the UI screenshots. Re-run without -nullrhi to capture them."));
		return true;
	}
}

/** Put the pistol in slot 2 and draw it, so the hotbar has something to show. */
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FCastleDrawPistol, FAutomationTestBase*, Test);

bool FCastleDrawPistol::Update()
{
	UInventoryComponent* Inventory = CastleUiShot::FindInventory();
	if (!Inventory)
	{
		Test->AddError(TEXT("No inventory on the player pawn; check BP_CastleCharacter."));
		return true;
	}

	UWeaponDefinition* Pistol = LoadObject<UWeaponDefinition>(nullptr, CastleUiShot::PistolPath);
	if (!Pistol)
	{
		Test->AddError(TEXT("DA_Weapon_Pistol did not load; run Tools\\create-content.ps1."));
		return true;
	}

	Inventory->AddWeapon(Pistol);
	// The swap lockout would otherwise still be running when the shot is taken.
	Inventory->FinishSwapNow();

	// One keycard, so the inventory screen has a row under every heading.
	Inventory->GiveKeycard(FName(TEXT("cellblock")));
	return true;
}

/** Open the Tab screen the way the player would. */
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FCastleOpenInventoryScreen, FAutomationTestBase*, Test);

bool FCastleOpenInventoryScreen::Update()
{
	ACastlePlayerController* PC = CastleUiShot::FindController();
	if (!PC)
	{
		Test->AddError(TEXT("No ACastlePlayerController to open the inventory with."));
		return true;
	}

	PC->ToggleInventory();
	if (!PC->IsInventoryOpen())
	{
		Test->AddError(TEXT("ToggleInventory did nothing; check InventoryWidgetClass on BP_CastlePlayerController."));
	}
	return true;
}

/** Ask for one screenshot under Saved/Screenshots/UI, with the UI drawn. */
DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(
	FCastleTakeInventoryUiShot, FAutomationTestBase*, Test, FString, FileName);

bool FCastleTakeInventoryUiShot::Update()
{
	const FString FullPath = CastleUiShot::UiPath(FileName);
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.CreateDirectoryTree(*FPaths::GetPath(FullPath));
	FScreenshotRequest::RequestScreenshot(FullPath, /*bInShowUI=*/true, /*bAddFilenameSuffix=*/false);
	Test->AddInfo(FString::Printf(TEXT("Requested %s"), *FullPath));
	return true;
}

bool FCastleScreenshotHotbar::RunTest(const FString& Parameters)
{
	if (CastleUiShot::SkipWithoutRHI(*this))
	{
		return true;
	}

	AutomationOpenMap(TEXT("/Game/Maps/L_M01_CellBlockD"));
	// Long enough for the editor's own billboards to stop drawing over the game view.
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.f));

	ADD_LATENT_AUTOMATION_COMMAND(FCastleDrawPistol(this));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleTakeInventoryUiShot(this, TEXT("hotbar.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));

	return true;
}

bool FCastleScreenshotInventory::RunTest(const FString& Parameters)
{
	if (CastleUiShot::SkipWithoutRHI(*this))
	{
		return true;
	}

	AutomationOpenMap(TEXT("/Game/Maps/L_M01_CellBlockD"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.f));

	// Something to list: a gun in slot 2 and a keycard on the ring.
	ADD_LATENT_AUTOMATION_COMMAND(FCastleDrawPistol(this));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleOpenInventoryScreen(this));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));
	ADD_LATENT_AUTOMATION_COMMAND(FCastleTakeInventoryUiShot(this, TEXT("inventory.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

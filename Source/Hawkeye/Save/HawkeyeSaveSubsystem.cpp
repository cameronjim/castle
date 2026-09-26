// Copyright Epic Games, Inc. All Rights Reserved.

#include "Save/HawkeyeSaveSubsystem.h"

#include "Hawkeye.h"
#include "HawkeyePlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Combat/HealthComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Mission/MissionDefinition.h"
#include "Mission/MissionSubsystem.h"
#include "Player/HawkeyeCharacter.h"
#include "SpudCustomSaveInfo.h"
#include "SpudState.h"
#include "SpudSubsystem.h"
#include "UObject/SoftObjectPath.h"

const TCHAR* UHawkeyeSaveSubsystem::DefaultSlotName = TEXT("HawkeyeCampaign");

namespace HawkeyeSave
{
	static const TCHAR* GlobalName = TEXT("HawkeyeCampaign");
	static const TCHAR* VersionKey = TEXT("Version");
	static const TCHAR* MissionKey = TEXT("Mission");
	static const TCHAR* ReasonKey = TEXT("Reason");
}

UHawkeyeSaveSubsystem* UHawkeyeSaveSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UHawkeyeSaveSubsystem>() : nullptr;
}

void UHawkeyeSaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<USpudSubsystem>();
	Super::Initialize(Collection);

	Campaign = NewObject<UHawkeyeCampaignState>(this, TEXT("HawkeyeCampaign"));
	if (USpudSubsystem* Spud = GetSpud())
	{
		Spud->AddPersistentGlobalObjectWithName(Campaign, HawkeyeSave::GlobalName);
		Spud->PostLoadGame.AddDynamic(this, &UHawkeyeSaveSubsystem::HandleSpudPostLoad);
		Spud->PostSaveGame.AddDynamic(this, &UHawkeyeSaveSubsystem::HandleSpudPostSave);
	}
	PreLoadMapHandle = FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &UHawkeyeSaveSubsystem::HandlePreLoadMap);
}

void UHawkeyeSaveSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PreLoadMap.Remove(PreLoadMapHandle);
	if (USpudSubsystem* Spud = GetSpud())
	{
		Spud->RemovePersistentGlobalObject(Campaign);
		Spud->PostLoadGame.RemoveDynamic(this, &UHawkeyeSaveSubsystem::HandleSpudPostLoad);
		Spud->PostSaveGame.RemoveDynamic(this, &UHawkeyeSaveSubsystem::HandleSpudPostSave);
	}
	Super::Deinitialize();
}

USpudSubsystem* UHawkeyeSaveSubsystem::GetSpud() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<USpudSubsystem>() : nullptr;
}

UWorld* UHawkeyeSaveSubsystem::GetGameWorld() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	return World && World->IsGameWorld() ? World : nullptr;
}

FString UHawkeyeSaveSubsystem::GetSlotName() const
{
	return SlotNameOverride.IsEmpty() ? FString(DefaultSlotName) : SlotNameOverride;
}

bool UHawkeyeSaveSubsystem::HasSave() const
{
	return IFileManager::Get().FileExists(*USpudSubsystem::GetSaveGameFilePath(GetSlotName()));
}

bool UHawkeyeSaveSubsystem::IsLoading() const
{
	return bLoadInFlight || bRestorePending;
}

// --- Saving -----------------------------------------------------------------------------------------

FString UHawkeyeSaveSubsystem::GetSaveRefusal(UWorld* World) const
{
	const USpudSubsystem* Spud = GetSpud();
	if (!World || !Spud)
	{
		return TEXT("no game world");
	}
	if (!Spud->IsIdle() || IsLoading())
	{
		return TEXT("a save or load is already under way");
	}
	const UMissionSubsystem* Missions = UMissionSubsystem::Get(World);
	if (!Missions || !Missions->GetCurrentMission())
	{
		return TEXT("no mission is running");
	}
	const APlayerController* PC = World->GetFirstPlayerController();
	const AHawkeyeCharacter* Player = PC ? Cast<AHawkeyeCharacter>(PC->GetPawn()) : nullptr;
	const UHealthComponent* Health = Player ? Player->GetHealthComponent() : nullptr;
	if (!Player || !Health)
	{
		return TEXT("no player");
	}
	if (!Health->IsAlive() || Player->IsDowned())
	{
		return TEXT("the player is down");
	}
	return FString();
}

void UHawkeyeSaveSubsystem::CaptureCampaign(UWorld* World)
{
	const UMissionSubsystem* Missions = UMissionSubsystem::Get(World);
	const UMissionTracker* Tracker = Missions ? Missions->GetTracker() : nullptr;
	Campaign->Version = UHawkeyeCampaignState::CurrentVersion;
	Campaign->MissionPath = Missions ? FSoftObjectPath(Missions->GetCurrentMission()).ToString() : FString();
	Campaign->CompletedObjectives = Tracker ? Tracker->GetCompletedObjectiveIds() : TArray<FName>();

	const APlayerController* PC = World->GetFirstPlayerController();
	const AHawkeyeCharacter* Player = PC ? Cast<AHawkeyeCharacter>(PC->GetPawn()) : nullptr;
	Campaign->ControlledCharacter = Player ? Player->GetCharacterName().ToString() : FString();
}

bool UHawkeyeSaveSubsystem::SaveCampaign(const FString& Reason)
{
	UWorld* World = GetGameWorld();
	const FString Refusal = GetSaveRefusal(World);
	if (!Refusal.IsEmpty())
	{
		UE_LOG(LogHawkeye, Log, TEXT("%s: save (%s) refused: %s."), *GetName(), *Reason, *Refusal);
		return false;
	}

	CaptureCampaign(World);
	USpudSubsystem* Spud = GetSpud();
	USpudCustomSaveInfo* Info = Spud->CreateCustomSaveInfo();
	Info->SetInt(HawkeyeSave::VersionKey, Campaign->Version);
	Info->SetString(HawkeyeSave::MissionKey, Campaign->MissionPath);
	Info->SetString(HawkeyeSave::ReasonKey, Reason);

	const UMissionDefinition* Mission = UMissionSubsystem::Get(World)->GetCurrentMission();
	const FText Title = FText::Format(NSLOCTEXT("Hawkeye", "SaveTitle", "{0} ({1})"),
		Mission->MissionName, FText::FromString(Reason));

	// Synchronous and without a thumbnail, so the save is on disk before this returns: an objective
	// autosave must land before OnMissionComplete clears the quiver.
	bLastSaveSucceeded = false;
	Spud->SaveGame(GetSlotName(), Title, /*bTakeScreenshot=*/false, Info);
	if (!bLastSaveSucceeded)
	{
		UE_LOG(LogHawkeye, Error, TEXT("%s: SPUD could not write slot %s (%s)."), *GetName(), *GetSlotName(), *Reason);
		OnCampaignSaved.Broadcast(Reason, false);
		return false;
	}

	++SaveCount;
	AutosaveClock.Reset();
	UE_LOG(LogHawkeye, Log, TEXT("%s: saved the campaign to %s (%s): %s, %d objective(s) done, playing %s."),
		*GetName(), *GetSlotName(), *Reason, *Campaign->MissionPath, Campaign->CompletedObjectives.Num(),
		*Campaign->ControlledCharacter);
	OnCampaignSaved.Broadcast(Reason, true);
	return true;
}

void UHawkeyeSaveSubsystem::HandleSpudPostSave(const FString& SlotName, bool bSuccess)
{
	if (SlotName == GetSlotName())
	{
		bLastSaveSucceeded = bSuccess;
	}
}

void UHawkeyeSaveSubsystem::AdvanceAutosave(float DeltaSeconds, bool bCanSave)
{
	if (Campaign && DeltaSeconds > 0.f)
	{
		Campaign->PlayTimeSeconds += DeltaSeconds;
	}
	if (AutosaveClock.Advance(DeltaSeconds, bCanSave))
	{
		SaveCampaign(TEXT("roaming"));
	}
}

// --- Loading ----------------------------------------------------------------------------------------

EHawkeyeLoadDecision UHawkeyeSaveSubsystem::DecideLoad(bool bSaveExists, int32 SavedVersion, const FString& InMissionPath) const
{
	const EHawkeyeLoadDecision Decision = UHawkeyeCampaignState::EvaluateLoad(bSaveExists, SavedVersion, InMissionPath);
	switch (Decision)
	{
	case EHawkeyeLoadDecision::VersionMismatch:
		UE_LOG(LogHawkeye, Warning, TEXT("%s: slot %s is save version %d, this build reads %d and has no migration; "
			"starting a new game."), *GetName(), *GetSlotName(), SavedVersion, UHawkeyeCampaignState::CurrentVersion);
		break;
	case EHawkeyeLoadDecision::StaleMission:
		UE_LOG(LogHawkeye, Error, TEXT("%s: slot %s is for mission '%s', which no longer exists; starting a new game."),
			*GetName(), *GetSlotName(), *InMissionPath);
		break;
	case EHawkeyeLoadDecision::NoSave:
		UE_LOG(LogHawkeye, Log, TEXT("%s: no save in slot %s; starting a new game."), *GetName(), *GetSlotName());
		break;
	default:
		break;
	}
	return Decision;
}

bool UHawkeyeSaveSubsystem::LoadCampaign()
{
	USpudSubsystem* Spud = GetSpud();
	if (!Spud || !GetGameWorld() || !Spud->IsIdle() || IsLoading())
	{
		UE_LOG(LogHawkeye, Warning, TEXT("%s: cannot load now (no world, or SPUD busy)."), *GetName());
		return false;
	}

	int32 SavedVersion = INDEX_NONE;
	FString SavedMission;
	const bool bExists = HasSave();
	if (USpudSaveGameInfo* Info = bExists ? Spud->GetSaveGameInfo(GetSlotName()) : nullptr)
	{
		if (Info->CustomInfo)
		{
			Info->CustomInfo->GetInt(HawkeyeSave::VersionKey, SavedVersion);
			Info->CustomInfo->GetString(HawkeyeSave::MissionKey, SavedMission);
		}
	}

	if (DecideLoad(bExists, SavedVersion, SavedMission) != EHawkeyeLoadDecision::Load)
	{
		StartNewGame();
		return false;
	}

	UE_LOG(LogHawkeye, Log, TEXT("%s: loading %s (%s)."), *GetName(), *GetSlotName(), *SavedMission);
	bLoadInFlight = true;
	bRestorePending = true;
	AutosaveClock.Reset();
	Spud->LoadGame(GetSlotName());
	return true;
}

void UHawkeyeSaveSubsystem::StartNewGame()
{
	UWorld* World = GetGameWorld();
	USpudSubsystem* Spud = GetSpud();
	Campaign->ResetCampaign();
	AutosaveClock.Reset();
	bRestorePending = false;
	bLoadInFlight = false;
	bNewGameAutosavePending = true;
	bBootMenuConsumed = true;
	if (!World || !Spud)
	{
		return;
	}
	Spud->NewGame(/*bCheckServerOnly=*/false, /*bAfterLevelLoad=*/true);
	const FString Level = UGameplayStatics::GetCurrentLevelName(World, /*bRemovePrefixString=*/true);
	UE_LOG(LogHawkeye, Log, TEXT("%s: new game; reopening %s."), *GetName(), *Level);
	UGameplayStatics::OpenLevel(World, FName(*Level));
}

bool UHawkeyeSaveSubsystem::ConsumeNewGameAutosave()
{
	const bool bPending = bNewGameAutosavePending;
	bNewGameAutosavePending = false;
	return bPending;
}

void UHawkeyeSaveSubsystem::HandlePreLoadMap(const FString& MapName)
{
	// SPUD keeps level state across every travel unless told otherwise. Only its own loads should
	// restore anything: a restart, a test reopening the map, or a new game must start clean.
	USpudSubsystem* Spud = GetSpud();
	if (Spud && !Spud->IsLoadingGame())
	{
		Spud->NewGame(/*bCheckServerOnly=*/false, /*bAfterLevelLoad=*/true);
	}
}

void UHawkeyeSaveSubsystem::ApplyPendingRestore(UWorld* World)
{
	if (!bRestorePending || !World)
	{
		return;
	}
	bRestorePending = false;

	UMissionSubsystem* Missions = UMissionSubsystem::Get(World);
	UMissionTracker* Tracker = Missions ? Missions->GetTracker() : nullptr;
	if (!Tracker)
	{
		return;
	}
	const FString Running = FSoftObjectPath(Missions->GetCurrentMission()).ToString();
	if (Running != Campaign->MissionPath)
	{
		// A save from another chapter's map: start the saved chapter instead of the map's own.
		UMissionDefinition* Saved = LoadObject<UMissionDefinition>(nullptr, *Campaign->MissionPath);
		if (!Saved || !Missions->StartMission(Saved))
		{
			UE_LOG(LogHawkeye, Error, TEXT("%s: could not start saved mission '%s'."), *GetName(), *Campaign->MissionPath);
			return;
		}
	}
	const int32 Restored = Tracker->RestoreCompletedObjectives(Campaign->CompletedObjectives);
	UE_LOG(LogHawkeye, Log, TEXT("%s: restored %d completed objective(s) of %s."), *GetName(), Restored, *Campaign->MissionPath);
}

void UHawkeyeSaveSubsystem::HandleSpudPostLoad(const FString& SlotName, bool bSuccess)
{
	if (!bLoadInFlight || SlotName != GetSlotName())
	{
		return;
	}
	bLoadInFlight = false;
	UWorld* World = GetGameWorld();

	if (!bSuccess)
	{
		UE_LOG(LogHawkeye, Error, TEXT("%s: SPUD failed to load %s; starting a new game."), *GetName(), *SlotName);
		bRestorePending = false;
		OnCampaignLoaded.Broadcast(false);
		StartNewGame();
		return;
	}

	// The map had no game mode to hand the objectives to (a test map, say): apply them here.
	ApplyPendingRestore(World);
	FinishRestoredWorld(World);
	UE_LOG(LogHawkeye, Log, TEXT("%s: loaded %s."), *GetName(), *SlotName);
	OnCampaignLoaded.Broadcast(true);
}

void UHawkeyeSaveSubsystem::FinishRestoredWorld(UWorld* World)
{
	AHawkeyePlayerController* PC = World ? Cast<AHawkeyePlayerController>(World->GetFirstPlayerController()) : nullptr;
	if (!PC)
	{
		return;
	}

	// The game mode spawns Kate for the player; if the save was made playing Clint, swap back.
	const AHawkeyeCharacter* Current = Cast<AHawkeyeCharacter>(PC->GetPawn());
	if (Current && !Campaign->ControlledCharacter.IsEmpty()
		&& Current->GetCharacterName().ToString() != Campaign->ControlledCharacter)
	{
		const bool bOverride = PC->bAllowSwitchingOverride;
		PC->bAllowSwitchingOverride = true;
		PC->SwitchCharacter();
		PC->bAllowSwitchingOverride = bOverride;
	}

	if (PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, FadeInSeconds, FLinearColor::Black, false, false);
	}
}

// --- Safehouses -------------------------------------------------------------------------------------

bool UHawkeyeSaveSubsystem::DiscoverSafehouse(FName SafehouseId)
{
	if (SafehouseId.IsNone() || Campaign->DiscoveredSafehouses.Contains(SafehouseId))
	{
		return false;
	}
	Campaign->DiscoveredSafehouses.Add(SafehouseId);
	UE_LOG(LogHawkeye, Log, TEXT("%s: safehouse %s discovered."), *GetName(), *SafehouseId.ToString());
	return true;
}

bool UHawkeyeSaveSubsystem::IsSafehouseDiscovered(FName SafehouseId) const
{
	return Campaign && Campaign->DiscoveredSafehouses.Contains(SafehouseId);
}

TArray<FName> UHawkeyeSaveSubsystem::GetDiscoveredSafehouses() const
{
	return Campaign ? Campaign->DiscoveredSafehouses : TArray<FName>();
}

// --- Main menu --------------------------------------------------------------------------------------

bool UHawkeyeSaveSubsystem::ConsumeBootMenu()
{
	if (bBootMenuConsumed)
	{
		return false;
	}
	bBootMenuConsumed = true;

	// Automation drives the game itself; a menu pausing the first world would only get in its way.
	const TCHAR* CommandLine = FCommandLine::Get();
	if (FParse::Param(CommandLine, TEXT("NoMainMenu")) || FCString::Stristr(CommandLine, TEXT("RunTests")) != nullptr
		|| GIsAutomationTesting)
	{
		return false;
	}
	return !IsLoading();
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/CastleNavigationLibrary.h"

#include "Castle.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "NavigationSystem.h"

bool UCastleNavigationLibrary::BuildNavigationNow(UObject* WorldContext)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(World);
	if (!NavSystem)
	{
		return false;
	}

	// An editor world adds AsyncLoadLock at load and waits for a core ticker to remove it; a
	// commandlet never ticks, so Build() would refuse forever without this.
	NavSystem->RemoveNavigationBuildLock(ENavigationBuildLock::AsyncLoadLock, UNavigationSystemV1::ELockRemovalRebuildAction::NoRebuild);
	NavSystem->Build();

	const bool bBuilt = NavSystem->GetDefaultNavDataInstance() != nullptr;
	UE_LOG(LogCastle, Log, TEXT("%s: navigation built (%s)."), *GetNameSafe(World),
		bBuilt ? *GetNameSafe(NavSystem->GetDefaultNavDataInstance()) : TEXT("no nav data"));
	return bBuilt;
}

bool UCastleNavigationLibrary::ProjectToNavigation(UObject* WorldContext, FVector Point, FVector Extent, FVector& OutProjected)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	const UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(World);
	FNavLocation Location;
	if (!NavSystem || !NavSystem->ProjectPointToNavigation(Point, Location, Extent))
	{
		OutProjected = Point;
		return false;
	}
	OutProjected = Location.Location;
	return true;
}

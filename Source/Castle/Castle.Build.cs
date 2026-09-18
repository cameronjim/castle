// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Castle : ModuleRules
{
	public Castle(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Lets every header be included relative to Source/Castle
		// e.g. #include "Mission/MissionSubsystem.h"
		PublicIncludePaths.AddRange(new string[] { "Castle" });

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"UMG",
			"Slate",
			"SlateCore",
			"AIModule",
			"NavigationSystem",
			"GameplayTasks"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });
	}
}

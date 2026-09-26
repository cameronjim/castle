// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Hawkeye : ModuleRules
{
	public Hawkeye(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Lets every header be included relative to Source/Hawkeye
		// e.g. #include "Mission/MissionSubsystem.h"
		PublicIncludePaths.AddRange(new string[] { "Hawkeye" });

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
			"GameplayTasks",
			// The partner's brain: ST_Partner runs on a StateTreeAIComponent.
			"StateTreeModule",
			"GameplayStateTreeModule",
			"GameplayTags"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		// UHawkeyePartnerTreeBuilder authors ST_Partner headless through the StateTree editor API.
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "StateTreeEditorModule", "PropertyBindingUtils", "UnrealEd" });
		}
	}
}

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
			"GameplayTags",
			// Campaign save: Plugins/SPUD (sinbad/SPUD, MIT) persists ISpudObject actors and globals.
			"SPUD"
		});

		// Hawkeye.Audio.Smoke finds the MetaSounds through the asset registry.
		PrivateDependencyModuleNames.AddRange(new string[] { "AssetRegistry" });

		// UHawkeyePartnerTreeBuilder authors ST_Partner headless through the StateTree editor API;
		// UHawkeyeBowIKGraphBuilder authors the bow hands AnimBlueprints through the anim graph nodes.
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "StateTreeEditorModule", "PropertyBindingUtils", "UnrealEd",
				"AnimGraph", "AnimGraphRuntime", "BlueprintGraph" });
		}
	}
}

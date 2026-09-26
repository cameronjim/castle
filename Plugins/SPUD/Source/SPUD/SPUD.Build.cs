using UnrealBuildTool;
using System.IO;
// ReSharper disable ConditionIsAlwaysTrueOrFalse

public class SPUD : ModuleRules
{
	public SPUD(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
            new string[] {
            }
            );


        PrivateIncludePaths.AddRange(
            new string[] {
            }
            );

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine"
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
			}
			);

		if (Target.Version.MajorVersion == 5 && Target.Version.MinorVersion < 5)
		{
			// FInstancedStruct moved into CoreUObject in UE5.5; the standalone
			// StructUtils module is only needed on earlier engines.
			PrivateDependencyModuleNames.Add("StructUtils");
		}
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
			);
	}
}

using System.IO;
using UnrealBuildTool;

public class UnrealMCPLogicDriver : ModuleRules
{
	public UnrealMCPLogicDriver(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		IWYUSupport = IWYUSupport.Full;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Json",
				"UnrealMCP",
				"SMSystem",
				"SMSystemEditor"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"UnrealEd",
				"EditorScriptingUtilities",
				"BlueprintGraph"
			}
		);

		PrivateIncludePaths.Add(
			Path.Combine(GetModuleDirectory("SMSystemEditor"), "Private")
		);
	}
}

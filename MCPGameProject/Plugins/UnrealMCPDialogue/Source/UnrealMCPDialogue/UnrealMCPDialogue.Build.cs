using UnrealBuildTool;

public class UnrealMCPDialogue : ModuleRules
{
	public UnrealMCPDialogue(ReadOnlyTargetRules Target) : base(Target)
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
				"DialogueSystem",
				"DialogueSystemEditor",
				"OFStateGraphCore",
				"OFStateGraphCoreEditor"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"UnrealEd",
				"EditorScriptingUtilities",
				"AssetTools",
				"BlueprintGraph"
			}
		);
	}
}

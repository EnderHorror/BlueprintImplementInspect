using UnrealBuildTool;

public class BlueprintImplementInspect : ModuleRules
{
	public BlueprintImplementInspect(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AssetRegistry",
				"BlueprintGraph",
				"ClassViewer",
				"ContentBrowser",
				"CoreUObject",
				"EditorFramework",
				"Engine",
				"GraphEditor",
				"InputCore",
				"Kismet",
				"PropertyEditor",
				"Slate",
				"SlateCore",
				"ToolMenus",
				"UnrealEd"
			}
		);
	}
}

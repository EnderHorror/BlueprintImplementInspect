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
				"ApplicationCore",
				"AssetRegistry",
				"BlueprintGraph",
				"ClassViewer",
				"ContentBrowser",
				"CoreUObject",
				"DesktopPlatform",
				"EditorFramework",
				"Engine",
				"GraphEditor",
				"InputCore",
				"Kismet",
				"PropertyEditor",
				"Projects",
				"Slate",
				"SlateCore",
				"ToolMenus",
				"UnrealEd"
			}
		);
	}
}

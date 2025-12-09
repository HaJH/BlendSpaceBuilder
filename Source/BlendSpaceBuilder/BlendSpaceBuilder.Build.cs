using UnrealBuildTool;

public class BlendSpaceBuilder : ModuleRules
{
	public BlendSpaceBuilder(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"InputCore",
				"UnrealEd",
				"ToolMenus",
				"AssetTools",
				"ContentBrowser",
				"AssetRegistry",
				"DeveloperSettings",
				"Projects",
				"EditorSubsystem",
				"Persona",
				"AnimationModifiers",
				"AnimationBlueprintLibrary",
				"AnimationEditor",
				"ClassViewer",
				"PropertyEditor",
			}
		);
	}
}

using UnrealBuildTool;

public class RZCombatEditor : ModuleRules
{
	public RZCombatEditor(ReadOnlyTargetRules Target) : base(Target)
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
				"AssetTools",
				"CoreUObject",
				"EditorFramework",
				"Engine",
				"GameplayAbilities",
				"GameplayTags",
				"GraphEditor",
				"InputCore",
				"PropertyEditor",
				"RZCombatRuntime",
				"Slate",
				"SlateCore",
				"ToolMenus",
				"UnrealEd"
			}
		);
	}
}

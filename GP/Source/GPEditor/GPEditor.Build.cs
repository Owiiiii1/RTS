// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GPEditor : ModuleRules
{
	public GPEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Slate",
			"SlateCore",
			"InputCore",
			"UnrealEd",
			"LevelEditor",
			"ToolMenus",
			"AssetRegistry",
			"NavigationSystem",
			"GPRuntime"
		});
	}
}

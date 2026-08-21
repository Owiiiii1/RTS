// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GPUIRuntime : ModuleRules
{
	public GPUIRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"DeveloperSettings",
			"UMG",
			"Slate",
			"SlateCore",
			"GameplayAbilities",
			"GameplayTags",
			"CommonUI",
			"CommonInput",
			"ModelViewViewModel",
			"GPRuntime",
			"GPGASRuntime"
		});
	}
}

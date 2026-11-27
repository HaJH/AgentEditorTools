// Copyright 2026 Jeonghyeon Ha. All Rights Reserved.

using UnrealBuildTool;

public class AgentEditorTools : ModuleRules
{
	public AgentEditorTools(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"AssetTools",
			"Json",
			"JsonUtilities",
			"MaterialEditor",
			"Slate",
			"SlateCore",
			"UnrealEd"
		});

		if (Target.Configuration != UnrealTargetConfiguration.Test && Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PrivateDependencyModuleNames.Add("CQTest");
		}
	}
}

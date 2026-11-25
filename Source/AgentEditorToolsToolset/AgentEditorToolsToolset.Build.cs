// Copyright 2026 Jeonghyeon Ha. All Rights Reserved.

using UnrealBuildTool;

public class AgentEditorToolsToolset : ModuleRules
{
	public AgentEditorToolsToolset(ReadOnlyTargetRules Target) : base(Target)
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
			"ToolsetRegistry"
		});
	}
}

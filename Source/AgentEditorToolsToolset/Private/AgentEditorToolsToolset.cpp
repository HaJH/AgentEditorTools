// Copyright 2026 Jeonghyeon Ha. All Rights Reserved.

#include "AgentEditorToolsToolset.h"

#include "AgentEditorToolset.h"
#include "ToolsetRegistry/UToolsetRegistry.h"

void FAgentEditorToolsToolsetModule::StartupModule()
{
	UToolsetRegistry::RegisterToolsetClass(UAgentEditorToolset::StaticClass());
}

void FAgentEditorToolsToolsetModule::ShutdownModule()
{
	UToolsetRegistry::UnregisterToolsetClass(UAgentEditorToolset::StaticClass());
}

IMPLEMENT_MODULE(FAgentEditorToolsToolsetModule, AgentEditorToolsToolset)

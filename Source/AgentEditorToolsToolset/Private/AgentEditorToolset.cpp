// Copyright 2026 Jeonghyeon Ha. All Rights Reserved.

#include "AgentEditorToolset.h"

#include "AgentGraphImportLibrary.h"
#include "AgentMaterialInstanceStateLibrary.h"

FString UAgentEditorToolset::ImportGraph(const FString& AssetPath, const FString& T3DFilePath, bool bReplace, bool bSave)
{
	return UAgentGraphImportLibrary::GraphImportResultToJson(UAgentGraphImportLibrary::ImportGraphFile(AssetPath, T3DFilePath, bReplace, bSave));
}

FString UAgentEditorToolset::ApplyMaterialInstanceState(const FString& StateFilePath, bool bWrite)
{
	return UAgentMaterialInstanceStateLibrary::MaterialInstanceStateResultToJson(UAgentMaterialInstanceStateLibrary::ApplyStateFile(StateFilePath, bWrite));
}

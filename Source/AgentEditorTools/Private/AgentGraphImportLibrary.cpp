// Copyright 2026 Jeonghyeon Ha. All Rights Reserved.

#include "AgentGraphImportLibrary.h"

#include "AgentGraphImporter.h"
#include "JsonObjectConverter.h"

FAgentGraphImportResult UAgentGraphImportLibrary::ImportGraphText(UObject* Asset, const FString& Text, bool bReplace, bool bSave)
{
	return FAgentGraphImporter::Import(Asset, Text, bReplace, bSave);
}

FString UAgentGraphImportLibrary::GraphImportResultToJson(const FAgentGraphImportResult& Result)
{
	FString Json;
	FJsonObjectConverter::UStructToJsonObjectString(Result, Json);
	return Json;
}

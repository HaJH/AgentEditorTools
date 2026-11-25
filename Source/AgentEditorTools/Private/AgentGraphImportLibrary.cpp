// Copyright 2026 Jeonghyeon Ha. All Rights Reserved.

#include "AgentGraphImportLibrary.h"

#include "AgentGraphImporter.h"
#include "JsonObjectConverter.h"

namespace AgentGraphImportLibraryLocal
{

static constexpr TCHAR JsonConversionFailedFormat[] = TEXT("Converting the graph import result for %s to JSON failed");

}

FAgentGraphImportResult UAgentGraphImportLibrary::ImportGraphText(UObject* Asset, const FString& Text, bool bReplace, bool bSave)
{
	return FAgentGraphImporter::Import(Asset, Text, bReplace, bSave);
}

FAgentGraphImportResult UAgentGraphImportLibrary::ImportGraphFile(const FString& AssetPath, const FString& T3DFilePath, bool bReplace, bool bSave)
{
	return FAgentGraphImporter::ImportFile(AssetPath, T3DFilePath, bReplace, bSave);
}

FString UAgentGraphImportLibrary::GraphImportResultToJson(const FAgentGraphImportResult& Result)
{
	FString Json;
	verifyf(FJsonObjectConverter::UStructToJsonObjectString(Result, Json), AgentGraphImportLibraryLocal::JsonConversionFailedFormat, *Result.Asset);
	return Json;
}

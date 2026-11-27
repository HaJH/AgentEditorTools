// Copyright 2026 Jeonghyeon Ha. All Rights Reserved.

#include "AgentMaterialInstanceStateLibrary.h"

#include "AgentMaterialInstanceStateApplier.h"
#include "JsonObjectConverter.h"

namespace AgentMaterialInstanceStateLibraryLocal
{

static constexpr TCHAR JsonConversionFailedFormat[] = TEXT("Converting the material instance state result for %s to JSON failed");

}

FAgentMaterialInstanceStateResult UAgentMaterialInstanceStateLibrary::ApplyStateFile(const FString& StateFilePath, bool bWrite)
{
	return FAgentMaterialInstanceStateApplier::ApplyFile(StateFilePath, bWrite);
}

FString UAgentMaterialInstanceStateLibrary::MaterialInstanceStateResultToJson(const FAgentMaterialInstanceStateResult& Result)
{
	FString Json;
	verifyf(FJsonObjectConverter::UStructToJsonObjectString(Result, Json), AgentMaterialInstanceStateLibraryLocal::JsonConversionFailedFormat, *Result.Instance);
	return Json;
}

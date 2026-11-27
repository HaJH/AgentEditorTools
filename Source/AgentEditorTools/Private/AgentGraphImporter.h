// Copyright 2026 Jeonghyeon Ha. All Rights Reserved.

#pragma once

#include "AgentGraphImportResult.h"
#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "SceneTypes.h"

class UObject;

class FAgentGraphImporter
{
public:
	static FAgentGraphImportResult Import(UObject* Asset, const FString& Text, bool bReplace, bool bSave);
	static FAgentGraphImportResult ImportFile(const FString& AssetPath, const FString& T3DFilePath, bool bReplace, bool bSave);

	static bool MatchesRootPinSourceName(FName RequestedPinName, const FString& ActualPinName, const FString& InputDisplayName, const FString& InputSourceName);

	static FString GetEnglishRootInputName(EMaterialProperty Property, FMaterialShadingModelField ShadingModels);
};

// Copyright 2026 Jeonghyeon Ha. All Rights Reserved.

#pragma once

#include "AgentGraphImportResult.h"
#include "CoreMinimal.h"

class UObject;

class FAgentGraphImporter
{
public:
	static FAgentGraphImportResult Import(UObject* Asset, const FString& Text, bool bReplace, bool bSave);
};

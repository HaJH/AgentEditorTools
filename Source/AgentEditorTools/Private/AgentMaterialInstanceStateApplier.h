// Copyright 2026 Jeonghyeon Ha. All Rights Reserved.

#pragma once

#include "AgentMaterialInstanceStateResult.h"
#include "CoreMinimal.h"

struct FAgentMaterialInstanceState;

class FAgentMaterialInstanceStateApplier
{
public:
	static FAgentMaterialInstanceStateResult ApplyFile(const FString& StateFilePath, bool bWrite);
	static FAgentMaterialInstanceStateResult Apply(const FAgentMaterialInstanceState& State, bool bWrite);
};

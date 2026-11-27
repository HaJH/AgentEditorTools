// Copyright 2026 Jeonghyeon Ha. All Rights Reserved.

#pragma once

#include "AgentMaterialInstanceStateResult.h"
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "AgentMaterialInstanceStateLibrary.generated.h"

UCLASS()
class AGENTEDITORTOOLS_API UAgentMaterialInstanceStateLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Agent Editor Tools|Material Instance State")
	static FAgentMaterialInstanceStateResult ApplyStateFile(const FString& StateFilePath, bool bWrite);

	UFUNCTION(BlueprintCallable, Category = "Agent Editor Tools|Material Instance State")
	static FString MaterialInstanceStateResultToJson(const FAgentMaterialInstanceStateResult& Result);
};

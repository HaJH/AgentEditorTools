// Copyright 2026 Jeonghyeon Ha. All Rights Reserved.

#pragma once

#include "AgentGraphImportResult.h"
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "AgentGraphImportLibrary.generated.h"

UCLASS()
class AGENTEDITORTOOLS_API UAgentGraphImportLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Agent Editor Tools|Graph Import")
	static FAgentGraphImportResult ImportGraphText(UObject* Asset, const FString& Text, bool bReplace, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "Agent Editor Tools|Graph Import")
	static FString GraphImportResultToJson(const FAgentGraphImportResult& Result);
};

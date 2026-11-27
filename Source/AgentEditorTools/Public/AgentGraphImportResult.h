// Copyright 2026 Jeonghyeon Ha. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "AgentGraphImportResult.generated.h"

UENUM(BlueprintType)
enum class EAgentGraphImportDiagnosticKind : uint8
{
	AssetNotFound,
	UnsupportedAsset,
	EditorOpen,
	FileReadFailed,
	EmptyImport,
	RootInFunction,
	UnmatchedRootPin,
	UnmatchedFunctionIO,
	NoRoot,
	CompileError,
	SaveFailed,
	MissingEntries
};

USTRUCT(BlueprintType)
struct FAgentGraphImportDiagnostic
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Agent Graph Import")
	EAgentGraphImportDiagnosticKind Kind = EAgentGraphImportDiagnosticKind::AssetNotFound;

	UPROPERTY(BlueprintReadOnly, Category = "Agent Graph Import")
	FString Detail;
};

USTRUCT(BlueprintType)
struct FAgentGraphImportResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Agent Graph Import")
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "Agent Graph Import")
	FString Asset;

	UPROPERTY(BlueprintReadOnly, Category = "Agent Graph Import")
	TArray<FString> CreatedExpressions;

	UPROPERTY(BlueprintReadOnly, Category = "Agent Graph Import")
	bool bSaved = false;

	UPROPERTY(BlueprintReadOnly, Category = "Agent Graph Import")
	TArray<FAgentGraphImportDiagnostic> Diagnostics;
};

// Copyright 2026 Jeonghyeon Ha. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "AgentMaterialInstanceStateResult.generated.h"

UENUM(BlueprintType)
enum class EAgentMaterialInstanceStateDiagnosticKind : uint8
{
	FileReadFailed,
	InvalidState,
	InstanceNotFound,
	ParentNotFound,
	NotAnInstance,
	EditorOpen,
	UnknownParameter,
	TextureNotFound,
	UnsupportedSampler,
	SamplerSwitchNotFound,
	SamplerConflict,
	CreateFailed,
	SaveFailed
};

USTRUCT(BlueprintType)
struct FAgentMaterialInstanceStateDiagnostic
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Agent Material Instance State")
	EAgentMaterialInstanceStateDiagnosticKind Kind = EAgentMaterialInstanceStateDiagnosticKind::FileReadFailed;

	UPROPERTY(BlueprintReadOnly, Category = "Agent Material Instance State")
	FString Detail;
};

USTRUCT(BlueprintType)
struct FAgentMaterialInstanceStateResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Agent Material Instance State")
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "Agent Material Instance State")
	FString Label;

	UPROPERTY(BlueprintReadOnly, Category = "Agent Material Instance State")
	FString Instance;

	UPROPERTY(BlueprintReadOnly, Category = "Agent Material Instance State")
	bool bCreated = false;

	UPROPERTY(BlueprintReadOnly, Category = "Agent Material Instance State")
	bool bWritten = false;

	UPROPERTY(BlueprintReadOnly, Category = "Agent Material Instance State")
	int32 Writes = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Agent Material Instance State")
	TArray<FString> SamplerSwitches;

	UPROPERTY(BlueprintReadOnly, Category = "Agent Material Instance State")
	bool bSaved = false;

	UPROPERTY(BlueprintReadOnly, Category = "Agent Material Instance State")
	TArray<FAgentMaterialInstanceStateDiagnostic> Diagnostics;
};

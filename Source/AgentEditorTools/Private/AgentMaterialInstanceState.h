// Copyright 2026 Jeonghyeon Ha. All Rights Reserved.

#pragma once

#include "AgentMaterialInstanceStateResult.h"
#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

enum class EAgentMaterialInstanceWriteKind : uint8
{
	Switch,
	Scalar,
	Vector,
	Texture
};

struct FAgentMaterialInstanceSamplerSwitch
{
	EMaterialSamplerType SamplerType = SAMPLERTYPE_Color;

	/** NAME_None when this sampler type is read with every listed switch off. */
	FName Switch;
};

struct FAgentMaterialInstanceWrite
{
	FName Name;
	EAgentMaterialInstanceWriteKind Kind = EAgentMaterialInstanceWriteKind::Switch;
	bool bSwitchValue = false;
	float ScalarValue = 0.0f;
	FLinearColor VectorValue = FLinearColor::Black;
	FString TexturePath;
	bool bHasSamplers = false;
	TArray<FAgentMaterialInstanceSamplerSwitch> Samplers;
};

struct FAgentMaterialInstanceState
{
	FString Label;
	FString Instance;
	FString Parent;
	bool bCreate = false;
	TArray<FAgentMaterialInstanceWrite> Writes;

	static bool ParseJson(const FString& JsonText, FAgentMaterialInstanceState& OutState, TArray<FAgentMaterialInstanceStateDiagnostic>& OutDiagnostics);
};

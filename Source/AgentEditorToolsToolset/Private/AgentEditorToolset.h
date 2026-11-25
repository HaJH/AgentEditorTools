// Copyright 2026 Jeonghyeon Ha. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "AgentEditorToolset.generated.h"

UCLASS()
class UAgentEditorToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/** Imports material editor T3D from the file at an absolute path into a material or material function in one call, without opening an asset editor, and returns the result as JSON.
	 * Refuses to change the asset while an asset editor for it is open.
	 * diagnostics[].kind is one of: AssetNotFound, UnsupportedAsset, EditorOpen, FileReadFailed, EmptyImport, RootInFunction, UnmatchedRootPin, NoRoot, CompileError, SaveFailed. */
	UFUNCTION(meta = (AICallable), Category = "AgentEditorTools")
	static FString ImportGraph(const FString& AssetPath, const FString& T3DFilePath, bool bReplace, bool bSave = true);
};

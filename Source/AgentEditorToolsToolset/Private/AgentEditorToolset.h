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
	 * AssetPath takes an object path (/Game/Dir/M.M) or a package path (/Game/Dir/M).
	 * Refuses to change the asset while an asset editor for it is open.
	 * When bReplace is true, deletes every existing expression before importing. When bSave is true, saves only that one asset.
	 * When bReplace replaces a material function, each imported function input or output keeps the id of the previous one with the same name, so function calls keep their connections; a previous name the text lacks is reported as UnmatchedFunctionIO.
	 * Expressions or comments the asset still lists but that no longer exist are dropped before the import and reported as MissingEntries; the nodes they stood for were already lost.
	 * bSucceeded tells whether the import succeeded. UnmatchedRootPin, UnmatchedFunctionIO, NoRoot, CompileError and MissingEntries are warnings: they appear in diagnostics but do not fail the import.
	 * diagnostics[].kind is one of: AssetNotFound, UnsupportedAsset, EditorOpen, FileReadFailed, EmptyImport, RootInFunction, UnmatchedRootPin, UnmatchedFunctionIO, NoRoot, CompileError, SaveFailed, MissingEntries. */
	UFUNCTION(meta = (AICallable), Category = "AgentEditorTools")
	static FString ImportGraph(const FString& AssetPath, const FString& T3DFilePath, bool bReplace, bool bSave = true);
};

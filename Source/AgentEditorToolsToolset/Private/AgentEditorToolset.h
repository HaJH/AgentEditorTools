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

	/** Applies the target state of one material instance constant, read from a JSON file at an absolute path, in one call, and returns the result as JSON.
	 * The file is one object: {"label": string, "instance": object path of the instance, "parent": object path of the parent material (only used when the instance is created; may be omitted otherwise), "create": bool (whether a missing instance may be created), "writes": [...]}.
	 * Each write is {"name": parameter name, "kind": "switch" | "scalar" | "vector" | "texture", "value": ...}; the value is a bool for switch, a number for scalar, an array of four numbers [r, g, b, a] for vector and a texture object path for texture.
	 * A texture write may add "samplers": an object whose keys are sampler types (EMaterialSamplerType names without the SAMPLERTYPE_ prefix: Color, Grayscale, Alpha, Normal, Masks, DistanceFieldFont, LinearColor, LinearGrayscale, Data, External, VirtualColor, VirtualGrayscale, VirtualAlpha, VirtualNormal, VirtualMasks, VirtualLinearColor, VirtualLinearGrayscale) and whose values name the static switch that makes the parameter read through that sampler, or null for the sampler it reads with every listed switch off.
	 * The texture's sampler type is the engine's default sampler for it (compression, sRGB, virtual texturing). Every switch listed in samplers is set by the tool: the one for the texture's sampler type is turned on and the others are turned off (all off when that type maps to null). These sampler switch values are written after all writes and win over a write of the same switch. A texture write without samplers does not look at samplers.
	 * Only global parameters are read and written; material layer parameters are not supported.
	 * Order: check everything, then (only when bWrite is true and the check found no problem) create the instance under the parent if it is missing, clear every override on it, write the values in the listed order without updating the instance per value, write the sampler switches, update the instance once, and save only that instance's package. Shader compilation is not waited for.
	 * The check collects every problem before returning. Parameter names are looked up on the instance when it exists, otherwise on the parent. When the check reports any diagnostic nothing is created, changed or saved.
	 * bWrite false only checks: nothing is created, changed or saved even when the check finds no problem, and bCreated, bWritten and bSaved are false. Use it to check many state files before writing any of them.
	 * Result fields: bSucceeded (no diagnostics), label, instance, bCreated, bWritten, writes (number of writes applied; 0 when nothing was written), samplerSwitches (sampler switches turned on; switches turned off are not listed; empty when nothing was written), bSaved, diagnostics[{kind, detail}].
	 * diagnostics[].kind is one of: FileReadFailed (the file cannot be read), InvalidState (the JSON does not have the shape above: missing key, unknown kind, value of the wrong type, unknown sampler type, samplers on a non-texture write, instance that is not an asset path), InstanceNotFound (the instance is missing and create is false), ParentNotFound (the instance is missing and the parent is missing or not a material), NotAnInstance (the instance path holds an asset that is not a material instance constant), EditorOpen (an asset editor for the instance is open; close it first), UnknownParameter (the name is not a global parameter of that kind), TextureNotFound, UnsupportedSampler (the texture's sampler type is not a key of samplers), SamplerSwitchNotFound (a switch named in samplers is not a static switch parameter), SamplerConflict (two texture writes need the same sampler switch on and off), CreateFailed, SaveFailed. */
	UFUNCTION(meta = (AICallable), Category = "AgentEditorTools")
	static FString ApplyMaterialInstanceState(const FString& StateFilePath, bool bWrite);
};

// Copyright 2026 Jeonghyeon Ha. All Rights Reserved.

#include "AgentMaterialInstanceStateApplier.h"

#include "AgentEditorAssetUtils.h"
#include "AgentMaterialInstanceState.h"
#include "Engine/EngineTypes.h"
#include "Engine/Texture.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "IAssetTools.h"
#include "MaterialEditingLibrary.h"
#include "Materials/MaterialExpressionUtils.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameters.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "UObject/Class.h"
#include "UObject/ReflectedTypeAccessors.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace AgentMaterialInstanceStateApplierLocal
{

static constexpr TCHAR FileReadFailedFormat[] = TEXT("Reading the state file '%s' failed");
static constexpr TCHAR NotAnInstanceFormat[] = TEXT("%s is a %s, not a material instance constant");
static constexpr TCHAR EditorOpenFormat[] = TEXT("An asset editor for %s is open");
static constexpr TCHAR InstanceNotFoundFormat[] = TEXT("No material instance was found at '%s' and the state does not allow creating it");
static constexpr TCHAR ParentNotFoundFormat[] = TEXT("No material was found at parent '%s' to create %s under");
static constexpr TCHAR UnknownParameterFormat[] = TEXT("%s is not a global %s parameter of %s");
static constexpr TCHAR TextureNotFoundFormat[] = TEXT("No texture was found at '%s' for %s");
static constexpr TCHAR UnsupportedSamplerFormat[] = TEXT("%s reads through the %s sampler, which the samplers of %s do not list");
static constexpr TCHAR SamplerSwitchNotFoundFormat[] = TEXT("Sampler switch %s listed by %s is not a global StaticSwitch parameter of %s");
static constexpr TCHAR SamplerConflictFormat[] = TEXT("Sampler switch %s is turned %s by %s and %s by %s");
static constexpr TCHAR CreateFailedFormat[] = TEXT("Creating a material instance constant at '%s' failed");
static constexpr TCHAR SaveFailedFormat[] = TEXT("Saving package %s failed");
static constexpr TCHAR SamplerTypePrefix[] = TEXT("SAMPLERTYPE_");
static constexpr TCHAR OnText[] = TEXT("on");
static constexpr TCHAR OffText[] = TEXT("off");

struct FResolvedTarget
{
	UMaterialInstanceConstant* Instance = nullptr;
	UMaterialInterface* Parent = nullptr;
	UMaterialInterface* ParameterSource = nullptr;
};

struct FParameterNames
{
	TSet<FName> Switches;
	TSet<FName> Scalars;
	TSet<FName> Vectors;
	TSet<FName> Textures;

	const TSet<FName>& Of(EAgentMaterialInstanceWriteKind Kind) const
	{
		switch (Kind)
		{
		case EAgentMaterialInstanceWriteKind::Switch:
			return Switches;
		case EAgentMaterialInstanceWriteKind::Scalar:
			return Scalars;
		case EAgentMaterialInstanceWriteKind::Vector:
			return Vectors;
		case EAgentMaterialInstanceWriteKind::Texture:
			return Textures;
		}
		checkNoEntry();
		return Switches;
	}
};

struct FPlannedSamplerSwitch
{
	FName Switch;
	bool bValue = false;
	FName SetBy;
};

struct FCheckedWrites
{
	TArray<UTexture*> Textures;
	TArray<FPlannedSamplerSwitch> SamplerSwitches;
};

static void AddDiagnostic(FAgentMaterialInstanceStateResult& Result, EAgentMaterialInstanceStateDiagnosticKind Kind, const FString& Detail)
{
	FAgentMaterialInstanceStateDiagnostic& Diagnostic = Result.Diagnostics.AddDefaulted_GetRef();
	Diagnostic.Kind = Kind;
	Diagnostic.Detail = Detail;
}

static FAgentMaterialInstanceStateResult Finish(FAgentMaterialInstanceStateResult& Result)
{
	Result.bSucceeded = Result.Diagnostics.IsEmpty();
	return MoveTemp(Result);
}

static EMaterialParameterType ToParameterType(EAgentMaterialInstanceWriteKind Kind)
{
	switch (Kind)
	{
	case EAgentMaterialInstanceWriteKind::Switch:
		return EMaterialParameterType::StaticSwitch;
	case EAgentMaterialInstanceWriteKind::Scalar:
		return EMaterialParameterType::Scalar;
	case EAgentMaterialInstanceWriteKind::Vector:
		return EMaterialParameterType::Vector;
	case EAgentMaterialInstanceWriteKind::Texture:
		return EMaterialParameterType::Texture;
	}
	checkNoEntry();
	return EMaterialParameterType::None;
}

static TSet<FName> CollectGlobalParameterNames(const UMaterialInterface& Source, EMaterialParameterType Type)
{
	TArray<FMaterialParameterInfo> ParameterInfos;
	TArray<FGuid> ParameterIds;
	Source.GetAllParameterInfoOfType(Type, ParameterInfos, ParameterIds);

	TSet<FName> Names;
	for (const FMaterialParameterInfo& ParameterInfo : ParameterInfos)
	{
		if (ParameterInfo.Association == EMaterialParameterAssociation::GlobalParameter)
		{
			Names.Add(ParameterInfo.Name);
		}
	}
	return Names;
}

static FParameterNames CollectParameterNames(const UMaterialInterface& Source)
{
	FParameterNames Names;
	Names.Switches = CollectGlobalParameterNames(Source, EMaterialParameterType::StaticSwitch);
	Names.Scalars = CollectGlobalParameterNames(Source, EMaterialParameterType::Scalar);
	Names.Vectors = CollectGlobalParameterNames(Source, EMaterialParameterType::Vector);
	Names.Textures = CollectGlobalParameterNames(Source, EMaterialParameterType::Texture);
	return Names;
}

static FString ParameterTypeName(EMaterialParameterType Type)
{
	return StaticEnum<EMaterialParameterType>()->GetNameStringByValue(static_cast<int64>(Type));
}

static FString SamplerTypeName(EMaterialSamplerType SamplerType)
{
	FString Name = StaticEnum<EMaterialSamplerType>()->GetNameStringByValue(SamplerType);
	Name.RemoveFromStart(SamplerTypePrefix, ESearchCase::CaseSensitive);
	return Name;
}

static bool ResolveTarget(const FAgentMaterialInstanceState& State, FAgentMaterialInstanceStateResult& Result, FResolvedTarget& OutTarget)
{
	if (UObject* InstanceAsset = AgentEditorAssetUtils::LoadAssetAtPath(State.Instance))
	{
		OutTarget.Instance = Cast<UMaterialInstanceConstant>(InstanceAsset);
		if (!OutTarget.Instance)
		{
			AddDiagnostic(Result, EAgentMaterialInstanceStateDiagnosticKind::NotAnInstance, FString::Printf(NotAnInstanceFormat, *InstanceAsset->GetPathName(), *InstanceAsset->GetClass()->GetName()));
			return false;
		}
		if (AgentEditorAssetUtils::IsAssetEditorOpen(*OutTarget.Instance))
		{
			AddDiagnostic(Result, EAgentMaterialInstanceStateDiagnosticKind::EditorOpen, FString::Printf(EditorOpenFormat, *OutTarget.Instance->GetPathName()));
		}
		OutTarget.ParameterSource = OutTarget.Instance;
		return true;
	}

	if (!State.bCreate)
	{
		AddDiagnostic(Result, EAgentMaterialInstanceStateDiagnosticKind::InstanceNotFound, FString::Printf(InstanceNotFoundFormat, *State.Instance));
		return false;
	}

	OutTarget.Parent = State.Parent.IsEmpty() ? nullptr : Cast<UMaterialInterface>(AgentEditorAssetUtils::LoadAssetAtPath(State.Parent));
	if (!OutTarget.Parent)
	{
		AddDiagnostic(Result, EAgentMaterialInstanceStateDiagnosticKind::ParentNotFound, FString::Printf(ParentNotFoundFormat, *State.Parent, *State.Instance));
		return false;
	}
	OutTarget.ParameterSource = OutTarget.Parent;
	return true;
}

static void PlanSamplerSwitch(FName Switch, bool bValue, FName Texture, FCheckedWrites& Checked, FAgentMaterialInstanceStateResult& Result)
{
	const FPlannedSamplerSwitch* Planned = Checked.SamplerSwitches.FindByPredicate([Switch](const FPlannedSamplerSwitch& Entry)
	{
		return Entry.Switch == Switch;
	});
	if (!Planned)
	{
		Checked.SamplerSwitches.Add({ Switch, bValue, Texture });
		return;
	}
	if (Planned->bValue != bValue)
	{
		AddDiagnostic(Result, EAgentMaterialInstanceStateDiagnosticKind::SamplerConflict, FString::Printf(SamplerConflictFormat,
			*Switch.ToString(), Planned->bValue ? OnText : OffText, *Planned->SetBy.ToString(), bValue ? OnText : OffText, *Texture.ToString()));
	}
}

static void CheckTextureWrite(const FAgentMaterialInstanceWrite& Write, int32 WriteIndex, const FParameterNames& Names, const UMaterialInterface& Source, FCheckedWrites& Checked, FAgentMaterialInstanceStateResult& Result)
{
	UTexture* Texture = Cast<UTexture>(AgentEditorAssetUtils::LoadAssetAtPath(Write.TexturePath));
	if (!Texture)
	{
		AddDiagnostic(Result, EAgentMaterialInstanceStateDiagnosticKind::TextureNotFound, FString::Printf(TextureNotFoundFormat, *Write.TexturePath, *Write.Name.ToString()));
		return;
	}
	Checked.Textures[WriteIndex] = Texture;
	if (!Write.bHasSamplers)
	{
		return;
	}

	bool bAllSwitchesFound = true;
	for (const FAgentMaterialInstanceSamplerSwitch& Sampler : Write.Samplers)
	{
		if (!Sampler.Switch.IsNone() && !Names.Switches.Contains(Sampler.Switch))
		{
			AddDiagnostic(Result, EAgentMaterialInstanceStateDiagnosticKind::SamplerSwitchNotFound, FString::Printf(SamplerSwitchNotFoundFormat, *Sampler.Switch.ToString(), *Write.Name.ToString(), *Source.GetPathName()));
			bAllSwitchesFound = false;
		}
	}

	const EMaterialSamplerType SamplerType = MaterialExpressionUtils::GetSamplerTypeForTexture(Texture);
	const FAgentMaterialInstanceSamplerSwitch* Chosen = Write.Samplers.FindByPredicate([SamplerType](const FAgentMaterialInstanceSamplerSwitch& Sampler)
	{
		return Sampler.SamplerType == SamplerType;
	});
	if (!Chosen)
	{
		AddDiagnostic(Result, EAgentMaterialInstanceStateDiagnosticKind::UnsupportedSampler, FString::Printf(UnsupportedSamplerFormat, *Texture->GetPathName(), *SamplerTypeName(SamplerType), *Write.Name.ToString()));
		return;
	}
	if (!bAllSwitchesFound)
	{
		return;
	}

	const FName ChosenSwitch = Chosen->Switch;
	for (const FAgentMaterialInstanceSamplerSwitch& Sampler : Write.Samplers)
	{
		if (!Sampler.Switch.IsNone())
		{
			PlanSamplerSwitch(Sampler.Switch, Sampler.Switch == ChosenSwitch, Write.Name, Checked, Result);
		}
	}
}

static FCheckedWrites CheckWrites(const FAgentMaterialInstanceState& State, const UMaterialInterface& Source, FAgentMaterialInstanceStateResult& Result)
{
	const FParameterNames Names = CollectParameterNames(Source);
	FCheckedWrites Checked;
	Checked.Textures.SetNumZeroed(State.Writes.Num());

	for (int32 WriteIndex = 0; WriteIndex < State.Writes.Num(); ++WriteIndex)
	{
		const FAgentMaterialInstanceWrite& Write = State.Writes[WriteIndex];
		if (!Names.Of(Write.Kind).Contains(Write.Name))
		{
			AddDiagnostic(Result, EAgentMaterialInstanceStateDiagnosticKind::UnknownParameter, FString::Printf(UnknownParameterFormat, *Write.Name.ToString(), *ParameterTypeName(ToParameterType(Write.Kind)), *Source.GetPathName()));
			continue;
		}
		if (Write.Kind == EAgentMaterialInstanceWriteKind::Texture)
		{
			CheckTextureWrite(Write, WriteIndex, Names, Source, Checked, Result);
		}
	}
	return Checked;
}

static UMaterialInstanceConstant* CreateInstance(const FString& InstancePath, UMaterialInterface& Parent)
{
	UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
	Factory->InitialParent = &Parent;
	const FString PackageName = FPackageName::ObjectPathToPackageName(InstancePath);
	UObject* Created = IAssetTools::Get().CreateAsset(FPackageName::GetShortName(PackageName), FPackageName::GetLongPackagePath(PackageName), UMaterialInstanceConstant::StaticClass(), Factory);
	return Cast<UMaterialInstanceConstant>(Created);
}

static void WriteValues(UMaterialInstanceConstant& Instance, const FAgentMaterialInstanceState& State, const FCheckedWrites& Checked)
{
	Instance.ClearParameterValuesEditorOnly();

	for (int32 WriteIndex = 0; WriteIndex < State.Writes.Num(); ++WriteIndex)
	{
		const FAgentMaterialInstanceWrite& Write = State.Writes[WriteIndex];
		const FMaterialParameterInfo ParameterInfo(Write.Name);
		switch (Write.Kind)
		{
		case EAgentMaterialInstanceWriteKind::Switch:
			Instance.SetStaticSwitchParameterValueEditorOnly(ParameterInfo, Write.bSwitchValue);
			break;
		case EAgentMaterialInstanceWriteKind::Scalar:
			Instance.SetScalarParameterValueEditorOnly(ParameterInfo, Write.ScalarValue);
			break;
		case EAgentMaterialInstanceWriteKind::Vector:
			Instance.SetVectorParameterValueEditorOnly(ParameterInfo, Write.VectorValue);
			break;
		case EAgentMaterialInstanceWriteKind::Texture:
			Instance.SetTextureParameterValueEditorOnly(ParameterInfo, Checked.Textures[WriteIndex]);
			break;
		}
	}

	for (const FPlannedSamplerSwitch& SamplerSwitch : Checked.SamplerSwitches)
	{
		Instance.SetStaticSwitchParameterValueEditorOnly(FMaterialParameterInfo(SamplerSwitch.Switch), SamplerSwitch.bValue);
	}
}

}

FAgentMaterialInstanceStateResult FAgentMaterialInstanceStateApplier::ApplyFile(const FString& StateFilePath, bool bWrite)
{
	using namespace AgentMaterialInstanceStateApplierLocal;

	FAgentMaterialInstanceStateResult Result;
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *StateFilePath))
	{
		AddDiagnostic(Result, EAgentMaterialInstanceStateDiagnosticKind::FileReadFailed, FString::Printf(FileReadFailedFormat, *StateFilePath));
		return Finish(Result);
	}

	FAgentMaterialInstanceState State;
	if (!FAgentMaterialInstanceState::ParseJson(Text, State, Result.Diagnostics))
	{
		Result.Label = State.Label;
		Result.Instance = State.Instance;
		return Finish(Result);
	}
	return Apply(State, bWrite);
}

FAgentMaterialInstanceStateResult FAgentMaterialInstanceStateApplier::Apply(const FAgentMaterialInstanceState& State, bool bWrite)
{
	using namespace AgentMaterialInstanceStateApplierLocal;

	FAgentMaterialInstanceStateResult Result;
	Result.Label = State.Label;
	Result.Instance = State.Instance;

	FResolvedTarget Target;
	if (!ResolveTarget(State, Result, Target))
	{
		return Finish(Result);
	}
	const FCheckedWrites Checked = CheckWrites(State, *Target.ParameterSource, Result);
	if (!Result.Diagnostics.IsEmpty() || !bWrite)
	{
		return Finish(Result);
	}

	UMaterialInstanceConstant* Instance = Target.Instance;
	if (!Instance)
	{
		Instance = CreateInstance(State.Instance, *Target.Parent);
		if (!Instance)
		{
			AddDiagnostic(Result, EAgentMaterialInstanceStateDiagnosticKind::CreateFailed, FString::Printf(CreateFailedFormat, *State.Instance));
			return Finish(Result);
		}
		Result.bCreated = true;
	}

	WriteValues(*Instance, State, Checked);
	UMaterialEditingLibrary::UpdateMaterialInstance(Instance);
	Result.bWritten = true;
	Result.Writes = State.Writes.Num();
	for (const FPlannedSamplerSwitch& SamplerSwitch : Checked.SamplerSwitches)
	{
		if (SamplerSwitch.bValue)
		{
			Result.SamplerSwitches.Add(SamplerSwitch.Switch.ToString());
		}
	}

	Result.bSaved = AgentEditorAssetUtils::SaveAssetPackage(*Instance);
	if (!Result.bSaved)
	{
		AddDiagnostic(Result, EAgentMaterialInstanceStateDiagnosticKind::SaveFailed, FString::Printf(SaveFailedFormat, *Instance->GetPackage()->GetName()));
	}
	return Finish(Result);
}

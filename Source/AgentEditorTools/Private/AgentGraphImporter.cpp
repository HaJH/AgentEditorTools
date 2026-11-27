// Copyright 2026 Jeonghyeon Ha. All Rights Reserved.

#include "AgentGraphImporter.h"

#include "Containers/ArrayView.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphUtilities.h"
#include "Editor.h"
#include "Internationalization/Text.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "MaterialEditingLibrary.h"
#include "MaterialGraph/MaterialGraph.h"
#include "MaterialGraph/MaterialGraphNode.h"
#include "MaterialGraph/MaterialGraphNode_Comment.h"
#include "MaterialGraph/MaterialGraphNode_Root.h"
#include "MaterialGraph/MaterialGraphSchema.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionComment.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionTextureBase.h"
#include "Materials/MaterialFunction.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/StringFormatArg.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"

namespace AgentGraphImporterLocal
{

static constexpr TCHAR BeginObjectClassPrefix[] = TEXT("Begin Object Class=");
static constexpr TCHAR TopLevelEndObject[] = TEXT("\nEnd Object");
static constexpr TCHAR PinMarker[] = TEXT("CustomProperties Pin (");
static constexpr TCHAR PinNameKey[] = TEXT("PinName=\"");
static constexpr TCHAR PinNameTerminator[] = TEXT("\"");
static constexpr TCHAR LinkedToKey[] = TEXT("LinkedTo=(");
static constexpr TCHAR LinkedToTerminator[] = TEXT(")");
static constexpr TCHAR LinkSeparator[] = TEXT(",");
static constexpr TCHAR LinkPartSeparator[] = TEXT(" ");
static constexpr TCHAR LineBreak[] = TEXT("\n");
static constexpr TCHAR AssetNotFoundDetail[] = TEXT("No asset was given to import into");
static constexpr TCHAR EmptyImportFormat[] = TEXT("Importing the node text (%d characters after removing root nodes) into %s created no nodes");
static constexpr TCHAR MalformedRootLinkFormat[] = TEXT("Root pin '%s' has a LinkedTo entry '%s' that is not a node name followed by a pin id");
static constexpr TCHAR UnsupportedAssetFormat[] = TEXT("%s is a %s, not a material or material function");
static constexpr TCHAR EditorOpenFormat[] = TEXT("An asset editor for %s is open");
static constexpr TCHAR RootInFunctionFormat[] = TEXT("%s is a material function and has no root node");
static constexpr TCHAR UnmatchedRootPinFormat[] = TEXT("The target root has no pin named '%s'");
static constexpr TCHAR UnresolvedRootLinkFormat[] = TEXT("Root pin '%s' links to pin %s, which is not among the imported nodes");
static constexpr TCHAR SaveFailedFormat[] = TEXT("Saving package %s failed");
static constexpr TCHAR AssetPathNotFoundFormat[] = TEXT("No asset was found at '%s'");
static constexpr TCHAR FileReadFailedFormat[] = TEXT("Reading the T3D file '%s' failed");
static constexpr TCHAR ObjectNameSeparator[] = TEXT(".");
static constexpr TCHAR ShadingModelPinNameSeparator[] = TEXT(" or ");
static constexpr TCHAR UnmatchedFunctionInputFormat[] = TEXT("The replaced function had an input named '{0}' that the imported text does not have; function calls lose that input's connection");
static constexpr TCHAR UnmatchedFunctionOutputFormat[] = TEXT("The replaced function had an output named '{0}' that the imported text does not have; function calls lose that output's connections");

struct FShadingModelPinName
{
	EMaterialShadingModel ShadingModel;
	const TCHAR* PinName;
};

static constexpr FShadingModelPinName MetallicPinNames[] = { { MSM_Hair, TEXT("Scatter") }, { MSM_Eye, TEXT("Curvature") } };
static constexpr TCHAR MetallicDefaultPinName[] = TEXT("Metallic");
static constexpr FShadingModelPinName NormalPinNames[] = { { MSM_Hair, TEXT("Tangent") } };
static constexpr TCHAR NormalDefaultPinName[] = TEXT("Normal");
static constexpr FShadingModelPinName SubsurfaceColorPinNames[] = { { MSM_Cloth, TEXT("Fuzz Color") } };
static constexpr TCHAR SubsurfaceColorDefaultPinName[] = TEXT("Subsurface Color");
static constexpr FShadingModelPinName CustomData0PinNames[] =
{
	{ MSM_ClearCoat, TEXT("Clear Coat") },
	{ MSM_Hair, TEXT("Backlit") },
	{ MSM_Cloth, TEXT("Cloth") },
	{ MSM_Eye, TEXT("Iris Mask") },
	{ MSM_SubsurfaceProfile, TEXT("Curvature") }
};
static constexpr TCHAR CustomData0DefaultPinName[] = TEXT("Custom Data 0");
static constexpr FShadingModelPinName CustomData1PinNames[] = { { MSM_ClearCoat, TEXT("Clear Coat Roughness") }, { MSM_Eye, TEXT("Iris Distance") } };
static constexpr TCHAR CustomData1DefaultPinName[] = TEXT("Custom Data 1");

static FString JoinShadingModelPinNames(FMaterialShadingModelField ShadingModels, TConstArrayView<FShadingModelPinName> CustomPinNames, const TCHAR* DefaultPinName)
{
	TArray<FString> PinNames;
	for (const FShadingModelPinName& CustomPinName : CustomPinNames)
	{
		if (ShadingModels.HasShadingModel(CustomPinName.ShadingModel))
		{
			PinNames.Add(CustomPinName.PinName);
			ShadingModels.RemoveShadingModel(CustomPinName.ShadingModel);
		}
	}
	if (ShadingModels.CountShadingModels() != 0)
	{
		PinNames.Add(DefaultPinName);
	}
	return FString::Join(PinNames, ShadingModelPinNameSeparator);
}

struct FRootPinLinks
{
	FName PinName;
	TArray<FGuid> LinkedPinIds;
};

static void AddDiagnostic(FAgentGraphImportResult& Result, EAgentGraphImportDiagnosticKind Kind, const FString& Detail)
{
	FAgentGraphImportDiagnostic& Diagnostic = Result.Diagnostics.AddDefaulted_GetRef();
	Diagnostic.Kind = Kind;
	Diagnostic.Detail = Detail;
}

static bool IsErrorKind(EAgentGraphImportDiagnosticKind Kind)
{
	switch (Kind)
	{
	case EAgentGraphImportDiagnosticKind::UnmatchedRootPin:
	case EAgentGraphImportDiagnosticKind::UnmatchedFunctionIO:
	case EAgentGraphImportDiagnosticKind::NoRoot:
	case EAgentGraphImportDiagnosticKind::CompileError:
		return false;
	default:
		return true;
	}
}

static FAgentGraphImportResult Finish(FAgentGraphImportResult& Result)
{
	Result.bSucceeded = !Result.Diagnostics.ContainsByPredicate([](const FAgentGraphImportDiagnostic& Diagnostic)
	{
		return IsErrorKind(Diagnostic.Kind);
	});
	return MoveTemp(Result);
}

static FString ExtractBetween(const FString& Line, const TCHAR* Key, const TCHAR* Terminator)
{
	const int32 KeyStart = Line.Find(Key, ESearchCase::CaseSensitive);
	if (KeyStart == INDEX_NONE)
	{
		return FString();
	}
	const int32 ValueStart = KeyStart + FCString::Strlen(Key);
	const int32 ValueEnd = Line.Find(Terminator, ESearchCase::CaseSensitive, ESearchDir::FromStart, ValueStart);
	if (ValueEnd == INDEX_NONE)
	{
		return FString();
	}
	return Line.Mid(ValueStart, ValueEnd - ValueStart);
}

static FRootPinLinks ParseRootPin(const FString& PinLine, FAgentGraphImportResult& Result)
{
	FRootPinLinks Pin;
	Pin.PinName = FName(*ExtractBetween(PinLine, PinNameKey, PinNameTerminator));

	TArray<FString> Links;
	ExtractBetween(PinLine, LinkedToKey, LinkedToTerminator).ParseIntoArray(Links, LinkSeparator, true);
	for (const FString& Link : Links)
	{
		const FString TrimmedLink = Link.TrimStartAndEnd();
		FString NodeName;
		FString PinIdText;
		FGuid PinId;
		if (!TrimmedLink.Split(LinkPartSeparator, &NodeName, &PinIdText) || !FGuid::Parse(PinIdText, PinId))
		{
			AddDiagnostic(Result, EAgentGraphImportDiagnosticKind::UnmatchedRootPin, FString::Printf(MalformedRootLinkFormat, *Pin.PinName.ToString(), *TrimmedLink));
			continue;
		}
		Pin.LinkedPinIds.Add(PinId);
	}
	return Pin;
}

static void ParseRootPins(const FString& RootBlock, TArray<FRootPinLinks>& OutRootPins, FAgentGraphImportResult& Result)
{
	int32 PinStart = RootBlock.Find(PinMarker, ESearchCase::CaseSensitive);
	while (PinStart != INDEX_NONE)
	{
		int32 LineEnd = RootBlock.Find(LineBreak, ESearchCase::CaseSensitive, ESearchDir::FromStart, PinStart);
		if (LineEnd == INDEX_NONE)
		{
			LineEnd = RootBlock.Len();
		}
		OutRootPins.Add(ParseRootPin(RootBlock.Mid(PinStart, LineEnd - PinStart), Result));
		PinStart = RootBlock.Find(PinMarker, ESearchCase::CaseSensitive, ESearchDir::FromStart, LineEnd);
	}
}

static bool ExtractRootNodes(const FString& Text, FString& OutNodeText, TArray<FRootPinLinks>& OutRootPins, FAgentGraphImportResult& Result)
{
	const FString RootBegin = FString(BeginObjectClassPrefix) + UMaterialGraphNode_Root::StaticClass()->GetPathName() + LinkPartSeparator;
	OutNodeText.Reset();
	bool bFoundRoot = false;
	int32 SearchFrom = 0;
	for (;;)
	{
		const int32 BlockStart = Text.Find(RootBegin, ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchFrom);
		if (BlockStart == INDEX_NONE)
		{
			OutNodeText += Text.Mid(SearchFrom);
			return bFoundRoot;
		}
		const int32 EndLineStart = Text.Find(TopLevelEndObject, ESearchCase::CaseSensitive, ESearchDir::FromStart, BlockStart);
		const int32 BlockEnd = EndLineStart == INDEX_NONE ? Text.Len() : EndLineStart + FCString::Strlen(TopLevelEndObject);
		OutNodeText += Text.Mid(SearchFrom, BlockStart - SearchFrom);
		ParseRootPins(Text.Mid(BlockStart, BlockEnd - BlockStart), OutRootPins, Result);
		bFoundRoot = true;
		SearchFrom = BlockEnd;
	}
}

static bool IsAssetEditorOpen(UObject& Asset)
{
	if (!GEditor)
	{
		return false;
	}
	UAssetEditorSubsystem* AssetEditors = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	return AssetEditors && !AssetEditors->FindEditorsForAsset(&Asset).IsEmpty();
}

template <typename OwnerType>
static void RemoveAllComments(OwnerType& Owner)
{
	const TArray<TObjectPtr<UMaterialExpressionComment>> Comments(Owner.GetEditorComments());
	for (UMaterialExpressionComment* Comment : Comments)
	{
		Owner.GetExpressionCollection().RemoveComment(Comment);
		Comment->MarkAsGarbage();
	}
}

static void DeleteAllExpressions(UMaterial* Material, UMaterialFunction* Function)
{
	if (Material)
	{
		const TArray<TObjectPtr<UMaterialExpression>> Expressions(Material->GetExpressions());
		for (UMaterialExpression* Expression : Expressions)
		{
			UMaterialEditingLibrary::DeleteMaterialExpression(Material, Expression);
		}
		RemoveAllComments(*Material);
		return;
	}

	const TArray<TObjectPtr<UMaterialExpression>> Expressions(Function->GetExpressions());
	for (UMaterialExpression* Expression : Expressions)
	{
		UMaterialEditingLibrary::DeleteMaterialExpressionInFunction(Function, Expression);
	}
	RemoveAllComments(*Function);
}

struct FFunctionIOIds
{
	TMap<FName, FGuid> InputIds;
	TMap<FName, FGuid> OutputIds;
};

static FFunctionIOIds CollectFunctionIOIds(const UMaterialFunction& Function)
{
	FFunctionIOIds Ids;
	for (const UMaterialExpression* Expression : Function.GetExpressions())
	{
		if (const UMaterialExpressionFunctionInput* FunctionInput = Cast<UMaterialExpressionFunctionInput>(Expression))
		{
			Ids.InputIds.Add(FunctionInput->InputName, FunctionInput->Id);
		}
		else if (const UMaterialExpressionFunctionOutput* FunctionOutput = Cast<UMaterialExpressionFunctionOutput>(Expression))
		{
			Ids.OutputIds.Add(FunctionOutput->OutputName, FunctionOutput->Id);
		}
	}
	return Ids;
}

template <typename ExpressionType>
static void TakeOverPreviousId(ExpressionType& Expression, FName Name, TMap<FName, FGuid>& PreviousIds)
{
	FGuid PreviousId;
	if (PreviousIds.RemoveAndCopyValue(Name, PreviousId))
	{
		Expression.Id = PreviousId;
	}
}

static void ReportUnmatchedFunctionIO(const TMap<FName, FGuid>& UnmatchedIds, const TCHAR* Format, FAgentGraphImportResult& Result)
{
	for (const TPair<FName, FGuid>& Unmatched : UnmatchedIds)
	{
		AddDiagnostic(Result, EAgentGraphImportDiagnosticKind::UnmatchedFunctionIO, FString::Format(Format, { Unmatched.Key.ToString() }));
	}
}

static void RestoreFunctionIOIds(const UMaterial& GraphMaterial, FFunctionIOIds& PreviousIds, FAgentGraphImportResult& Result)
{
	for (UMaterialExpression* Expression : GraphMaterial.GetExpressions())
	{
		if (UMaterialExpressionFunctionInput* FunctionInput = Cast<UMaterialExpressionFunctionInput>(Expression))
		{
			TakeOverPreviousId(*FunctionInput, FunctionInput->InputName, PreviousIds.InputIds);
		}
		else if (UMaterialExpressionFunctionOutput* FunctionOutput = Cast<UMaterialExpressionFunctionOutput>(Expression))
		{
			TakeOverPreviousId(*FunctionOutput, FunctionOutput->OutputName, PreviousIds.OutputIds);
		}
	}
	ReportUnmatchedFunctionIO(PreviousIds.InputIds, UnmatchedFunctionInputFormat, Result);
	ReportUnmatchedFunctionIO(PreviousIds.OutputIds, UnmatchedFunctionOutputFormat, Result);
}

static UMaterial* MakeFunctionEditMaterial(UMaterialFunction& Function)
{
	UMaterial* EditMaterial = NewObject<UMaterial>(GetTransientPackage(), NAME_None, RF_Transient);
	EditMaterial->AssignExpressionCollection(Function.GetExpressionCollection());
	return EditMaterial;
}

struct FTemporaryGraph
{
	UMaterial& GraphMaterial;
	TStrongObjectPtr<UMaterialGraph> Graph;

	FTemporaryGraph(UMaterial& InGraphMaterial, UMaterialFunction* Function)
		: GraphMaterial(InGraphMaterial)
	{
		Graph.Reset(CastChecked<UMaterialGraph>(FBlueprintEditorUtils::CreateNewGraph(&GraphMaterial, NAME_None, UMaterialGraph::StaticClass(), UMaterialGraphSchema::StaticClass())));
		Graph->SetFlags(RF_Transient);
		Graph->Material = &GraphMaterial;
		Graph->MaterialFunction = Function;
		GraphMaterial.MaterialGraph = Graph.Get();
		Graph->RebuildGraph();
	}

	~FTemporaryGraph()
	{
		for (UMaterialExpression* Expression : GraphMaterial.GetExpressions())
		{
			Expression->GraphNode = nullptr;
		}
		for (UMaterialExpressionComment* Comment : GraphMaterial.GetEditorComments())
		{
			Comment->GraphNode = nullptr;
		}
		GraphMaterial.MaterialGraph = nullptr;
	}

	FTemporaryGraph(const FTemporaryGraph&) = delete;
	FTemporaryGraph& operator=(const FTemporaryGraph&) = delete;
	FTemporaryGraph(FTemporaryGraph&&) = delete;
	FTemporaryGraph& operator=(FTemporaryGraph&&) = delete;
};

static void FixUpPastedFunctionCall(UMaterialExpressionMaterialFunctionCall& FunctionCall)
{
	FunctionCall.UpdateFromFunctionResource(false);

	if (UMaterialGraphNode* MaterialGraphNode = Cast<UMaterialGraphNode>(FunctionCall.GraphNode))
	{
		for (UEdGraphPin* Pin : MaterialGraphNode->Pins)
		{
			if (Pin->Direction != EGPD_Input)
			{
				continue;
			}
			const int32 InputIndex = Pin->SourceIndex;
			if (InputIndex >= 0 && InputIndex < FunctionCall.FunctionInputs.Num())
			{
				Pin->PinType.PinCategory = FunctionCall.IsInputConnectionRequired(InputIndex) ? UMaterialGraphSchema::PC_Required : UMaterialGraphSchema::PC_Optional;

				const FName NewPinName = UMaterialGraphNode::GetShortenPinName(FunctionCall.GetInputName(InputIndex));
				if (!NewPinName.IsNone())
				{
					Pin->PinName = NewPinName;
				}
			}
		}
	}

	if (FunctionCall.GraphNode && FunctionCall.MaterialFunction == nullptr && FunctionCall.FunctionInputs.IsEmpty() && FunctionCall.FunctionOutputs.IsEmpty())
	{
		FunctionCall.GraphNode->Pins.Empty();
	}
}

static void PostPasteExpression(UMaterial& GraphMaterial, UMaterialExpression& NewExpression)
{
	NewExpression.UpdateMaterialExpressionGuid(true, true);

	if (UMaterialExpressionTextureBase* TextureSample = Cast<UMaterialExpressionTextureBase>(&NewExpression))
	{
		TextureSample->IsDefaultMeshpaintTexture = false;
	}

	NewExpression.UpdateParameterGuid(true, true);
	GraphMaterial.AddExpressionParameter(&NewExpression, GraphMaterial.EditorParameters);

	if (UMaterialExpressionFunctionInput* FunctionInput = Cast<UMaterialExpressionFunctionInput>(&NewExpression))
	{
		FunctionInput->ConditionallyGenerateId(true);
		FunctionInput->ValidateName();
	}

	if (UMaterialExpressionFunctionOutput* FunctionOutput = Cast<UMaterialExpressionFunctionOutput>(&NewExpression))
	{
		FunctionOutput->ConditionallyGenerateId(true);
		FunctionOutput->ValidateName();
	}

	if (UMaterialExpressionMaterialFunctionCall* FunctionCall = Cast<UMaterialExpressionMaterialFunctionCall>(&NewExpression))
	{
		FixUpPastedFunctionCall(*FunctionCall);
	}
}

static void RegisterPastedNodes(const UMaterialGraph& Graph, const TSet<UEdGraphNode*>& PastedNodes, UMaterial& GraphMaterial, UMaterialFunction* Function, FAgentGraphImportResult& Result)
{
	TArray<UMaterialExpression*> NewExpressions;
	for (UEdGraphNode* Node : PastedNodes)
	{
		const UMaterialGraphNode* GraphNode = Cast<UMaterialGraphNode>(Node);
		if (UMaterialExpression* NewExpression = GraphNode ? GraphNode->MaterialExpression.Get() : nullptr)
		{
			NewExpression->Material = &GraphMaterial;
			NewExpression->Function = Function;
			NewExpression->SubgraphExpression = Graph.SubgraphExpression;

			if (NewExpression->HasAParameterName())
			{
				NewExpression->ValidateParameterName();
			}

			NewExpressions.Add(NewExpression);
			GraphMaterial.GetExpressionCollection().AddExpression(NewExpression);
			PostPasteExpression(GraphMaterial, *NewExpression);
		}
		else if (UMaterialGraphNode_Comment* CommentNode = Cast<UMaterialGraphNode_Comment>(Node))
		{
			if (UMaterialExpressionComment* Comment = CommentNode->MaterialExpressionComment)
			{
				Comment->Material = &GraphMaterial;
				Comment->Function = Function;
				Comment->SubgraphExpression = Graph.SubgraphExpression;
				GraphMaterial.GetExpressionCollection().AddComment(Comment);
			}
		}

		Node->CreateNewGuid();
	}

	for (UMaterialExpression* NewExpression : NewExpressions)
	{
		NewExpression->PostCopyNode(NewExpressions);
	}

	for (const UMaterialExpression* NewExpression : NewExpressions)
	{
		Result.CreatedExpressions.Add(NewExpression->GetName());
	}
}

static bool IsJoinedFromLocalizedPieces(const FText& InputName)
{
	return FTextInspector::GetTextId(InputName).IsEmpty();
}

static bool IsRootInputPinNamed(const UMaterialGraph& Graph, const UEdGraphPin& Pin, FName PinName)
{
	if (Pin.PinName == PinName)
	{
		return true;
	}
	check(Graph.MaterialInputs.IsValidIndex(Pin.SourceIndex));
	const FMaterialInputInfo& Input = Graph.MaterialInputs[Pin.SourceIndex];
	const FText& InputName = Input.GetName();
	const FString ActualPinName = Pin.PinName.ToString();
	const FString InputDisplayName = InputName.ToString();

	if (IsJoinedFromLocalizedPieces(InputName))
	{
		check(Graph.Material);
		const FString EnglishName = FAgentGraphImporter::GetEnglishRootInputName(Input.GetProperty(), Graph.Material->GetShadingModels());
		return !EnglishName.IsEmpty() && FAgentGraphImporter::MatchesRootPinSourceName(PinName, ActualPinName, InputDisplayName, EnglishName);
	}

	const FString* InputSourceName = FTextInspector::GetSourceString(InputName);
	return InputSourceName && FAgentGraphImporter::MatchesRootPinSourceName(PinName, ActualPinName, InputDisplayName, *InputSourceName);
}

static UEdGraphPin* FindRootInputPin(const UMaterialGraph& Graph, FName PinName)
{
	for (UEdGraphPin* Pin : Graph.RootNode->Pins)
	{
		if (Pin->Direction == EGPD_Input && IsRootInputPinNamed(Graph, *Pin, PinName))
		{
			return Pin;
		}
	}
	return nullptr;
}

static void ConnectRoot(const UMaterialGraph& Graph, const TSet<UEdGraphNode*>& PastedNodes, const TArray<FRootPinLinks>& RootPins, FAgentGraphImportResult& Result)
{
	check(Graph.RootNode);

	TMap<FGuid, UEdGraphPin*> PastedPins;
	for (const UEdGraphNode* Node : PastedNodes)
	{
		for (UEdGraphPin* Pin : Node->Pins)
		{
			PastedPins.Add(Pin->PinId, Pin);
		}
	}

	for (const FRootPinLinks& RootPin : RootPins)
	{
		if (RootPin.LinkedPinIds.IsEmpty())
		{
			continue;
		}

		UEdGraphPin* TargetPin = FindRootInputPin(Graph, RootPin.PinName);
		if (!TargetPin)
		{
			AddDiagnostic(Result, EAgentGraphImportDiagnosticKind::UnmatchedRootPin, FString::Printf(UnmatchedRootPinFormat, *RootPin.PinName.ToString()));
			continue;
		}

		TargetPin->BreakAllPinLinks();
		for (const FGuid& LinkedPinId : RootPin.LinkedPinIds)
		{
			UEdGraphPin* const* SourcePin = PastedPins.Find(LinkedPinId);
			if (!SourcePin)
			{
				AddDiagnostic(Result, EAgentGraphImportDiagnosticKind::UnmatchedRootPin, FString::Printf(UnresolvedRootLinkFormat, *RootPin.PinName.ToString(), *LinkedPinId.ToString()));
				continue;
			}
			TargetPin->MakeLinkTo(*SourcePin);
		}
	}
}

static void CopyExpressionsBackToFunction(const UMaterial& EditMaterial, UMaterialFunction& Function)
{
	Function.AssignExpressionCollection(EditMaterial.GetExpressionCollection());
	for (UMaterialExpression* Expression : Function.GetExpressions())
	{
		Expression->Material = nullptr;
		Expression->Function = &Function;
	}
	for (UMaterialExpressionComment* Comment : Function.GetEditorComments())
	{
		Comment->Material = nullptr;
		Comment->Function = &Function;
	}
}

static bool SaveAssetPackage(UObject& Asset)
{
	UPackage* Package = Asset.GetPackage();
	FString Filename;
	if (!FPackageName::TryConvertLongPackageNameToFilename(Package->GetName(), Filename, FPackageName::GetAssetPackageExtension()))
	{
		return false;
	}

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	return UPackage::SavePackage(Package, &Asset, *Filename, SaveArgs);
}

static UObject* LoadAssetAtPath(const FString& AssetPath)
{
	const FString PackageName = FPackageName::ObjectPathToPackageName(AssetPath);
	if (!FPackageName::IsValidLongPackageName(PackageName))
	{
		return nullptr;
	}
	const FString ObjectPath = PackageName == AssetPath ? AssetPath + ObjectNameSeparator + FPackageName::GetShortName(PackageName) : AssetPath;
	return LoadObject<UObject>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
}

}

bool FAgentGraphImporter::MatchesRootPinSourceName(FName RequestedPinName, const FString& ActualPinName, const FString& InputDisplayName, const FString& InputSourceName)
{
	if (!ActualPinName.StartsWith(InputDisplayName, ESearchCase::CaseSensitive))
	{
		return false;
	}
	const FString SourcePinName = InputSourceName + ActualPinName.RightChop(InputDisplayName.Len());
	return !SourcePinName.IsEmpty() && FName(*SourcePinName) == RequestedPinName;
}

FString FAgentGraphImporter::GetEnglishRootInputName(EMaterialProperty Property, FMaterialShadingModelField ShadingModels)
{
	using namespace AgentGraphImporterLocal;

	switch (Property)
	{
	case MP_Metallic:
		return JoinShadingModelPinNames(ShadingModels, MetallicPinNames, MetallicDefaultPinName);
	case MP_Normal:
		return JoinShadingModelPinNames(ShadingModels, NormalPinNames, NormalDefaultPinName);
	case MP_SubsurfaceColor:
		return JoinShadingModelPinNames(ShadingModels, SubsurfaceColorPinNames, SubsurfaceColorDefaultPinName);
	case MP_CustomData0:
		return JoinShadingModelPinNames(ShadingModels, CustomData0PinNames, CustomData0DefaultPinName);
	case MP_CustomData1:
		return JoinShadingModelPinNames(ShadingModels, CustomData1PinNames, CustomData1DefaultPinName);
	default:
		return FString();
	}
}

FAgentGraphImportResult FAgentGraphImporter::ImportFile(const FString& AssetPath, const FString& T3DFilePath, bool bReplace, bool bSave)
{
	using namespace AgentGraphImporterLocal;

	FAgentGraphImportResult Result;
	UObject* Asset = LoadAssetAtPath(AssetPath);
	if (!Asset)
	{
		AddDiagnostic(Result, EAgentGraphImportDiagnosticKind::AssetNotFound, FString::Printf(AssetPathNotFoundFormat, *AssetPath));
	}

	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *T3DFilePath))
	{
		AddDiagnostic(Result, EAgentGraphImportDiagnosticKind::FileReadFailed, FString::Printf(FileReadFailedFormat, *T3DFilePath));
	}

	if (!Result.Diagnostics.IsEmpty())
	{
		Result.Asset = AssetPath;
		return Finish(Result);
	}
	return Import(Asset, Text, bReplace, bSave);
}

FAgentGraphImportResult FAgentGraphImporter::Import(UObject* Asset, const FString& Text, bool bReplace, bool bSave)
{
	using namespace AgentGraphImporterLocal;

	FAgentGraphImportResult Result;
	if (!Asset)
	{
		AddDiagnostic(Result, EAgentGraphImportDiagnosticKind::AssetNotFound, AssetNotFoundDetail);
		return Finish(Result);
	}
	Result.Asset = Asset->GetPathName();

	UMaterial* Material = Cast<UMaterial>(Asset);
	UMaterialFunction* Function = Cast<UMaterialFunction>(Asset);
	if (!Material && !Function)
	{
		AddDiagnostic(Result, EAgentGraphImportDiagnosticKind::UnsupportedAsset, FString::Printf(UnsupportedAssetFormat, *Result.Asset, *Asset->GetClass()->GetName()));
		return Finish(Result);
	}

	if (IsAssetEditorOpen(*Asset))
	{
		AddDiagnostic(Result, EAgentGraphImportDiagnosticKind::EditorOpen, FString::Printf(EditorOpenFormat, *Result.Asset));
		return Finish(Result);
	}

	FString NodeText;
	TArray<FRootPinLinks> RootPins;
	const bool bHasRoot = ExtractRootNodes(Text, NodeText, RootPins, Result);
	if (Function && bHasRoot)
	{
		AddDiagnostic(Result, EAgentGraphImportDiagnosticKind::RootInFunction, FString::Printf(RootInFunctionFormat, *Result.Asset));
		return Finish(Result);
	}

	const bool bReplaceFunction = bReplace && Function;
	FFunctionIOIds PreviousFunctionIOIds;
	if (bReplaceFunction)
	{
		PreviousFunctionIOIds = CollectFunctionIOIds(*Function);
	}

	if (bReplace)
	{
		DeleteAllExpressions(Material, Function);
	}

	TStrongObjectPtr<UMaterial> FunctionEditMaterial;
	if (Function)
	{
		FunctionEditMaterial.Reset(MakeFunctionEditMaterial(*Function));
	}
	UMaterial& GraphMaterial = Material ? *Material : *FunctionEditMaterial;
	{
		FTemporaryGraph TemporaryGraph(GraphMaterial, Function);

		TSet<UEdGraphNode*> PastedNodes;
		FEdGraphUtilities::ImportNodesFromText(TemporaryGraph.Graph.Get(), NodeText, PastedNodes);
		if (PastedNodes.IsEmpty())
		{
			AddDiagnostic(Result, EAgentGraphImportDiagnosticKind::EmptyImport, FString::Printf(EmptyImportFormat, NodeText.Len(), *Result.Asset));
			return Finish(Result);
		}

		RegisterPastedNodes(*TemporaryGraph.Graph, PastedNodes, GraphMaterial, Function, Result);

		if (bReplaceFunction)
		{
			RestoreFunctionIOIds(GraphMaterial, PreviousFunctionIOIds, Result);
		}

		if (Material)
		{
			if (bHasRoot)
			{
				ConnectRoot(*TemporaryGraph.Graph, PastedNodes, RootPins, Result);
			}
			else
			{
				AddDiagnostic(Result, EAgentGraphImportDiagnosticKind::NoRoot, FString());
			}
		}

		TemporaryGraph.Graph->LinkMaterialExpressionsFromGraph();

		if (Function)
		{
			CopyExpressionsBackToFunction(GraphMaterial, *Function);
		}
	}

	Asset->MarkPackageDirty();

	if (Material)
	{
		const TArray<FString> CompileErrors = UMaterialEditingLibrary::RecompileMaterial(Material);
		if (!CompileErrors.IsEmpty())
		{
			AddDiagnostic(Result, EAgentGraphImportDiagnosticKind::CompileError, FString::Join(CompileErrors, LineBreak));
		}
	}
	else
	{
		UMaterialEditingLibrary::UpdateMaterialFunction(Function);
	}

	if (bSave)
	{
		Result.bSaved = SaveAssetPackage(*Asset);
		if (!Result.bSaved)
		{
			AddDiagnostic(Result, EAgentGraphImportDiagnosticKind::SaveFailed, FString::Printf(SaveFailedFormat, *Asset->GetPackage()->GetName()));
		}
	}

	return Finish(Result);
}

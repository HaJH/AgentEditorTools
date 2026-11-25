// Copyright 2026 Jeonghyeon Ha. All Rights Reserved.

#include "AgentGraphImporter.h"

#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphUtilities.h"
#include "Editor.h"
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
#include "Misc/PackageName.h"
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

static UEdGraphPin* FindRootInputPin(const UMaterialGraphNode_Root& RootNode, FName PinName)
{
	for (UEdGraphPin* Pin : RootNode.Pins)
	{
		if (Pin->Direction == EGPD_Input && Pin->PinName == PinName)
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

		UEdGraphPin* TargetPin = FindRootInputPin(*Graph.RootNode, RootPin.PinName);
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

	if (bReplace)
	{
		DeleteAllExpressions(Material, Function);
	}

	UMaterial& GraphMaterial = Material ? *Material : *MakeFunctionEditMaterial(*Function);
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

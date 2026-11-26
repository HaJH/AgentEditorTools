// Copyright 2026 Jeonghyeon Ha. All Rights Reserved.

#if WITH_AUTOMATION_TESTS

#include "AgentGraphImporter.h"
#include "AgentGraphImportResult.h"
#include "CQTest.h"
#include "Internationalization/Text.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialFunction.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialAttributeDefinitionMap.h"
#include "Tests/Support/AgentGraphImportT3D.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/Package.h"

namespace AgentGraphImporterTests
{

using AgentGraphImportT3D::FPinLink;

static constexpr TCHAR ConstantNode[] = TEXT("MaterialGraphNode_0");
static constexpr TCHAR MultiplyNode[] = TEXT("MaterialGraphNode_1");
static constexpr TCHAR RootNodeName[] = TEXT("MaterialGraphNode_2");
static constexpr TCHAR ConstantOutputPin[] = TEXT("A0000000000000000000000000000001");
static constexpr TCHAR MultiplyInputAPin[] = TEXT("A0000000000000000000000000000002");
static constexpr TCHAR MultiplyInputBPin[] = TEXT("A0000000000000000000000000000003");
static constexpr TCHAR MultiplyOutputPin[] = TEXT("A0000000000000000000000000000004");
static constexpr TCHAR RootInputPin[] = TEXT("A0000000000000000000000000000005");
static constexpr TCHAR ConstantNodeGuid[] = TEXT("B0000000000000000000000000000001");
static constexpr TCHAR MultiplyNodeGuid[] = TEXT("B0000000000000000000000000000002");
static constexpr TCHAR RootNodeGuid[] = TEXT("B0000000000000000000000000000003");
static constexpr TCHAR ConstantProperties[] = TEXT("      R=2.000000\n      MaterialExpressionGuid=C0000000000000000000000000000001\n");
static constexpr TCHAR MultiplyProperties[] = TEXT("      ConstB=3.000000\n      MaterialExpressionGuid=C0000000000000000000000000000002\n");
static constexpr TCHAR ConstantName[] = TEXT("MaterialExpressionConstant_0");
static constexpr TCHAR MultiplyName[] = TEXT("MaterialExpressionMultiply_0");
static constexpr TCHAR OutputPinName[] = TEXT("Output");
static constexpr TCHAR InputAPinName[] = TEXT("A");
static constexpr TCHAR InputBPinName[] = TEXT("B");
static constexpr TCHAR MissingRootPinName[] = TEXT("No Such Input");
static constexpr TCHAR LocalizedInputName[] = TEXT("Endfarbe");
static constexpr TCHAR SourceInputName[] = TEXT("Final Color");
static constexpr TCHAR OtherSourceInputName[] = TEXT("Opacity");
static constexpr TCHAR LocalizedSuffixedInputName[] = TEXT("Brechung");
static constexpr TCHAR SourceSuffixedInputName[] = TEXT("Refraction");
static constexpr TCHAR PinNameSuffix[] = TEXT(" (IOR)");

static constexpr TCHAR FunctionInputNode[] = TEXT("MaterialGraphNode_0");
static constexpr TCHAR FunctionOutputNode[] = TEXT("MaterialGraphNode_1");
static constexpr TCHAR FunctionInputOutputPin[] = TEXT("D0000000000000000000000000000001");
static constexpr TCHAR FunctionOutputInputPin[] = TEXT("D0000000000000000000000000000002");
static constexpr TCHAR FunctionInputNodeGuid[] = TEXT("E0000000000000000000000000000001");
static constexpr TCHAR FunctionOutputNodeGuid[] = TEXT("E0000000000000000000000000000002");
static constexpr TCHAR FunctionInputName[] = TEXT("MaterialExpressionFunctionInput_0");
static constexpr TCHAR FunctionOutputName[] = TEXT("MaterialExpressionFunctionOutput_0");
static constexpr TCHAR FunctionInputProperties[] = TEXT("      InputName=\"In\"\n      InputType=FunctionInput_Scalar\n      MaterialExpressionGuid=F0000000000000000000000000000001\n");
static constexpr TCHAR FunctionOutputProperties[] = TEXT("      OutputName=\"Out\"\n      MaterialExpressionGuid=F0000000000000000000000000000002\n");
static constexpr TCHAR FunctionOutputInputPinName[] = TEXT("Input");

static FString ConstantMultiplyNodes()
{
	using namespace AgentGraphImportT3D;
	const FPinLink ConstantToMultiply[] = { { MultiplyNode, MultiplyInputAPin } };
	const FPinLink MultiplyFromConstant[] = { { ConstantNode, ConstantOutputPin } };
	const FPinLink MultiplyToRoot[] = { { RootNodeName, RootInputPin } };
	return ExpressionNode(ConstantNode, *UMaterialExpressionConstant::StaticClass(), ConstantName, ConstantProperties, 0, ConstantNodeGuid,
			Pin(ConstantOutputPin, OutputPinName, EGPD_Output, ConstantToMultiply))
		+ ExpressionNode(MultiplyNode, *UMaterialExpressionMultiply::StaticClass(), MultiplyName, MultiplyProperties, 300, MultiplyNodeGuid,
			Pin(MultiplyInputAPin, InputAPinName, EGPD_Input, MultiplyFromConstant)
			+ Pin(MultiplyInputBPin, InputBPinName, EGPD_Input)
			+ Pin(MultiplyOutputPin, OutputPinName, EGPD_Output, MultiplyToRoot));
}

static FString RootLinkedToMultiply(const FString& RootPinName)
{
	using namespace AgentGraphImportT3D;
	const FPinLink RootFromMultiply[] = { { MultiplyNode, MultiplyOutputPin } };
	return RootNode(RootNodeName, RootNodeGuid, Pin(RootInputPin, *RootPinName, EGPD_Input, RootFromMultiply));
}

static FString FunctionInputOutputNodes()
{
	using namespace AgentGraphImportT3D;
	const FPinLink InputToOutput[] = { { FunctionOutputNode, FunctionOutputInputPin } };
	const FPinLink OutputFromInput[] = { { FunctionInputNode, FunctionInputOutputPin } };
	return ExpressionNode(FunctionInputNode, *UMaterialExpressionFunctionInput::StaticClass(), FunctionInputName, FunctionInputProperties, 0, FunctionInputNodeGuid,
			Pin(FunctionInputOutputPin, OutputPinName, EGPD_Output, InputToOutput))
		+ ExpressionNode(FunctionOutputNode, *UMaterialExpressionFunctionOutput::StaticClass(), FunctionOutputName, FunctionOutputProperties, 300, FunctionOutputNodeGuid,
			Pin(FunctionOutputInputPin, FunctionOutputInputPinName, EGPD_Input, OutputFromInput));
}

template <typename ExpressionType, typename OwnerType>
static ExpressionType* FindExpression(const OwnerType& Owner)
{
	for (UMaterialExpression* Expression : Owner.GetExpressions())
	{
		if (ExpressionType* Typed = Cast<ExpressionType>(Expression))
		{
			return Typed;
		}
	}
	return nullptr;
}

TEST_CLASS(AgentGraphImporter, "AgentEditorTools.Unit.GraphImport")
{
	TStrongObjectPtr<UMaterial> Material;
	TStrongObjectPtr<UMaterialFunction> Function;

	BEFORE_EACH()
	{
		Material.Reset(NewObject<UMaterial>(GetTransientPackage(), NAME_None, RF_Transient));
		Function.Reset(NewObject<UMaterialFunction>(GetTransientPackage(), NAME_None, RF_Transient));
	}

	FString BaseColorPinName() const
	{
		return FMaterialAttributeDefinitionMap::GetDisplayNameForMaterial(MP_BaseColor, Material.Get()).ToString();
	}

	FString BaseColorSourcePinName() const
	{
		const FText DisplayName = FMaterialAttributeDefinitionMap::GetDisplayNameForMaterial(MP_BaseColor, Material.Get());
		const FString* SourceName = FTextInspector::GetSourceString(DisplayName);
		return SourceName ? *SourceName : DisplayName.ToString();
	}

	TEST_METHOD(SourceRootPinNameFeedsBaseColor)
	{
		const FAgentGraphImportResult Result = FAgentGraphImporter::Import(Material.Get(), ConstantMultiplyNodes() + RootLinkedToMultiply(BaseColorSourcePinName()), false, false);

		ASSERT_THAT(IsTrue(Result.bSucceeded));
		ASSERT_THAT(IsFalse(AgentGraphImportT3D::HasDiagnostic(Result, EAgentGraphImportDiagnosticKind::UnmatchedRootPin)));
		UMaterialExpressionMultiply* Multiply = FindExpression<UMaterialExpressionMultiply>(*Material);
		ASSERT_THAT(IsNotNull(Multiply));
		ASSERT_THAT(IsTrue(Material->GetEditorOnlyData()->BaseColor.Expression == Multiply));
	}

	TEST_METHOD(SourceNameMatchesLocalizedRootPin)
	{
		ASSERT_THAT(IsTrue(FAgentGraphImporter::MatchesRootPinSourceName(SourceInputName, LocalizedInputName, LocalizedInputName, SourceInputName)));
	}

	TEST_METHOD(OtherSourceNameDoesNotMatchLocalizedRootPin)
	{
		ASSERT_THAT(IsFalse(FAgentGraphImporter::MatchesRootPinSourceName(OtherSourceInputName, LocalizedInputName, LocalizedInputName, SourceInputName)));
	}

	TEST_METHOD(SourceNameKeepsPinNameSuffix)
	{
		const FString ActualPinName = FString(LocalizedSuffixedInputName) + PinNameSuffix;
		const FString RequestedPinName = FString(SourceSuffixedInputName) + PinNameSuffix;

		ASSERT_THAT(IsTrue(FAgentGraphImporter::MatchesRootPinSourceName(FName(*RequestedPinName), ActualPinName, LocalizedSuffixedInputName, SourceSuffixedInputName)));
		ASSERT_THAT(IsFalse(FAgentGraphImporter::MatchesRootPinSourceName(SourceSuffixedInputName, ActualPinName, LocalizedSuffixedInputName, SourceSuffixedInputName)));
	}

	TEST_METHOD(ConstantTimesConstantFeedsBaseColor)
	{
		const FAgentGraphImportResult Result = FAgentGraphImporter::Import(Material.Get(), ConstantMultiplyNodes() + RootLinkedToMultiply(BaseColorPinName()), false, false);

		ASSERT_THAT(IsTrue(Result.bSucceeded));
		ASSERT_THAT(IsTrue(Result.Diagnostics.IsEmpty()));
		ASSERT_THAT(AreEqual(2, Result.CreatedExpressions.Num()));
		ASSERT_THAT(AreEqual(2, Material->GetExpressions().Num()));

		UMaterialExpressionConstant* Constant = FindExpression<UMaterialExpressionConstant>(*Material);
		UMaterialExpressionMultiply* Multiply = FindExpression<UMaterialExpressionMultiply>(*Material);
		ASSERT_THAT(IsNotNull(Constant));
		ASSERT_THAT(IsNotNull(Multiply));
		ASSERT_THAT(IsTrue(Multiply->A.Expression == Constant));
		ASSERT_THAT(IsTrue(Material->GetEditorOnlyData()->BaseColor.Expression == Multiply));
	}

	TEST_METHOD(ReplaceRemovesExistingExpressions)
	{
		UMaterialExpression* Existing = UMaterialEditingLibrary::CreateMaterialExpression(Material.Get(), UMaterialExpressionConstant::StaticClass());

		const FAgentGraphImportResult Result = FAgentGraphImporter::Import(Material.Get(), ConstantMultiplyNodes() + RootLinkedToMultiply(BaseColorPinName()), true, false);

		ASSERT_THAT(IsTrue(Result.bSucceeded));
		ASSERT_THAT(IsFalse(Material->GetExpressions().Contains(Existing)));
		ASSERT_THAT(AreEqual(2, Material->GetExpressions().Num()));
	}

	TEST_METHOD(AppendKeepsExistingExpressions)
	{
		UMaterialExpression* Existing = UMaterialEditingLibrary::CreateMaterialExpression(Material.Get(), UMaterialExpressionConstant::StaticClass());

		const FAgentGraphImportResult Result = FAgentGraphImporter::Import(Material.Get(), ConstantMultiplyNodes() + RootLinkedToMultiply(BaseColorPinName()), false, false);

		ASSERT_THAT(IsTrue(Result.bSucceeded));
		ASSERT_THAT(IsTrue(Material->GetExpressions().Contains(Existing)));
		ASSERT_THAT(AreEqual(3, Material->GetExpressions().Num()));
	}

	TEST_METHOD(FunctionInputAndOutputLandInFunction)
	{
		const FAgentGraphImportResult Result = FAgentGraphImporter::Import(Function.Get(), FunctionInputOutputNodes(), false, false);

		ASSERT_THAT(IsTrue(Result.bSucceeded));
		ASSERT_THAT(AreEqual(2, Function->GetExpressions().Num()));

		UMaterialExpressionFunctionInput* Input = FindExpression<UMaterialExpressionFunctionInput>(*Function);
		UMaterialExpressionFunctionOutput* Output = FindExpression<UMaterialExpressionFunctionOutput>(*Function);
		ASSERT_THAT(IsNotNull(Input));
		ASSERT_THAT(IsNotNull(Output));
		ASSERT_THAT(IsTrue(Output->A.Expression == Input));
		ASSERT_THAT(IsTrue(Input->GetOuter() == Function.Get()));
		ASSERT_THAT(IsTrue(Input->Function == Function.Get()));
		ASSERT_THAT(IsNull(Input->Material.Get()));
	}

	TEST_METHOD(RootInFunctionIsRejectedBeforeAnyChange)
	{
		const FAgentGraphImportResult Result = FAgentGraphImporter::Import(Function.Get(), FunctionInputOutputNodes() + RootLinkedToMultiply(BaseColorPinName()), false, false);

		ASSERT_THAT(IsFalse(Result.bSucceeded));
		ASSERT_THAT(IsTrue(AgentGraphImportT3D::HasDiagnostic(Result, EAgentGraphImportDiagnosticKind::RootInFunction)));
		ASSERT_THAT(IsTrue(Function->GetExpressions().IsEmpty()));
	}

	TEST_METHOD(EmptyTextIsEmptyImport)
	{
		const FAgentGraphImportResult Result = FAgentGraphImporter::Import(Material.Get(), FString(), false, false);

		ASSERT_THAT(IsFalse(Result.bSucceeded));
		ASSERT_THAT(IsTrue(AgentGraphImportT3D::HasDiagnostic(Result, EAgentGraphImportDiagnosticKind::EmptyImport)));
		ASSERT_THAT(IsTrue(Material->GetExpressions().IsEmpty()));
	}

	TEST_METHOD(RootPinMissingFromTargetIsReported)
	{
		const FAgentGraphImportResult Result = FAgentGraphImporter::Import(Material.Get(), ConstantMultiplyNodes() + RootLinkedToMultiply(MissingRootPinName), false, false);

		ASSERT_THAT(IsTrue(Result.bSucceeded));
		ASSERT_THAT(IsTrue(AgentGraphImportT3D::HasDiagnostic(Result, EAgentGraphImportDiagnosticKind::UnmatchedRootPin)));
		ASSERT_THAT(IsNull(Material->GetEditorOnlyData()->BaseColor.Expression));
	}
};

}

#endif

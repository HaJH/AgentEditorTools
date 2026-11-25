// Copyright 2026 Jeonghyeon Ha. All Rights Reserved.

#if WITH_AUTOMATION_TESTS

#include "AgentGraphImporter.h"
#include "AgentGraphImportResult.h"
#include "CQTest.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionComment.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Misc/Guid.h"
#include "Tests/Support/AgentGraphImportT3D.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

namespace AgentGraphImportPasteFixupsTests
{

using AgentGraphImportT3D::FPinLink;

static constexpr TCHAR DeclarationNode[] = TEXT("MaterialGraphNode_0");
static constexpr TCHAR UsageNode[] = TEXT("MaterialGraphNode_1");
static constexpr TCHAR FirstParameterNode[] = TEXT("MaterialGraphNode_2");
static constexpr TCHAR SecondParameterNode[] = TEXT("MaterialGraphNode_3");
static constexpr TCHAR CommentNodeName[] = TEXT("MaterialGraphNode_Comment_0");
static constexpr TCHAR DeclarationName[] = TEXT("MaterialExpressionNamedRerouteDeclaration_0");
static constexpr TCHAR UsageName[] = TEXT("MaterialExpressionNamedRerouteUsage_0");
static constexpr TCHAR FirstParameterName[] = TEXT("MaterialExpressionScalarParameter_0");
static constexpr TCHAR SecondParameterName[] = TEXT("MaterialExpressionScalarParameter_1");
static constexpr TCHAR CommentName[] = TEXT("MaterialExpressionComment_0");
static constexpr TCHAR DeclarationInputPin[] = TEXT("A1000000000000000000000000000001");
static constexpr TCHAR DeclarationOutputPin[] = TEXT("A1000000000000000000000000000002");
static constexpr TCHAR UsageOutputPin[] = TEXT("A1000000000000000000000000000003");
static constexpr TCHAR FirstParameterOutputPin[] = TEXT("A1000000000000000000000000000004");
static constexpr TCHAR SecondParameterOutputPin[] = TEXT("A1000000000000000000000000000005");
static constexpr TCHAR DeclarationNodeGuid[] = TEXT("B1000000000000000000000000000001");
static constexpr TCHAR UsageNodeGuid[] = TEXT("B1000000000000000000000000000002");
static constexpr TCHAR FirstParameterNodeGuid[] = TEXT("B1000000000000000000000000000003");
static constexpr TCHAR SecondParameterNodeGuid[] = TEXT("B1000000000000000000000000000004");
static constexpr TCHAR CommentNodeGuid[] = TEXT("B1000000000000000000000000000005");
static constexpr TCHAR CommentGuid[] = TEXT("C1000000000000000000000000000005");
static constexpr TCHAR DeclarationExpressionGuid[] = TEXT("C1000000000000000000000000000001");
static constexpr TCHAR UsageExpressionGuid[] = TEXT("C1000000000000000000000000000002");
static constexpr TCHAR FirstParameterExpressionGuid[] = TEXT("C1000000000000000000000000000003");
static constexpr TCHAR SecondParameterExpressionGuid[] = TEXT("C1000000000000000000000000000004");
static constexpr const TCHAR* ExpressionGuidsInText[] =
{
	DeclarationExpressionGuid,
	UsageExpressionGuid,
	FirstParameterExpressionGuid,
	SecondParameterExpressionGuid
};
static constexpr TCHAR RerouteVariableGuid[] = TEXT("D1000000000000000000000000000001");
static constexpr TCHAR SharedParameterName[] = TEXT("Strength");
static constexpr TCHAR FirstParameterValue[] = TEXT("1.000000");
static constexpr TCHAR SecondParameterValue[] = TEXT("2.000000");
static constexpr TCHAR DeclarationPropertiesFormat[] = TEXT("      VariableGuid=%s\n      MaterialExpressionGuid=%s\n");
static constexpr TCHAR UsagePropertiesFormat[] = TEXT("      DeclarationGuid=%s\n      MaterialExpressionGuid=%s\n");
static constexpr TCHAR ParameterPropertiesFormat[] = TEXT("      DefaultValue=%s\n      ParameterName=\"%s\"\n      MaterialExpressionGuid=%s\n");
static constexpr TCHAR CommentText[] = TEXT("Fixture Note");
static constexpr TCHAR InputPinName[] = TEXT("Input");
static constexpr TCHAR OutputPinName[] = TEXT("Output");

static FString PasteFixupNodes()
{
	using namespace AgentGraphImportT3D;
	const FString DeclarationProperties = FString::Printf(DeclarationPropertiesFormat, RerouteVariableGuid, DeclarationExpressionGuid);
	const FString UsageProperties = FString::Printf(UsagePropertiesFormat, RerouteVariableGuid, UsageExpressionGuid);
	const FString FirstParameterProperties = FString::Printf(ParameterPropertiesFormat, FirstParameterValue, SharedParameterName, FirstParameterExpressionGuid);
	const FString SecondParameterProperties = FString::Printf(ParameterPropertiesFormat, SecondParameterValue, SharedParameterName, SecondParameterExpressionGuid);
	return ExpressionNode(DeclarationNode, *UMaterialExpressionNamedRerouteDeclaration::StaticClass(), DeclarationName, DeclarationProperties, 0, DeclarationNodeGuid,
			Pin(DeclarationInputPin, InputPinName, EGPD_Input) + Pin(DeclarationOutputPin, OutputPinName, EGPD_Output))
		+ ExpressionNode(UsageNode, *UMaterialExpressionNamedRerouteUsage::StaticClass(), UsageName, UsageProperties, 300, UsageNodeGuid,
			Pin(UsageOutputPin, OutputPinName, EGPD_Output))
		+ ExpressionNode(FirstParameterNode, *UMaterialExpressionScalarParameter::StaticClass(), FirstParameterName, FirstParameterProperties, 600, FirstParameterNodeGuid,
			Pin(FirstParameterOutputPin, OutputPinName, EGPD_Output))
		+ ExpressionNode(SecondParameterNode, *UMaterialExpressionScalarParameter::StaticClass(), SecondParameterName, SecondParameterProperties, 900, SecondParameterNodeGuid,
			Pin(SecondParameterOutputPin, OutputPinName, EGPD_Output))
		+ CommentNode(CommentNodeName, CommentName, CommentText, CommentGuid, CommentNodeGuid);
}

TEST_CLASS(AgentGraphImporterPasteFixups, "AgentEditorTools.Unit.GraphImport")
{
	TStrongObjectPtr<UMaterial> Material;
	FAgentGraphImportResult Result;

	BEFORE_EACH()
	{
		Material.Reset(NewObject<UMaterial>(GetTransientPackage(), NAME_None, RF_Transient));
		Result = FAgentGraphImporter::Import(Material.Get(), PasteFixupNodes(), false, false);
		ASSERT_THAT(IsTrue(Result.bSucceeded));
	}

	TArray<UMaterialExpressionScalarParameter*> ScalarParameters() const
	{
		TArray<UMaterialExpressionScalarParameter*> Parameters;
		for (UMaterialExpression* Expression : Material->GetExpressions())
		{
			if (UMaterialExpressionScalarParameter* Parameter = Cast<UMaterialExpressionScalarParameter>(Expression))
			{
				Parameters.Add(Parameter);
			}
		}
		return Parameters;
	}

	TEST_METHOD(NamedRerouteUsagePointsAtPastedDeclaration)
	{
		UMaterialExpressionNamedRerouteDeclaration* Declaration = nullptr;
		UMaterialExpressionNamedRerouteUsage* Usage = nullptr;
		for (UMaterialExpression* Expression : Material->GetExpressions())
		{
			if (UMaterialExpressionNamedRerouteDeclaration* AsDeclaration = Cast<UMaterialExpressionNamedRerouteDeclaration>(Expression))
			{
				Declaration = AsDeclaration;
			}
			else if (UMaterialExpressionNamedRerouteUsage* AsUsage = Cast<UMaterialExpressionNamedRerouteUsage>(Expression))
			{
				Usage = AsUsage;
			}
		}

		ASSERT_THAT(IsNotNull(Declaration));
		ASSERT_THAT(IsNotNull(Usage));
		ASSERT_THAT(IsTrue(Usage->Declaration == Declaration));
		ASSERT_THAT(IsTrue(Usage->DeclarationGuid == Declaration->VariableGuid));
	}

	TEST_METHOD(SameNamedParametersKeepTheSharedName)
	{
		const TArray<UMaterialExpressionScalarParameter*> Parameters = ScalarParameters();

		ASSERT_THAT(AreEqual(2, Parameters.Num()));
		ASSERT_THAT(IsTrue(Parameters[0]->ParameterName == FName(SharedParameterName)));
		ASSERT_THAT(IsTrue(Parameters[1]->ParameterName == FName(SharedParameterName)));
	}

	TEST_METHOD(CommentEntersCommentCollection)
	{
		ASSERT_THAT(AreEqual(1, Material->GetEditorComments().Num()));
		ASSERT_THAT(AreEqual(FString(CommentText), Material->GetEditorComments()[0]->Text));
	}

	TEST_METHOD(PastedExpressionGuidsDifferFromText)
	{
		TArray<FGuid> TextGuids;
		for (const TCHAR* GuidText : ExpressionGuidsInText)
		{
			FGuid& TextGuid = TextGuids.AddDefaulted_GetRef();
			ASSERT_THAT(IsTrue(FGuid::Parse(GuidText, TextGuid)));
		}

		ASSERT_THAT(AreEqual(4, Material->GetExpressions().Num()));
		for (const UMaterialExpression* Expression : Material->GetExpressions())
		{
			ASSERT_THAT(IsTrue(Expression->MaterialExpressionGuid.IsValid()));
			ASSERT_THAT(IsFalse(TextGuids.Contains(Expression->MaterialExpressionGuid)));
		}
	}
};

}

#endif

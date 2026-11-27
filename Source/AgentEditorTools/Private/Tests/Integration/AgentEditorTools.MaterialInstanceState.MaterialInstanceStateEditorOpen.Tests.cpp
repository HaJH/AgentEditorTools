// Copyright 2026 Jeonghyeon Ha. All Rights Reserved.

#if WITH_AUTOMATION_TESTS

#include "AgentMaterialInstanceState.h"
#include "AgentMaterialInstanceStateApplier.h"
#include "AgentMaterialInstanceStateResult.h"
#include "CQTest.h"
#include "Editor.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialParameters.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Tests/Support/AgentFakeAssetEditor.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

namespace MaterialInstanceStateEditorOpenTests
{

using AgentFakeAssetEditor::FFakeAssetEditor;

static constexpr TCHAR Label[] = TEXT("Editor open test");
static constexpr TCHAR ScalarName[] = TEXT("Amount");
static constexpr float ExistingValue = 0.25f;
static constexpr float RequestedValue = 0.75f;

TEST_CLASS(MaterialInstanceStateEditorOpen, "AgentEditorTools.Integration.MaterialInstanceState")
{
	TStrongObjectPtr<UMaterial> Material;
	TStrongObjectPtr<UMaterialInstanceConstant> Instance;
	UAssetEditorSubsystem* AssetEditors = nullptr;
	FFakeAssetEditor FakeEditor;
	bool bFakeEditorOpen = false;

	BEFORE_EACH()
	{
		ASSERT_THAT(IsNotNull(GEditor));
		AssetEditors = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
		ASSERT_THAT(IsNotNull(AssetEditors));

		Material.Reset(NewObject<UMaterial>(GetTransientPackage(), NAME_None, RF_Transient));
		UMaterialEditingLibrary::CreateMaterialExpression(Material.Get(), UMaterialExpressionScalarParameter::StaticClass())->SetParameterName(ScalarName);
		Material->UpdateCachedExpressionData();

		Instance.Reset(NewObject<UMaterialInstanceConstant>(GetTransientPackage(), NAME_None, RF_Transient));
		Instance->SetParentEditorOnly(Material.Get(), false);
		Instance->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(ScalarName), ExistingValue);
	}

	AFTER_EACH()
	{
		if (bFakeEditorOpen)
		{
			AssetEditors->NotifyEditorClosed(&FakeEditor);
			bFakeEditorOpen = false;
		}
	}

	TEST_METHOD(OpenEditorRejectsStateWithoutChange)
	{
		AssetEditors->NotifyAssetOpened(Instance.Get(), &FakeEditor);
		bFakeEditorOpen = true;

		FAgentMaterialInstanceState State;
		State.Label = Label;
		State.Instance = Instance->GetPathName();
		FAgentMaterialInstanceWrite& Write = State.Writes.AddDefaulted_GetRef();
		Write.Name = ScalarName;
		Write.Kind = EAgentMaterialInstanceWriteKind::Scalar;
		Write.ScalarValue = RequestedValue;

		const FAgentMaterialInstanceStateResult Result = FAgentMaterialInstanceStateApplier::Apply(State, true);

		float Value = 0.0f;
		ASSERT_THAT(IsFalse(Result.bSucceeded));
		ASSERT_THAT(IsTrue(Result.Diagnostics.ContainsByPredicate([](const FAgentMaterialInstanceStateDiagnostic& Diagnostic)
		{
			return Diagnostic.Kind == EAgentMaterialInstanceStateDiagnosticKind::EditorOpen;
		})));
		ASSERT_THAT(IsFalse(Result.bWritten));
		ASSERT_THAT(IsTrue(Instance->GetScalarParameterValue(FHashedMaterialParameterInfo(FName(ScalarName)), Value, true)));
		ASSERT_THAT(AreEqual(ExistingValue, Value));
	}
};

}

#endif

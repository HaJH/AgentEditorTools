// Copyright 2026 Jeonghyeon Ha. All Rights Reserved.

#if WITH_AUTOMATION_TESTS

#include "AgentGraphImporter.h"
#include "AgentGraphImportResult.h"
#include "CQTest.h"
#include "Editor.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Tests/Support/AgentGraphImportT3D.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

namespace AgentGraphImportEditorOpenTests
{

static constexpr TCHAR FakeEditorName[] = TEXT("AgentGraphImportFakeEditor");
static constexpr TCHAR ConstantNode[] = TEXT("MaterialGraphNode_0");
static constexpr TCHAR ConstantName[] = TEXT("MaterialExpressionConstant_0");
static constexpr TCHAR ConstantOutputPin[] = TEXT("A2000000000000000000000000000001");
static constexpr TCHAR ConstantNodeGuid[] = TEXT("B2000000000000000000000000000001");
static constexpr TCHAR ConstantProperties[] = TEXT("      R=1.000000\n      MaterialExpressionGuid=C2000000000000000000000000000001\n");
static constexpr TCHAR OutputPinName[] = TEXT("Output");

class FFakeAssetEditor final : public IAssetEditorInstance
{
public:
	virtual FName GetEditorName() const override { return FName(FakeEditorName); }
	virtual void FocusWindow(UObject* ObjectToFocusOn) override {}
	virtual bool CloseWindow(EAssetEditorCloseReason InCloseReason) override { return true; }
	virtual bool IncludeAssetInRestoreOpenAssetsPrompt(UObject* Asset) const override { return false; }
	virtual bool IsPrimaryEditor() const override { return true; }
	virtual void InvokeTab(const FTabId& TabId) override {}
	virtual TSharedPtr<FTabManager> GetAssociatedTabManager() override { return nullptr; }
	virtual double GetLastActivationTime() override { return 0.0; }
	virtual void RemoveEditingAsset(UObject* Asset) override {}
};

TEST_CLASS(AgentGraphImporterEditorOpen, "AgentEditorTools.Integration.GraphImport")
{
	TStrongObjectPtr<UMaterial> Material;
	UAssetEditorSubsystem* AssetEditors = nullptr;
	FFakeAssetEditor FakeEditor;
	bool bFakeEditorOpen = false;

	BEFORE_EACH()
	{
		ASSERT_THAT(IsNotNull(GEditor));
		AssetEditors = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
		ASSERT_THAT(IsNotNull(AssetEditors));
		Material.Reset(NewObject<UMaterial>(GetTransientPackage(), NAME_None, RF_Transient));
	}

	AFTER_EACH()
	{
		if (bFakeEditorOpen)
		{
			AssetEditors->NotifyEditorClosed(&FakeEditor);
			bFakeEditorOpen = false;
		}
	}

	TEST_METHOD(OpenEditorRejectsImportWithoutChange)
	{
		using namespace AgentGraphImportT3D;
		UMaterialExpression* Existing = UMaterialEditingLibrary::CreateMaterialExpression(Material.Get(), UMaterialExpressionConstant::StaticClass());
		AssetEditors->NotifyAssetOpened(Material.Get(), &FakeEditor);
		bFakeEditorOpen = true;

		const FString Text = ExpressionNode(ConstantNode, *UMaterialExpressionConstant::StaticClass(), ConstantName, ConstantProperties, 0, ConstantNodeGuid,
			Pin(ConstantOutputPin, OutputPinName, EGPD_Output));
		const FAgentGraphImportResult Result = FAgentGraphImporter::Import(Material.Get(), Text, true, false);

		ASSERT_THAT(IsFalse(Result.bSucceeded));
		ASSERT_THAT(IsTrue(HasDiagnostic(Result, EAgentGraphImportDiagnosticKind::EditorOpen)));
		ASSERT_THAT(AreEqual(1, Material->GetExpressions().Num()));
		ASSERT_THAT(IsTrue(Material->GetExpressions().Contains(Existing)));
	}
};

}

#endif

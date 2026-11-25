// Copyright 2026 Jeonghyeon Ha. All Rights Reserved.

#if WITH_AUTOMATION_TESTS

#include "AgentGraphImporter.h"
#include "AgentGraphImportResult.h"
#include "CQTest.h"
#include "HAL/FileManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Tests/Support/AgentGraphImportT3D.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"

namespace AgentGraphImportFileTests
{

static constexpr TCHAR TestDirectoryName[] = TEXT("AgentGraphImportFileTests");
static constexpr TCHAR T3DFileExtension[] = TEXT(".t3d");
static constexpr TCHAR MissingAssetSuffix[] = TEXT("_Missing");
static constexpr TCHAR PackagePathSeparator[] = TEXT("/");
static constexpr TCHAR ConstantNode[] = TEXT("MaterialGraphNode_0");
static constexpr TCHAR ConstantOutputPin[] = TEXT("A0000000000000000000000000000001");
static constexpr TCHAR ConstantNodeGuid[] = TEXT("B0000000000000000000000000000001");
static constexpr TCHAR ConstantProperties[] = TEXT("      R=2.000000\n      MaterialExpressionGuid=C0000000000000000000000000000001\n");
static constexpr TCHAR ConstantName[] = TEXT("MaterialExpressionConstant_0");
static constexpr TCHAR OutputPinName[] = TEXT("Output");

static FString ConstantNodeText()
{
	using namespace AgentGraphImportT3D;
	return ExpressionNode(ConstantNode, *UMaterialExpressionConstant::StaticClass(), ConstantName, ConstantProperties, 0, ConstantNodeGuid,
		Pin(ConstantOutputPin, OutputPinName, EGPD_Output));
}

TEST_CLASS(AgentGraphImportFile, "AgentEditorTools.Unit.McpToolset")
{
	TStrongObjectPtr<UMaterial> Material;
	FString T3DFilePath;

	BEFORE_EACH()
	{
		Material.Reset(NewObject<UMaterial>(GetTransientPackage(), NAME_None, RF_Transient));
		T3DFilePath = FPaths::Combine(FPaths::AutomationTransientDir(), TestDirectoryName, FGuid::NewGuid().ToString() + T3DFileExtension);
	}

	AFTER_EACH()
	{
		IFileManager::Get().Delete(*T3DFilePath);
	}

	void WriteT3DFile() const
	{
		verify(FFileHelper::SaveStringToFile(ConstantNodeText(), *T3DFilePath));
	}

	TEST_METHOD(ObjectPathImportsFileIntoAsset)
	{
		WriteT3DFile();

		const FAgentGraphImportResult Result = FAgentGraphImporter::ImportFile(Material->GetPathName(), T3DFilePath, false, false);

		ASSERT_THAT(IsTrue(Result.bSucceeded));
		ASSERT_THAT(AreEqual(Material->GetPathName(), Result.Asset));
		ASSERT_THAT(AreEqual(1, Material->GetExpressions().Num()));
	}

	TEST_METHOD(PackagePathResolvesAssetNamedAfterPackage)
	{
		const FName AssetName = MakeUniqueObjectName(GetTransientPackage(), UMaterial::StaticClass());
		UPackage* Package = CreatePackage(*(GetTransientPackage()->GetName() + PackagePathSeparator + AssetName.ToString()));
		const TStrongObjectPtr<UMaterial> PackagedMaterial(NewObject<UMaterial>(Package, AssetName, RF_Transient));
		WriteT3DFile();

		const FAgentGraphImportResult Result = FAgentGraphImporter::ImportFile(Package->GetName(), T3DFilePath, false, false);

		ASSERT_THAT(IsTrue(Result.bSucceeded));
		ASSERT_THAT(AreEqual(PackagedMaterial->GetPathName(), Result.Asset));
		ASSERT_THAT(AreEqual(1, PackagedMaterial->GetExpressions().Num()));
	}

	TEST_METHOD(MissingAssetIsAssetNotFound)
	{
		WriteT3DFile();

		const FAgentGraphImportResult Result = FAgentGraphImporter::ImportFile(Material->GetPathName() + MissingAssetSuffix, T3DFilePath, false, false);

		ASSERT_THAT(IsFalse(Result.bSucceeded));
		ASSERT_THAT(IsTrue(AgentGraphImportT3D::HasDiagnostic(Result, EAgentGraphImportDiagnosticKind::AssetNotFound)));
		ASSERT_THAT(IsTrue(Material->GetExpressions().IsEmpty()));
	}

	TEST_METHOD(UnreadableFileIsFileReadFailed)
	{
		const FAgentGraphImportResult Result = FAgentGraphImporter::ImportFile(Material->GetPathName(), T3DFilePath, false, false);

		ASSERT_THAT(IsFalse(Result.bSucceeded));
		ASSERT_THAT(IsTrue(AgentGraphImportT3D::HasDiagnostic(Result, EAgentGraphImportDiagnosticKind::FileReadFailed)));
		ASSERT_THAT(IsFalse(AgentGraphImportT3D::HasDiagnostic(Result, EAgentGraphImportDiagnosticKind::AssetNotFound)));
		ASSERT_THAT(IsTrue(Material->GetExpressions().IsEmpty()));
	}
};

}

#endif

// Copyright 2026 Jeonghyeon Ha. All Rights Reserved.

#if WITH_AUTOMATION_TESTS

#include "AgentMaterialInstanceState.h"
#include "AgentMaterialInstanceStateApplier.h"
#include "AgentMaterialInstanceStateResult.h"
#include "CQTest.h"
#include "Engine/EngineTypes.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureDefines.h"
#include "HAL/FileManager.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialParameters.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Templates/SubclassOf.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"

namespace MaterialInstanceStateApplierTests
{

static constexpr TCHAR TestPackageRoot[] = TEXT("/Temp/AgentMaterialInstanceStateTests");
static constexpr TCHAR PackagePathSeparator[] = TEXT("/");
static constexpr TCHAR MaterialName[] = TEXT("M_StateTest");
static constexpr TCHAR InstanceName[] = TEXT("MI_StateTest");
static constexpr TCHAR ColorTextureName[] = TEXT("T_StateTestColor");
static constexpr TCHAR LinearTextureName[] = TEXT("T_StateTestLinear");
static constexpr TCHAR MasksTextureName[] = TEXT("T_StateTestMasks");
static constexpr TCHAR Label[] = TEXT("Applier test");
static constexpr TCHAR ScalarA[] = TEXT("ScalarA");
static constexpr TCHAR ScalarB[] = TEXT("ScalarB");
static constexpr TCHAR Tint[] = TEXT("Tint");
static constexpr TCHAR TextureA[] = TEXT("TextureA");
static constexpr TCHAR TextureB[] = TEXT("TextureB");
static constexpr TCHAR FlagSwitch[] = TEXT("bFlag");
static constexpr TCHAR LinearSwitch[] = TEXT("bTexture_LinearColor");
static constexpr TCHAR MasksSwitch[] = TEXT("bTexture_Masks");
static constexpr TCHAR MissingParameter[] = TEXT("NoSuchParameter");
static constexpr float ScalarAValue = 0.6f;
static constexpr float ScalarBValue = 0.25f;
static constexpr int32 FullWriteCount = 4;
static const FLinearColor TintValue(1.0f, 0.5f, 0.2f, 1.0f);

static FAgentMaterialInstanceWrite SwitchWrite(const TCHAR* Name, bool bValue)
{
	FAgentMaterialInstanceWrite Write;
	Write.Name = Name;
	Write.Kind = EAgentMaterialInstanceWriteKind::Switch;
	Write.bSwitchValue = bValue;
	return Write;
}

static FAgentMaterialInstanceWrite ScalarWrite(const TCHAR* Name, float Value)
{
	FAgentMaterialInstanceWrite Write;
	Write.Name = Name;
	Write.Kind = EAgentMaterialInstanceWriteKind::Scalar;
	Write.ScalarValue = Value;
	return Write;
}

static FAgentMaterialInstanceWrite VectorWrite(const TCHAR* Name, const FLinearColor& Value)
{
	FAgentMaterialInstanceWrite Write;
	Write.Name = Name;
	Write.Kind = EAgentMaterialInstanceWriteKind::Vector;
	Write.VectorValue = Value;
	return Write;
}

static FAgentMaterialInstanceWrite TextureWrite(const TCHAR* Name, const UTexture& Texture)
{
	FAgentMaterialInstanceWrite Write;
	Write.Name = Name;
	Write.Kind = EAgentMaterialInstanceWriteKind::Texture;
	Write.TexturePath = Texture.GetPathName();
	return Write;
}

static FAgentMaterialInstanceWrite TextureWriteWithSamplers(const TCHAR* Name, const UTexture& Texture)
{
	FAgentMaterialInstanceWrite Write = TextureWrite(Name, Texture);
	Write.bHasSamplers = true;
	Write.Samplers = { { SAMPLERTYPE_Color, NAME_None }, { SAMPLERTYPE_LinearColor, FName(LinearSwitch) }, { SAMPLERTYPE_Masks, FName(MasksSwitch) } };
	return Write;
}

static bool HasDiagnostic(const FAgentMaterialInstanceStateResult& Result, EAgentMaterialInstanceStateDiagnosticKind Kind)
{
	return Result.Diagnostics.ContainsByPredicate([Kind](const FAgentMaterialInstanceStateDiagnostic& Diagnostic)
	{
		return Diagnostic.Kind == Kind;
	});
}

TEST_CLASS(MaterialInstanceStateApplier, "AgentEditorTools.Unit.MaterialInstanceState")
{
	FString PackageRoot;
	TArray<TStrongObjectPtr<UObject>> Assets;
	UMaterial* Material = nullptr;
	UMaterialInstanceConstant* Instance = nullptr;
	UTexture2D* ColorTexture = nullptr;
	UTexture2D* LinearTexture = nullptr;
	UTexture2D* MasksTexture = nullptr;

	template <typename AssetType>
	AssetType* CreateTestAsset(const TCHAR* Name)
	{
		UPackage* Package = CreatePackage(*(PackageRoot + PackagePathSeparator + Name));
		AssetType* Asset = NewObject<AssetType>(Package, Name, RF_Public | RF_Standalone);
		Assets.Emplace(Asset);
		return Asset;
	}

	UTexture2D* CreateTestTexture(const TCHAR* Name, TextureCompressionSettings Compression, bool bSRGB)
	{
		UTexture2D* Texture = CreateTestAsset<UTexture2D>(Name);
		Texture->CompressionSettings = Compression;
		Texture->SRGB = bSRGB;
		return Texture;
	}

	void AddParameter(TSubclassOf<UMaterialExpression> ExpressionClass, const TCHAR* Name)
	{
		UMaterialExpression* Expression = UMaterialEditingLibrary::CreateMaterialExpression(Material, ExpressionClass);
		Expression->SetParameterName(Name);
	}

	BEFORE_EACH()
	{
		PackageRoot = FString(TestPackageRoot) + PackagePathSeparator + FGuid::NewGuid().ToString();

		Material = CreateTestAsset<UMaterial>(MaterialName);
		AddParameter(UMaterialExpressionScalarParameter::StaticClass(), ScalarA);
		AddParameter(UMaterialExpressionScalarParameter::StaticClass(), ScalarB);
		AddParameter(UMaterialExpressionVectorParameter::StaticClass(), Tint);
		AddParameter(UMaterialExpressionTextureSampleParameter2D::StaticClass(), TextureA);
		AddParameter(UMaterialExpressionTextureSampleParameter2D::StaticClass(), TextureB);
		AddParameter(UMaterialExpressionStaticSwitchParameter::StaticClass(), FlagSwitch);
		AddParameter(UMaterialExpressionStaticSwitchParameter::StaticClass(), LinearSwitch);
		AddParameter(UMaterialExpressionStaticSwitchParameter::StaticClass(), MasksSwitch);
		Material->UpdateCachedExpressionData();

		Instance = CreateTestAsset<UMaterialInstanceConstant>(InstanceName);
		Instance->SetParentEditorOnly(Material, false);

		ColorTexture = CreateTestTexture(ColorTextureName, TC_Default, true);
		LinearTexture = CreateTestTexture(LinearTextureName, TC_Default, false);
		MasksTexture = CreateTestTexture(MasksTextureName, TC_Masks, false);
	}

	AFTER_EACH()
	{
		for (const TStrongObjectPtr<UObject>& Asset : Assets)
		{
			Asset->ClearFlags(RF_Public | RF_Standalone);
			Asset->MarkAsGarbage();
		}
		Assets.Empty();

		FString Directory;
		if (FPackageName::TryConvertLongPackageNameToFilename(PackageRoot, Directory))
		{
			IFileManager::Get().DeleteDirectory(*Directory, false, true);
		}
	}

	FAgentMaterialInstanceState MakeState(TArray<FAgentMaterialInstanceWrite> Writes) const
	{
		FAgentMaterialInstanceState State;
		State.Label = Label;
		State.Instance = Instance->GetPathName();
		State.Writes = MoveTemp(Writes);
		return State;
	}

	bool HasScalarOverride(const TCHAR* Name, float& OutValue) const
	{
		return Instance->GetScalarParameterValue(FHashedMaterialParameterInfo(FName(Name)), OutValue, true);
	}

	bool SwitchIs(const TCHAR* Name, bool bExpected) const
	{
		bool bValue = false;
		FGuid ExpressionGuid;
		return Instance->GetStaticSwitchParameterValue(FHashedMaterialParameterInfo(FName(Name)), bValue, ExpressionGuid) && bValue == bExpected;
	}

	TEST_METHOD(UnknownParameterLeavesInstanceUnchanged)
	{
		Instance->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(ScalarB), ScalarBValue);

		const FAgentMaterialInstanceStateResult Result = FAgentMaterialInstanceStateApplier::Apply(MakeState({ ScalarWrite(MissingParameter, ScalarAValue), ScalarWrite(ScalarA, ScalarAValue) }), true);

		float Value = 0.0f;
		ASSERT_THAT(IsFalse(Result.bSucceeded));
		ASSERT_THAT(IsTrue(HasDiagnostic(Result, EAgentMaterialInstanceStateDiagnosticKind::UnknownParameter)));
		ASSERT_THAT(IsFalse(Result.bWritten));
		ASSERT_THAT(IsFalse(Result.bSaved));
		ASSERT_THAT(IsTrue(HasScalarOverride(ScalarB, Value)));
		ASSERT_THAT(IsNear(ScalarBValue, Value, KINDA_SMALL_NUMBER));
		ASSERT_THAT(IsFalse(HasScalarOverride(ScalarA, Value)));
	}

	TEST_METHOD(CheckOnlyLeavesInstanceUnchanged)
	{
		Instance->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(ScalarB), ScalarBValue);

		const FAgentMaterialInstanceStateResult Result = FAgentMaterialInstanceStateApplier::Apply(MakeState({ ScalarWrite(ScalarA, ScalarAValue) }), false);

		float Value = 0.0f;
		ASSERT_THAT(IsTrue(Result.bSucceeded));
		ASSERT_THAT(IsFalse(Result.bCreated));
		ASSERT_THAT(IsFalse(Result.bWritten));
		ASSERT_THAT(IsFalse(Result.bSaved));
		ASSERT_THAT(AreEqual(0, Result.Writes));
		ASSERT_THAT(IsTrue(HasScalarOverride(ScalarB, Value)));
		ASSERT_THAT(IsFalse(HasScalarOverride(ScalarA, Value)));
	}

	TEST_METHOD(WriteReplacesOverridesWithState)
	{
		Instance->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(ScalarB), ScalarBValue);

		const FAgentMaterialInstanceStateResult Result = FAgentMaterialInstanceStateApplier::Apply(MakeState({
			SwitchWrite(FlagSwitch, true),
			ScalarWrite(ScalarA, ScalarAValue),
			VectorWrite(Tint, TintValue),
			TextureWrite(TextureA, *ColorTexture) }), true);

		float Value = 0.0f;
		FLinearColor Color;
		UTexture* Texture = nullptr;
		ASSERT_THAT(IsTrue(Result.bSucceeded));
		ASSERT_THAT(IsTrue(Result.bWritten));
		ASSERT_THAT(IsTrue(Result.bSaved));
		ASSERT_THAT(AreEqual(FullWriteCount, Result.Writes));
		ASSERT_THAT(IsFalse(HasScalarOverride(ScalarB, Value)));
		ASSERT_THAT(IsTrue(HasScalarOverride(ScalarA, Value)));
		ASSERT_THAT(IsNear(ScalarAValue, Value, KINDA_SMALL_NUMBER));
		ASSERT_THAT(IsTrue(Instance->GetVectorParameterValue(FHashedMaterialParameterInfo(FName(Tint)), Color, true)));
		ASSERT_THAT(IsTrue(Color.Equals(TintValue)));
		ASSERT_THAT(IsTrue(Instance->GetTextureParameterValue(FHashedMaterialParameterInfo(FName(TextureA)), Texture, true)));
		ASSERT_THAT(IsTrue(Texture == ColorTexture));
		ASSERT_THAT(IsTrue(SwitchIs(FlagSwitch, true)));
	}

	TEST_METHOD(SamplerTurnsOnMatchingSwitchOnly)
	{
		const FAgentMaterialInstanceStateResult Result = FAgentMaterialInstanceStateApplier::Apply(MakeState({
			SwitchWrite(MasksSwitch, true),
			TextureWriteWithSamplers(TextureA, *LinearTexture) }), true);

		ASSERT_THAT(IsTrue(Result.bSucceeded));
		ASSERT_THAT(IsTrue(SwitchIs(LinearSwitch, true)));
		ASSERT_THAT(IsTrue(SwitchIs(MasksSwitch, false)));
		ASSERT_THAT(AreEqual(1, Result.SamplerSwitches.Num()));
		ASSERT_THAT(AreEqual(FString(LinearSwitch), Result.SamplerSwitches[0]));
	}

	TEST_METHOD(NullSamplerTurnsOffListedSwitches)
	{
		const FAgentMaterialInstanceStateResult Result = FAgentMaterialInstanceStateApplier::Apply(MakeState({
			SwitchWrite(LinearSwitch, true),
			SwitchWrite(MasksSwitch, true),
			TextureWriteWithSamplers(TextureA, *ColorTexture) }), true);

		ASSERT_THAT(IsTrue(Result.bSucceeded));
		ASSERT_THAT(IsTrue(SwitchIs(LinearSwitch, false)));
		ASSERT_THAT(IsTrue(SwitchIs(MasksSwitch, false)));
		ASSERT_THAT(IsTrue(Result.SamplerSwitches.IsEmpty()));
	}

	TEST_METHOD(UnlistedSamplerIsUnsupportedSampler)
	{
		FAgentMaterialInstanceWrite Write = TextureWrite(TextureA, *MasksTexture);
		Write.bHasSamplers = true;
		Write.Samplers = { { SAMPLERTYPE_Color, NAME_None } };

		const FAgentMaterialInstanceStateResult Result = FAgentMaterialInstanceStateApplier::Apply(MakeState({ Write }), true);

		ASSERT_THAT(IsFalse(Result.bSucceeded));
		ASSERT_THAT(IsTrue(HasDiagnostic(Result, EAgentMaterialInstanceStateDiagnosticKind::UnsupportedSampler)));
		ASSERT_THAT(IsFalse(Result.bWritten));
	}

	TEST_METHOD(SameSwitchSetBothWaysIsSamplerConflict)
	{
		const FAgentMaterialInstanceStateResult Result = FAgentMaterialInstanceStateApplier::Apply(MakeState({
			TextureWriteWithSamplers(TextureA, *LinearTexture),
			TextureWriteWithSamplers(TextureB, *ColorTexture) }), true);

		ASSERT_THAT(IsFalse(Result.bSucceeded));
		ASSERT_THAT(IsTrue(HasDiagnostic(Result, EAgentMaterialInstanceStateDiagnosticKind::SamplerConflict)));
		ASSERT_THAT(IsFalse(Result.bWritten));
	}
};

}

#endif

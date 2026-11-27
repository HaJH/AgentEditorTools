// Copyright 2026 Jeonghyeon Ha. All Rights Reserved.

#include "AgentEditorAssetUtils.h"

#include "Editor.h"
#include "Misc/PackageName.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectGlobals.h"

namespace AgentEditorAssetUtilsLocal
{

static constexpr TCHAR ObjectNameSeparator[] = TEXT(".");

}

UObject* AgentEditorAssetUtils::LoadAssetAtPath(const FString& AssetPath)
{
	const FString PackageName = FPackageName::ObjectPathToPackageName(AssetPath);
	if (!FPackageName::IsValidLongPackageName(PackageName))
	{
		return nullptr;
	}
	const FString ObjectPath = PackageName == AssetPath ? AssetPath + AgentEditorAssetUtilsLocal::ObjectNameSeparator + FPackageName::GetShortName(PackageName) : AssetPath;
	return LoadObject<UObject>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
}

bool AgentEditorAssetUtils::IsAssetEditorOpen(UObject& Asset)
{
	if (!GEditor)
	{
		return false;
	}
	UAssetEditorSubsystem* AssetEditors = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	return AssetEditors && !AssetEditors->FindEditorsForAsset(&Asset).IsEmpty();
}

bool AgentEditorAssetUtils::SaveAssetPackage(UObject& Asset)
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

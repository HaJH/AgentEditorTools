// Copyright 2026 Jeonghyeon Ha. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UObject;

namespace AgentEditorAssetUtils
{

UObject* LoadAssetAtPath(const FString& AssetPath);

bool IsAssetEditorOpen(UObject& Asset);

bool SaveAssetPackage(UObject& Asset);

}

// Copyright 2026 Jeonghyeon Ha. All Rights Reserved.

#pragma once

#if WITH_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Subsystems/AssetEditorSubsystem.h"

namespace AgentFakeAssetEditor
{

static constexpr TCHAR FakeEditorName[] = TEXT("AgentFakeAssetEditor");

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

}

#endif

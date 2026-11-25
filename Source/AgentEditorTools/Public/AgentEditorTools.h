// Copyright 2026 Jeonghyeon Ha. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delegates/IDelegateInstance.h"
#include "Modules/ModuleManager.h"
#include "Templates/UniquePtr.h"

class FAgentModalDialogRecorder;

class FAgentEditorToolsModule : public IModuleInterface
{
public:
	FAgentEditorToolsModule();
	virtual ~FAgentEditorToolsModule() override;

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TUniquePtr<FAgentModalDialogRecorder> ModalDialogRecorder;
	FDelegateHandle ModalLoopTickHandle;
	FDelegateHandle WindowBeingDestroyedHandle;
};

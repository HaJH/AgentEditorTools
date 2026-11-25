// Copyright 2026 Jeonghyeon Ha. All Rights Reserved.

#include "AgentEditorTools.h"

#include "AgentModalDialogRecorder.h"
#include "CoreGlobals.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"

FAgentEditorToolsModule::FAgentEditorToolsModule() = default;

FAgentEditorToolsModule::~FAgentEditorToolsModule() = default;

void FAgentEditorToolsModule::StartupModule()
{
	if (!FSlateApplication::IsInitialized() || IsRunningCommandlet())
	{
		return;
	}

	ModalDialogRecorder = MakeUnique<FAgentModalDialogRecorder>(
		&FAgentModalDialogRecorder::QueryActiveSlateModalWindow,
		FAgentModalDialogRecorder::GetDefaultLogFilePath());

	FAgentModalDialogRecorder* Recorder = ModalDialogRecorder.Get();
	FSlateApplication& SlateApplication = FSlateApplication::Get();
	ModalLoopTickHandle = SlateApplication.GetOnModalLoopTickEvent().AddLambda([Recorder](float)
	{
		Recorder->HandleModalLoopTick();
	});
	WindowBeingDestroyedHandle = SlateApplication.OnWindowBeingDestroyed().AddLambda([Recorder](const SWindow& Window)
	{
		Recorder->HandleWindowBeingDestroyed(FAgentModalDialogRecorder::GetWindowId(Window));
	});
}

void FAgentEditorToolsModule::ShutdownModule()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication& SlateApplication = FSlateApplication::Get();
		if (ModalLoopTickHandle.IsValid())
		{
			SlateApplication.GetOnModalLoopTickEvent().Remove(ModalLoopTickHandle);
		}
		if (WindowBeingDestroyedHandle.IsValid())
		{
			SlateApplication.OnWindowBeingDestroyed().Remove(WindowBeingDestroyedHandle);
		}
	}
	ModalLoopTickHandle.Reset();
	WindowBeingDestroyedHandle.Reset();
	ModalDialogRecorder.Reset();
}

IMPLEMENT_MODULE(FAgentEditorToolsModule, AgentEditorTools)

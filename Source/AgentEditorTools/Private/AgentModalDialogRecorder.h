// Copyright 2026 Jeonghyeon Ha. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "Misc/Optional.h"
#include "Templates/Function.h"

class SWindow;

struct FAgentModalWindowInfo
{
	uint64 WindowId = 0;
	FString Title;
	TArray<FString> Texts;
};

class FAgentModalDialogRecorder
{
public:
	using FActiveModalWindowQuery = TFunction<TOptional<FAgentModalWindowInfo>()>;

	FAgentModalDialogRecorder(FActiveModalWindowQuery InActiveModalWindowQuery, FString InLogFilePath);

	void HandleModalLoopTick();
	void HandleWindowBeingDestroyed(uint64 WindowId);

	static FString GetDefaultLogFilePath();
	static uint64 GetWindowId(const SWindow& Window);
	static TOptional<FAgentModalWindowInfo> QueryActiveSlateModalWindow();

	static TOptional<TArray<uint64>> FindUnclosedWindows(const FString& LogContent);

private:
	void AppendLine(const FString& Line) const;

	FActiveModalWindowQuery ActiveModalWindowQuery;
	FString LogFilePath;
	TSet<uint64> OpenWindowIds;
};

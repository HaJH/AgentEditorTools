// Copyright 2026 Jeonghyeon Ha. All Rights Reserved.

#include "AgentModalDialogRecorder.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "Layout/Children.h"
#include "Misc/CString.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Widgets/SWidget.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

namespace AgentModalDialogRecorderLocal
{

static constexpr TCHAR LogDirectoryName[] = TEXT("AgentEditorTools");
static constexpr TCHAR LogFileName[] = TEXT("ModalDialogs.jsonl");
static constexpr TCHAR LineTerminator[] = TEXT("\n");
static constexpr int32 DecimalBase = 10;

static constexpr TCHAR TimeKey[] = TEXT("time");
static constexpr TCHAR PhaseKey[] = TEXT("phase");
static constexpr TCHAR WindowIdKey[] = TEXT("windowId");
static constexpr TCHAR TitleKey[] = TEXT("title");
static constexpr TCHAR TextsKey[] = TEXT("texts");

static constexpr TCHAR OpenedPhase[] = TEXT("opened");
static constexpr TCHAR ClosedPhase[] = TEXT("closed");

using FLineWriter = TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>;
using FLineWriterFactory = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>;

static void WriteLineHeader(FLineWriter& Writer, const TCHAR* Phase, uint64 WindowId)
{
	Writer.WriteObjectStart();
	Writer.WriteValue(TimeKey, FDateTime::UtcNow().ToIso8601());
	Writer.WriteValue(PhaseKey, FStringView(Phase));
	Writer.WriteValue(WindowIdKey, LexToString(WindowId));
}

static FString MakeOpenedLine(const FAgentModalWindowInfo& Window)
{
	FString Line;
	const TSharedRef<FLineWriter> Writer = FLineWriterFactory::Create(&Line);
	WriteLineHeader(*Writer, OpenedPhase, Window.WindowId);
	Writer->WriteValue(TitleKey, Window.Title);
	Writer->WriteValue(TextsKey, Window.Texts);
	Writer->WriteObjectEnd();
	Writer->Close();
	return Line;
}

static FString MakeClosedLine(uint64 WindowId)
{
	FString Line;
	const TSharedRef<FLineWriter> Writer = FLineWriterFactory::Create(&Line);
	WriteLineHeader(*Writer, ClosedPhase, WindowId);
	Writer->WriteObjectEnd();
	Writer->Close();
	return Line;
}

static void CollectTextBlockTexts(SWidget& Widget, TArray<FString>& OutTexts)
{
	if (Widget.GetWidgetClass().GetWidgetType() == STextBlock::StaticWidgetClass().GetWidgetType())
	{
		const FString Text = static_cast<STextBlock&>(Widget).GetText().ToString();
		if (!Text.IsEmpty())
		{
			OutTexts.Add(Text);
		}
	}

	FChildren* Children = Widget.GetChildren();
	if (!Children)
	{
		return;
	}
	for (int32 Index = 0; Index < Children->Num(); ++Index)
	{
		CollectTextBlockTexts(Children->GetChildAt(Index).Get(), OutTexts);
	}
}

}

FAgentModalDialogRecorder::FAgentModalDialogRecorder(FActiveModalWindowQuery InActiveModalWindowQuery, FString InLogFilePath)
	: ActiveModalWindowQuery(MoveTemp(InActiveModalWindowQuery))
	, LogFilePath(MoveTemp(InLogFilePath))
{
	check(ActiveModalWindowQuery);
	ensureMsgf(FFileHelper::SaveStringToFile(FString(), *LogFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM),
		TEXT("Could not empty the modal dialog log %s"), *LogFilePath);
}

void FAgentModalDialogRecorder::HandleModalLoopTick()
{
	const TOptional<FAgentModalWindowInfo> ActiveWindow = ActiveModalWindowQuery();
	if (!ActiveWindow.IsSet() || OpenWindowIds.Contains(ActiveWindow->WindowId))
	{
		return;
	}

	OpenWindowIds.Add(ActiveWindow->WindowId);
	AppendLine(AgentModalDialogRecorderLocal::MakeOpenedLine(*ActiveWindow));
}

void FAgentModalDialogRecorder::HandleWindowBeingDestroyed(uint64 WindowId)
{
	if (OpenWindowIds.Remove(WindowId) == 0)
	{
		return;
	}

	AppendLine(AgentModalDialogRecorderLocal::MakeClosedLine(WindowId));
}

FString FAgentModalDialogRecorder::GetDefaultLogFilePath()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), AgentModalDialogRecorderLocal::LogDirectoryName, AgentModalDialogRecorderLocal::LogFileName);
}

uint64 FAgentModalDialogRecorder::GetWindowId(const SWindow& Window)
{
	return static_cast<uint64>(reinterpret_cast<UPTRINT>(&Window));
}

TOptional<FAgentModalWindowInfo> FAgentModalDialogRecorder::QueryActiveSlateModalWindow()
{
	const TSharedPtr<SWindow> Window = FSlateApplication::Get().GetActiveModalWindow();
	if (!Window.IsValid())
	{
		return {};
	}

	FAgentModalWindowInfo Info;
	Info.WindowId = GetWindowId(*Window);
	Info.Title = Window->GetTitle().ToString();
	AgentModalDialogRecorderLocal::CollectTextBlockTexts(*Window, Info.Texts);
	return Info;
}

TOptional<TArray<uint64>> FAgentModalDialogRecorder::FindUnclosedWindows(const FString& LogContent)
{
	using namespace AgentModalDialogRecorderLocal;

	TArray<FString> Lines;
	LogContent.ParseIntoArrayLines(Lines);

	TArray<uint64> Unclosed;
	for (const FString& Line : Lines)
	{
		TSharedPtr<FJsonObject> Record;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Line), Record) || !Record.IsValid())
		{
			return {};
		}

		FString Phase;
		FString WindowIdText;
		if (!Record->TryGetStringField(PhaseKey, Phase)
			|| !Record->TryGetStringField(WindowIdKey, WindowIdText)
			|| WindowIdText.IsEmpty()
			|| !WindowIdText.IsNumeric())
		{
			return {};
		}
		const uint64 WindowId = FCString::Strtoui64(*WindowIdText, nullptr, DecimalBase);

		if (Phase == OpenedPhase)
		{
			Unclosed.Add(WindowId);
		}
		else if (Phase == ClosedPhase)
		{
			Unclosed.Remove(WindowId);
		}
		else
		{
			return {};
		}
	}
	return Unclosed;
}

void FAgentModalDialogRecorder::AppendLine(const FString& Line) const
{
	ensureMsgf(FFileHelper::SaveStringToFile(Line + AgentModalDialogRecorderLocal::LineTerminator, *LogFilePath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append),
		TEXT("Could not append to the modal dialog log %s"), *LogFilePath);
}

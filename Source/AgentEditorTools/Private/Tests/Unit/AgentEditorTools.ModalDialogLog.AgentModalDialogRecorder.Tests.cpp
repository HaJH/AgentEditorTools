// Copyright 2026 Jeonghyeon Ha. All Rights Reserved.

#if WITH_AUTOMATION_TESTS

#include "AgentModalDialogRecorder.h"
#include "CQTest.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Templates/UniquePtr.h"

namespace AgentModalDialogRecorderTests
{

static constexpr TCHAR TestDirectoryName[] = TEXT("AgentModalDialogRecorderTests");
static constexpr TCHAR LogFileExtension[] = TEXT(".jsonl");
static constexpr TCHAR StaleContent[] = TEXT("{\"phase\":\"opened\",\"windowId\":\"99\"}\n");
static constexpr TCHAR PhaseKey[] = TEXT("phase");
static constexpr TCHAR WindowIdKey[] = TEXT("windowId");
static constexpr TCHAR TitleKey[] = TEXT("title");
static constexpr TCHAR Opened[] = TEXT("opened");
static constexpr TCHAR Closed[] = TEXT("closed");
static constexpr TCHAR WindowAId[] = TEXT("1");
static constexpr TCHAR WindowBId[] = TEXT("2");
static constexpr TCHAR WindowCId[] = TEXT("3");
static constexpr TCHAR WindowATitle[] = TEXT("Window A");
static constexpr uint64 WindowA = 1;
static constexpr uint64 WindowB = 2;
static constexpr uint64 WindowC = 3;
static constexpr uint64 UnrecordedWindow = 42;
static constexpr int32 RepeatedTickCount = 3;

struct FLogLine
{
	FString Phase;
	FString WindowId;
	FString Title;
};

static FAgentModalWindowInfo MakeWindow(uint64 WindowId, const FString& Title)
{
	FAgentModalWindowInfo Window;
	Window.WindowId = WindowId;
	Window.Title = Title;
	return Window;
}

static TArray<FLogLine> ParseLogLines(const FString& Content)
{
	TArray<FString> RawLines;
	Content.ParseIntoArrayLines(RawLines);

	TArray<FLogLine> Lines;
	for (const FString& RawLine : RawLines)
	{
		TSharedPtr<FJsonObject> Record;
		verify(FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(RawLine), Record) && Record.IsValid());
		FLogLine& Line = Lines.AddDefaulted_GetRef();
		Line.Phase = Record->GetStringField(PhaseKey);
		Line.WindowId = Record->GetStringField(WindowIdKey);
		Record->TryGetStringField(TitleKey, Line.Title);
	}
	return Lines;
}

TEST_CLASS(AgentModalDialogRecorder, "AgentEditorTools.Unit.ModalDialogLog")
{
	FString LogFilePath;
	TOptional<FAgentModalWindowInfo> ActiveWindow;

	BEFORE_EACH()
	{
		LogFilePath = FPaths::Combine(FPaths::AutomationTransientDir(), TestDirectoryName, FGuid::NewGuid().ToString() + LogFileExtension);
		ActiveWindow.Reset();
	}

	AFTER_EACH()
	{
		IFileManager::Get().Delete(*LogFilePath);
	}

	TUniquePtr<FAgentModalDialogRecorder> MakeRecorder()
	{
		return MakeUnique<FAgentModalDialogRecorder>([this]() { return ActiveWindow; }, LogFilePath);
	}

	FString ReadLog() const
	{
		FString Content;
		verify(FFileHelper::LoadFileToString(Content, *LogFilePath));
		return Content;
	}

	TEST_METHOD(RepeatedTicksOnSameWindowWriteOneOpenedLine)
	{
		const TUniquePtr<FAgentModalDialogRecorder> Recorder = MakeRecorder();
		ActiveWindow = MakeWindow(WindowA, WindowATitle);

		for (int32 Tick = 0; Tick < RepeatedTickCount; ++Tick)
		{
			Recorder->HandleModalLoopTick();
		}

		const TArray<FLogLine> Lines = ParseLogLines(ReadLog());
		ASSERT_THAT(AreEqual(1, Lines.Num()));
		ASSERT_THAT(AreEqual(FString(Opened), Lines[0].Phase));
		ASSERT_THAT(AreEqual(FString(WindowAId), Lines[0].WindowId));
		ASSERT_THAT(AreEqual(FString(WindowATitle), Lines[0].Title));
	}

	TEST_METHOD(InnerWindowClosedThenAnotherOpenedLeavesOuterAndLatestUnclosed)
	{
		const TUniquePtr<FAgentModalDialogRecorder> Recorder = MakeRecorder();

		ActiveWindow = MakeWindow(WindowA, WindowATitle);
		Recorder->HandleModalLoopTick();
		ActiveWindow = MakeWindow(WindowB, FString());
		Recorder->HandleModalLoopTick();
		Recorder->HandleWindowBeingDestroyed(WindowB);
		ActiveWindow = MakeWindow(WindowC, FString());
		Recorder->HandleModalLoopTick();

		const FString Content = ReadLog();
		const TArray<FLogLine> Lines = ParseLogLines(Content);
		ASSERT_THAT(AreEqual(4, Lines.Num()));
		ASSERT_THAT(AreEqual(FString(Opened), Lines[0].Phase));
		ASSERT_THAT(AreEqual(FString(WindowAId), Lines[0].WindowId));
		ASSERT_THAT(AreEqual(FString(Opened), Lines[1].Phase));
		ASSERT_THAT(AreEqual(FString(WindowBId), Lines[1].WindowId));
		ASSERT_THAT(AreEqual(FString(Closed), Lines[2].Phase));
		ASSERT_THAT(AreEqual(FString(WindowBId), Lines[2].WindowId));
		ASSERT_THAT(AreEqual(FString(Opened), Lines[3].Phase));
		ASSERT_THAT(AreEqual(FString(WindowCId), Lines[3].WindowId));

		const TOptional<TArray<uint64>> Unclosed = FAgentModalDialogRecorder::FindUnclosedWindows(Content);
		ASSERT_THAT(IsTrue(Unclosed.IsSet()));
		ASSERT_THAT(AreEqual(2, Unclosed->Num()));
		ASSERT_THAT(IsTrue((*Unclosed)[0] == WindowA));
		ASSERT_THAT(IsTrue((*Unclosed)[1] == WindowC));
	}

	TEST_METHOD(DestroyingUnrecordedWindowWritesNothing)
	{
		const TUniquePtr<FAgentModalDialogRecorder> Recorder = MakeRecorder();

		Recorder->HandleWindowBeingDestroyed(UnrecordedWindow);

		ASSERT_THAT(IsTrue(ReadLog().IsEmpty()));
	}

	TEST_METHOD(InstallEmptiesPreviousLog)
	{
		ASSERT_THAT(IsTrue(FFileHelper::SaveStringToFile(FString(StaleContent), *LogFilePath)));

		const TUniquePtr<FAgentModalDialogRecorder> Recorder = MakeRecorder();

		ASSERT_THAT(IsTrue(ReadLog().IsEmpty()));
	}
};

}

#endif

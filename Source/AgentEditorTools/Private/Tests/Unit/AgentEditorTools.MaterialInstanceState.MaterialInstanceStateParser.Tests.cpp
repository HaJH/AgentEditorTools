// Copyright 2026 Jeonghyeon Ha. All Rights Reserved.

#if WITH_AUTOMATION_TESTS

#include "AgentMaterialInstanceState.h"
#include "AgentMaterialInstanceStateResult.h"
#include "CQTest.h"

namespace MaterialInstanceStateParserTests
{

static constexpr TCHAR ValidState[] = TEXT(R"({
	"label": "Parser test",
	"instance": "/Temp/ParserTest/MI_X.MI_X",
	"parent": "/Temp/ParserTest/M_X.M_X",
	"create": true,
	"writes": [
		{ "name": "bFlag", "kind": "switch", "value": true },
		{ "name": "Amount", "kind": "scalar", "value": 0.5 },
		{ "name": "Tint", "kind": "vector", "value": [1, 0.5, 0.25, 1] },
		{ "name": "Noise", "kind": "texture", "value": "/Temp/ParserTest/T_X.T_X", "samplers": { "Color": null, "Masks": "bNoise_Masks" } }
	]
})");

static constexpr TCHAR UnknownKindState[] = TEXT(R"({
	"label": "Parser test",
	"instance": "/Temp/ParserTest/MI_X.MI_X",
	"create": false,
	"writes": [ { "name": "Amount", "kind": "curve", "value": 0.5 } ]
})");

static constexpr TCHAR ValueTypeMismatchState[] = TEXT(R"({
	"label": "Parser test",
	"instance": "/Temp/ParserTest/MI_X.MI_X",
	"create": false,
	"writes": [ { "name": "Amount", "kind": "scalar", "value": "0.5" } ]
})");

static constexpr TCHAR SwitchName[] = TEXT("bFlag");
static constexpr TCHAR MasksSwitchName[] = TEXT("bNoise_Masks");
static constexpr float ScalarValue = 0.5f;
static constexpr int32 WriteCount = 4;
static constexpr int32 SamplerCount = 2;
static const FLinearColor VectorValue(1.0f, 0.5f, 0.25f, 1.0f);

static bool HasOnlyInvalidState(const TArray<FAgentMaterialInstanceStateDiagnostic>& Diagnostics)
{
	return !Diagnostics.IsEmpty() && !Diagnostics.ContainsByPredicate([](const FAgentMaterialInstanceStateDiagnostic& Diagnostic)
	{
		return Diagnostic.Kind != EAgentMaterialInstanceStateDiagnosticKind::InvalidState;
	});
}

TEST_CLASS(MaterialInstanceStateParser, "AgentEditorTools.Unit.MaterialInstanceState")
{
	TEST_METHOD(ValidStateParsesEveryWrite)
	{
		FAgentMaterialInstanceState State;
		TArray<FAgentMaterialInstanceStateDiagnostic> Diagnostics;

		ASSERT_THAT(IsTrue(FAgentMaterialInstanceState::ParseJson(ValidState, State, Diagnostics)));
		ASSERT_THAT(IsTrue(Diagnostics.IsEmpty()));
		ASSERT_THAT(IsTrue(State.bCreate));
		ASSERT_THAT(AreEqual(WriteCount, State.Writes.Num()));
		ASSERT_THAT(AreEqual(FName(SwitchName), State.Writes[0].Name));
		ASSERT_THAT(IsTrue(State.Writes[0].bSwitchValue));
		ASSERT_THAT(AreEqual(ScalarValue, State.Writes[1].ScalarValue));
		ASSERT_THAT(IsTrue(State.Writes[2].VectorValue.Equals(VectorValue)));
		ASSERT_THAT(IsTrue(State.Writes[3].bHasSamplers));
		ASSERT_THAT(AreEqual(SamplerCount, State.Writes[3].Samplers.Num()));
		ASSERT_THAT(IsTrue(State.Writes[3].Samplers[0].SamplerType == SAMPLERTYPE_Color));
		ASSERT_THAT(IsTrue(State.Writes[3].Samplers[0].Switch.IsNone()));
		ASSERT_THAT(IsTrue(State.Writes[3].Samplers[1].SamplerType == SAMPLERTYPE_Masks));
		ASSERT_THAT(AreEqual(FName(MasksSwitchName), State.Writes[3].Samplers[1].Switch));
	}

	TEST_METHOD(UnknownKindIsInvalidState)
	{
		FAgentMaterialInstanceState State;
		TArray<FAgentMaterialInstanceStateDiagnostic> Diagnostics;

		ASSERT_THAT(IsFalse(FAgentMaterialInstanceState::ParseJson(UnknownKindState, State, Diagnostics)));
		ASSERT_THAT(IsTrue(HasOnlyInvalidState(Diagnostics)));
	}

	TEST_METHOD(ValueTypeMismatchIsInvalidState)
	{
		FAgentMaterialInstanceState State;
		TArray<FAgentMaterialInstanceStateDiagnostic> Diagnostics;

		ASSERT_THAT(IsFalse(FAgentMaterialInstanceState::ParseJson(ValueTypeMismatchState, State, Diagnostics)));
		ASSERT_THAT(IsTrue(HasOnlyInvalidState(Diagnostics)));
	}
};

}

#endif

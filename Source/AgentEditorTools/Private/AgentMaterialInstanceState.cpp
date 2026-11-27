// Copyright 2026 Jeonghyeon Ha. All Rights Reserved.

#include "AgentMaterialInstanceState.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonTypes.h"
#include "UObject/Class.h"
#include "UObject/ReflectedTypeAccessors.h"

namespace AgentMaterialInstanceStateLocal
{

static constexpr TCHAR LabelKey[] = TEXT("label");
static constexpr TCHAR InstanceKey[] = TEXT("instance");
static constexpr TCHAR ParentKey[] = TEXT("parent");
static constexpr TCHAR CreateKey[] = TEXT("create");
static constexpr TCHAR WritesKey[] = TEXT("writes");
static constexpr TCHAR NameKey[] = TEXT("name");
static constexpr TCHAR KindKey[] = TEXT("kind");
static constexpr TCHAR ValueKey[] = TEXT("value");
static constexpr TCHAR SamplersKey[] = TEXT("samplers");
static constexpr TCHAR SamplerTypePrefix[] = TEXT("SAMPLERTYPE_");
static constexpr TCHAR StateWhere[] = TEXT("the state");
static constexpr TCHAR WriteWhereFormat[] = TEXT("writes[%d]");
static constexpr TCHAR SamplersWhereFormat[] = TEXT("%s.%s");
static constexpr TCHAR NotAWriteObjectFormat[] = TEXT("%s must be an object");
static constexpr TCHAR StringType[] = TEXT("a string");
static constexpr TCHAR BooleanType[] = TEXT("a boolean");
static constexpr TCHAR NumberType[] = TEXT("a number");
static constexpr TCHAR ArrayType[] = TEXT("an array");
static constexpr TCHAR ObjectType[] = TEXT("an object");
static constexpr TCHAR StringOrNullType[] = TEXT("a string or null");
static constexpr TCHAR NotAnObjectFormat[] = TEXT("The state is not a JSON object: %s");
static constexpr TCHAR MissingKeyFormat[] = TEXT("%s has no '%s'");
static constexpr TCHAR WrongTypeFormat[] = TEXT("'%s' in %s must be %s");
static constexpr TCHAR UnknownKindFormat[] = TEXT("%s has kind '%s', which is not one of switch, scalar, vector, texture");
static constexpr TCHAR InvalidInstancePathFormat[] = TEXT("instance '%s' is not an asset path");
static constexpr TCHAR VectorSizeFormat[] = TEXT("'value' in %s must be an array of %d numbers");
static constexpr TCHAR UnknownSamplerFormat[] = TEXT("%s lists sampler type '%s', which is not an EMaterialSamplerType name without the SAMPLERTYPE_ prefix");
static constexpr TCHAR SamplersOnNonTextureFormat[] = TEXT("%s has 'samplers' but its kind is not texture");
static constexpr TCHAR EmptySwitchNameFormat[] = TEXT("%s maps sampler type '%s' to an empty switch name");
static constexpr int32 VectorComponentCount = 4;

struct FKindName
{
	EAgentMaterialInstanceWriteKind Kind;
	const TCHAR* Name;
};

static constexpr FKindName KindNames[] =
{
	{ EAgentMaterialInstanceWriteKind::Switch, TEXT("switch") },
	{ EAgentMaterialInstanceWriteKind::Scalar, TEXT("scalar") },
	{ EAgentMaterialInstanceWriteKind::Vector, TEXT("vector") },
	{ EAgentMaterialInstanceWriteKind::Texture, TEXT("texture") }
};

static void AddInvalidState(TArray<FAgentMaterialInstanceStateDiagnostic>& OutDiagnostics, const FString& Detail)
{
	FAgentMaterialInstanceStateDiagnostic& Diagnostic = OutDiagnostics.AddDefaulted_GetRef();
	Diagnostic.Kind = EAgentMaterialInstanceStateDiagnosticKind::InvalidState;
	Diagnostic.Detail = Detail;
}

static TSharedPtr<FJsonValue> FindTypedField(const FJsonObject& Object, const TCHAR* Key, EJson Type, const TCHAR* TypeName, const FString& Where, bool bRequired, TArray<FAgentMaterialInstanceStateDiagnostic>& OutDiagnostics)
{
	TSharedPtr<FJsonValue> Value = Object.TryGetField(Key);
	if (!Value)
	{
		if (bRequired)
		{
			AddInvalidState(OutDiagnostics, FString::Printf(MissingKeyFormat, *Where, Key));
		}
		return nullptr;
	}
	if (Value->Type != Type)
	{
		AddInvalidState(OutDiagnostics, FString::Printf(WrongTypeFormat, Key, *Where, TypeName));
		return nullptr;
	}
	return Value;
}

static bool ReadString(const FJsonObject& Object, const TCHAR* Key, const FString& Where, bool bRequired, FString& OutValue, TArray<FAgentMaterialInstanceStateDiagnostic>& OutDiagnostics)
{
	const TSharedPtr<FJsonValue> Value = FindTypedField(Object, Key, EJson::String, StringType, Where, bRequired, OutDiagnostics);
	return Value && Value->TryGetString(OutValue);
}

static bool ParseKind(const FString& KindName, EAgentMaterialInstanceWriteKind& OutKind)
{
	for (const FKindName& Entry : KindNames)
	{
		if (KindName.Equals(Entry.Name, ESearchCase::CaseSensitive))
		{
			OutKind = Entry.Kind;
			return true;
		}
	}
	return false;
}

static bool ParseSamplerType(const FString& SamplerName, EMaterialSamplerType& OutSamplerType)
{
	const int64 Value = StaticEnum<EMaterialSamplerType>()->GetValueByNameString(FString(SamplerTypePrefix) + SamplerName, EGetByNameFlags::CaseSensitive);
	if (Value == INDEX_NONE || Value == SAMPLERTYPE_MAX)
	{
		return false;
	}
	OutSamplerType = static_cast<EMaterialSamplerType>(Value);
	return true;
}

static void ParseVector(const FJsonValue& Value, const FString& Where, FAgentMaterialInstanceWrite& OutWrite, TArray<FAgentMaterialInstanceStateDiagnostic>& OutDiagnostics)
{
	const TArray<TSharedPtr<FJsonValue>>& Components = Value.AsArray();
	const bool bAllNumbers = !Components.ContainsByPredicate([](const TSharedPtr<FJsonValue>& Component)
	{
		return !Component || Component->Type != EJson::Number;
	});
	if (Components.Num() != VectorComponentCount || !bAllNumbers)
	{
		AddInvalidState(OutDiagnostics, FString::Printf(VectorSizeFormat, *Where, VectorComponentCount));
		return;
	}
	OutWrite.VectorValue = FLinearColor(
		static_cast<float>(Components[0]->AsNumber()),
		static_cast<float>(Components[1]->AsNumber()),
		static_cast<float>(Components[2]->AsNumber()),
		static_cast<float>(Components[3]->AsNumber()));
}

static void ParseValue(const FJsonObject& Object, const FString& Where, FAgentMaterialInstanceWrite& OutWrite, TArray<FAgentMaterialInstanceStateDiagnostic>& OutDiagnostics)
{
	switch (OutWrite.Kind)
	{
	case EAgentMaterialInstanceWriteKind::Switch:
		if (const TSharedPtr<FJsonValue> Value = FindTypedField(Object, ValueKey, EJson::Boolean, BooleanType, Where, true, OutDiagnostics))
		{
			OutWrite.bSwitchValue = Value->AsBool();
		}
		break;
	case EAgentMaterialInstanceWriteKind::Scalar:
		if (const TSharedPtr<FJsonValue> Value = FindTypedField(Object, ValueKey, EJson::Number, NumberType, Where, true, OutDiagnostics))
		{
			OutWrite.ScalarValue = static_cast<float>(Value->AsNumber());
		}
		break;
	case EAgentMaterialInstanceWriteKind::Vector:
		if (const TSharedPtr<FJsonValue> Value = FindTypedField(Object, ValueKey, EJson::Array, ArrayType, Where, true, OutDiagnostics))
		{
			ParseVector(*Value, Where, OutWrite, OutDiagnostics);
		}
		break;
	case EAgentMaterialInstanceWriteKind::Texture:
		ReadString(Object, ValueKey, Where, true, OutWrite.TexturePath, OutDiagnostics);
		break;
	}
}

static void ParseSamplers(const FJsonObject& Object, const FString& Where, FAgentMaterialInstanceWrite& OutWrite, TArray<FAgentMaterialInstanceStateDiagnostic>& OutDiagnostics)
{
	const TSharedPtr<FJsonValue> SamplersValue = FindTypedField(Object, SamplersKey, EJson::Object, ObjectType, Where, false, OutDiagnostics);
	if (!SamplersValue)
	{
		return;
	}
	if (OutWrite.Kind != EAgentMaterialInstanceWriteKind::Texture)
	{
		AddInvalidState(OutDiagnostics, FString::Printf(SamplersOnNonTextureFormat, *Where));
		return;
	}

	OutWrite.bHasSamplers = true;
	const FJsonObject& Samplers = *SamplersValue->AsObject();
	for (const auto& Entry : Samplers.Values)
	{
		const FString SamplerName(*Entry.Key);
		FAgentMaterialInstanceSamplerSwitch SamplerSwitch;
		if (!ParseSamplerType(SamplerName, SamplerSwitch.SamplerType))
		{
			AddInvalidState(OutDiagnostics, FString::Printf(UnknownSamplerFormat, *Where, *SamplerName));
			continue;
		}

		const TSharedPtr<FJsonValue>& SwitchValue = Entry.Value;
		if (SwitchValue && SwitchValue->Type == EJson::String)
		{
			const FString SwitchName = SwitchValue->AsString();
			if (SwitchName.IsEmpty())
			{
				AddInvalidState(OutDiagnostics, FString::Printf(EmptySwitchNameFormat, *Where, *SamplerName));
				continue;
			}
			SamplerSwitch.Switch = FName(*SwitchName);
		}
		else if (!SwitchValue || SwitchValue->Type != EJson::Null)
		{
			AddInvalidState(OutDiagnostics, FString::Printf(WrongTypeFormat, *SamplerName, *FString::Printf(SamplersWhereFormat, *Where, SamplersKey), StringOrNullType));
			continue;
		}
		OutWrite.Samplers.Add(SamplerSwitch);
	}
}

static void ParseWrite(const FJsonValue& WriteValue, const FString& Where, FAgentMaterialInstanceState& OutState, TArray<FAgentMaterialInstanceStateDiagnostic>& OutDiagnostics)
{
	if (WriteValue.Type != EJson::Object)
	{
		AddInvalidState(OutDiagnostics, FString::Printf(NotAWriteObjectFormat, *Where));
		return;
	}
	const FJsonObject& Object = *WriteValue.AsObject();

	FAgentMaterialInstanceWrite Write;
	FString Name;
	if (ReadString(Object, NameKey, Where, true, Name, OutDiagnostics))
	{
		Write.Name = FName(*Name);
	}

	FString KindName;
	if (!ReadString(Object, KindKey, Where, true, KindName, OutDiagnostics))
	{
		return;
	}
	if (!ParseKind(KindName, Write.Kind))
	{
		AddInvalidState(OutDiagnostics, FString::Printf(UnknownKindFormat, *Where, *KindName));
		return;
	}

	ParseValue(Object, Where, Write, OutDiagnostics);
	ParseSamplers(Object, Where, Write, OutDiagnostics);
	OutState.Writes.Add(MoveTemp(Write));
}

}

bool FAgentMaterialInstanceState::ParseJson(const FString& JsonText, FAgentMaterialInstanceState& OutState, TArray<FAgentMaterialInstanceStateDiagnostic>& OutDiagnostics)
{
	using namespace AgentMaterialInstanceStateLocal;

	const int32 InitialDiagnostics = OutDiagnostics.Num();
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root)
	{
		AddInvalidState(OutDiagnostics, FString::Printf(NotAnObjectFormat, *Reader->GetErrorMessage()));
		return false;
	}

	const FString Where(StateWhere);
	ReadString(*Root, LabelKey, Where, true, OutState.Label, OutDiagnostics);
	if (ReadString(*Root, InstanceKey, Where, true, OutState.Instance, OutDiagnostics)
		&& !FPackageName::IsValidLongPackageName(FPackageName::ObjectPathToPackageName(OutState.Instance)))
	{
		AddInvalidState(OutDiagnostics, FString::Printf(InvalidInstancePathFormat, *OutState.Instance));
	}
	ReadString(*Root, ParentKey, Where, false, OutState.Parent, OutDiagnostics);
	if (const TSharedPtr<FJsonValue> Create = FindTypedField(*Root, CreateKey, EJson::Boolean, BooleanType, Where, true, OutDiagnostics))
	{
		OutState.bCreate = Create->AsBool();
	}

	if (const TSharedPtr<FJsonValue> Writes = FindTypedField(*Root, WritesKey, EJson::Array, ArrayType, Where, true, OutDiagnostics))
	{
		const TArray<TSharedPtr<FJsonValue>>& WriteValues = Writes->AsArray();
		for (int32 Index = 0; Index < WriteValues.Num(); ++Index)
		{
			ParseWrite(*WriteValues[Index], FString::Printf(WriteWhereFormat, Index), OutState, OutDiagnostics);
		}
	}

	return OutDiagnostics.Num() == InitialDiagnostics;
}

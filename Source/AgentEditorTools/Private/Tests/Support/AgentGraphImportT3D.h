// Copyright 2026 Jeonghyeon Ha. All Rights Reserved.

#pragma once

#if WITH_AUTOMATION_TESTS

#include "AgentGraphImportResult.h"
#include "Containers/ArrayView.h"
#include "CoreMinimal.h"
#include "EdGraph/EdGraphPin.h"
#include "MaterialGraph/MaterialGraphNode.h"
#include "MaterialGraph/MaterialGraphNode_Comment.h"
#include "MaterialGraph/MaterialGraphNode_Root.h"
#include "Materials/MaterialExpressionComment.h"
#include "UObject/Class.h"

namespace AgentGraphImportT3D
{

static constexpr TCHAR EmptyText[] = TEXT("");
static constexpr TCHAR ExpressionReferenceKey[] = TEXT("MaterialExpression");
static constexpr TCHAR CommentReferenceKey[] = TEXT("MaterialExpressionComment");
static constexpr TCHAR OutputDirection[] = TEXT("Direction=\"EGPD_Output\",");
static constexpr TCHAR OptionalCategory[] = TEXT("optional");
static constexpr TCHAR LinkFormat[] = TEXT("%s %s,");
static constexpr TCHAR LinkedToFormat[] = TEXT("LinkedTo=(%s),");
static constexpr TCHAR PinFormat[] = TEXT("   CustomProperties Pin (PinId=%s,PinName=\"%s\",%sPinType.PinCategory=\"%s\",PinType.PinSubCategory=\"\",PinType.PinSubCategoryObject=None,PinType.PinSubCategoryMemberReference=(),PinType.PinValueType=(),PinType.ContainerType=None,PinType.bIsReference=False,PinType.bIsConst=False,PinType.bIsWeakPointer=False,PinType.bIsUObjectWrapper=False,PinType.bSerializeAsSinglePrecisionFloat=False,%sPersistentGuid=00000000000000000000000000000000,bHidden=False,bNotConnectable=False,bDefaultValueIsReadOnly=False,bDefaultValueIsIgnored=False,bAdvancedView=False,bOrphanedPin=False,)\n");
static constexpr TCHAR NodeBeginFormat[] = TEXT("Begin Object Class=%s Name=\"%s\" ExportPath=%s'\"/Engine/Transient.AgentGraphFixture:MaterialGraph_0.%s\"'\n");
static constexpr TCHAR SubobjectFormat[] = TEXT("   Begin Object Class=%s Name=\"%s\" ExportPath=%s'\"/Engine/Transient.AgentGraphFixture:MaterialGraph_0.%s.%s\"'\n   End Object\n   Begin Object Name=\"%s\" ExportPath=%s'\"/Engine/Transient.AgentGraphFixture:MaterialGraph_0.%s.%s\"'\n%s   End Object\n   %s=%s'\"%s\"'\n");
static constexpr TCHAR NodeBodyFormat[] = TEXT("   NodePosX=%d\n   NodePosY=0\n   NodeGuid=%s\n%s");
static constexpr TCHAR CommentBodyFormat[] = TEXT("   NodePosX=0\n   NodePosY=0\n   NodeWidth=400\n   NodeHeight=200\n   NodeComment=\"%s\"\n   NodeGuid=%s\n");
static constexpr TCHAR CommentPropertiesFormat[] = TEXT("      SizeX=400\n      SizeY=200\n      Text=\"%s\"\n      MaterialExpressionGuid=%s\n");
static constexpr TCHAR NodeEnd[] = TEXT("End Object\n");

struct FPinLink
{
	const TCHAR* NodeName;
	const TCHAR* PinId;
};

inline FString Pin(const TCHAR* PinId, const TCHAR* PinName, EEdGraphPinDirection Direction, TConstArrayView<FPinLink> Links = {})
{
	FString LinkText;
	for (const FPinLink& Link : Links)
	{
		LinkText += FString::Printf(LinkFormat, Link.NodeName, Link.PinId);
	}
	const FString LinkedTo = Links.IsEmpty() ? FString() : FString::Printf(LinkedToFormat, *LinkText);
	const bool bOutput = Direction == EGPD_Output;
	return FString::Printf(PinFormat, PinId, PinName, bOutput ? OutputDirection : EmptyText, bOutput ? EmptyText : OptionalCategory, *LinkedTo);
}

inline FString NodeWithSubobject(const UClass& NodeClass, const TCHAR* NodeName, const UClass& ExpressionClass, const TCHAR* ExpressionName, const TCHAR* ReferenceKey, const FString& ExpressionProperties, const FString& NodeBody)
{
	const FString NodeClassPath = NodeClass.GetPathName();
	const FString ExpressionClassPath = ExpressionClass.GetPathName();
	return FString::Printf(NodeBeginFormat, *NodeClassPath, NodeName, *NodeClassPath, NodeName)
		+ FString::Printf(SubobjectFormat,
			*ExpressionClassPath, ExpressionName, *ExpressionClassPath, NodeName, ExpressionName,
			ExpressionName, *ExpressionClassPath, NodeName, ExpressionName,
			*ExpressionProperties,
			ReferenceKey, *ExpressionClassPath, ExpressionName)
		+ NodeBody
		+ NodeEnd;
}

inline FString ExpressionNode(const TCHAR* NodeName, const UClass& ExpressionClass, const TCHAR* ExpressionName, const FString& ExpressionProperties, int32 NodePosX, const TCHAR* NodeGuid, const FString& Pins)
{
	return NodeWithSubobject(*UMaterialGraphNode::StaticClass(), NodeName, ExpressionClass, ExpressionName, ExpressionReferenceKey, ExpressionProperties,
		FString::Printf(NodeBodyFormat, NodePosX, NodeGuid, *Pins));
}

inline FString CommentNode(const TCHAR* NodeName, const TCHAR* CommentName, const TCHAR* Text, const TCHAR* CommentGuid, const TCHAR* NodeGuid)
{
	return NodeWithSubobject(*UMaterialGraphNode_Comment::StaticClass(), NodeName, *UMaterialExpressionComment::StaticClass(), CommentName, CommentReferenceKey,
		FString::Printf(CommentPropertiesFormat, Text, CommentGuid),
		FString::Printf(CommentBodyFormat, Text, NodeGuid));
}

inline FString RootNode(const TCHAR* NodeName, const TCHAR* NodeGuid, const FString& Pins)
{
	const FString RootClassPath = UMaterialGraphNode_Root::StaticClass()->GetPathName();
	return FString::Printf(NodeBeginFormat, *RootClassPath, NodeName, *RootClassPath, NodeName)
		+ FString::Printf(NodeBodyFormat, 0, NodeGuid, *Pins)
		+ NodeEnd;
}

inline bool HasDiagnostic(const FAgentGraphImportResult& Result, EAgentGraphImportDiagnosticKind Kind)
{
	return Result.Diagnostics.ContainsByPredicate([Kind](const FAgentGraphImportDiagnostic& Diagnostic)
	{
		return Diagnostic.Kind == Kind;
	});
}

}

#endif

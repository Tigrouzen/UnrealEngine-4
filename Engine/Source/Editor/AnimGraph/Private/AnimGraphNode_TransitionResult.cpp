// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphPrivatePCH.h"

#include "GraphEditorActions.h"
#include "ScopedTransaction.h"

/////////////////////////////////////////////////////
// UAnimGraphNode_TransitionResult

UAnimGraphNode_TransitionResult::UAnimGraphNode_TransitionResult(const FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
}

FLinearColor UAnimGraphNode_TransitionResult::GetNodeTitleColor() const
{
	return FLinearColor(1.0f, 0.65f, 0.4f);
}

FString UAnimGraphNode_TransitionResult::GetTooltip() const
{
	return TEXT("This expression is evaluated to determine if the state transition can be taken");
}

FString UAnimGraphNode_TransitionResult::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return TEXT("Result");
}

void UAnimGraphNode_TransitionResult::GetMenuEntries(FGraphContextMenuBuilder& ContextMenuBuilder) const
{
	// Intentionally empty.  This node is autogenerated when a transition graph is created.
}

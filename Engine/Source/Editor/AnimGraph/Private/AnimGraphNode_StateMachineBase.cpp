// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphPrivatePCH.h"

#include "GraphEditorActions.h"
#include "ScopedTransaction.h"
#include "EdGraphUtilities.h"
#include "Kismet2NameValidators.h"

/////////////////////////////////////////////////////
// FAnimStateMachineNodeNameValidator

class FAnimStateMachineNodeNameValidator : public FStringSetNameValidator
{
public:
	FAnimStateMachineNodeNameValidator(const UAnimGraphNode_StateMachineBase* InStateMachineNode)
		: FStringSetNameValidator(FString())
	{
		TArray<UAnimGraphNode_StateMachineBase*> Nodes;

		UAnimationGraph* StateMachine = CastChecked<UAnimationGraph>(InStateMachineNode->GetOuter());
		StateMachine->GetNodesOfClassEx<UAnimGraphNode_StateMachine, UAnimGraphNode_StateMachineBase>(Nodes);

		for (auto NodeIt = Nodes.CreateIterator(); NodeIt; ++NodeIt)
		{
			auto Node = *NodeIt;
			if (Node != InStateMachineNode)
			{
				Names.Add(Node->GetStateMachineName());
			}
		}
	}
};

/////////////////////////////////////////////////////
// UAnimGraphNode_StateMachineBase

UAnimGraphNode_StateMachineBase::UAnimGraphNode_StateMachineBase(const FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
}

FLinearColor UAnimGraphNode_StateMachineBase::GetNodeTitleColor() const
{
	return FLinearColor(0.8f, 0.8f, 0.8f);
}

FString UAnimGraphNode_StateMachineBase::GetTooltip() const
{
	return TEXT("Animation State Machine");
}

FString UAnimGraphNode_StateMachineBase::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	FString FirstLine = (EditorStateMachineGraph != NULL) ? EditorStateMachineGraph->GetName() : TEXT("Error: No Graph");
	if(TitleType == ENodeTitleType::FullTitle)
	{
		return FirstLine + TEXT("\nState Machine");
	}

	return FirstLine;
}

FString UAnimGraphNode_StateMachineBase::GetNodeCategory() const
{
	return TEXT("State Machines");
}

void UAnimGraphNode_StateMachineBase::GetMenuEntries(FGraphContextMenuBuilder& ContextMenuBuilder) const
{
	if ((ContextMenuBuilder.FromPin == NULL) || ((ContextMenuBuilder.FromPin->Direction == EGPD_Input) && (ContextMenuBuilder.FromPin->PinType.PinSubCategoryObject == FPoseLink::StaticStruct())))
	{
		TSharedPtr<FEdGraphSchemaAction_K2NewNode> MenuEntry = CreateDefaultMenuEntry(ContextMenuBuilder);

		MenuEntry->MenuDescription = TEXT("Add New State Machine...");
		MenuEntry->TooltipDescription = TEXT("Create a new state machine");
	}
}

void UAnimGraphNode_StateMachineBase::PostPlacedNewNode()
{
	// Create a new animation graph
	check(EditorStateMachineGraph == NULL);
	EditorStateMachineGraph = CastChecked<UAnimationStateMachineGraph>(FBlueprintEditorUtils::CreateNewGraph(this, NAME_None, UAnimationStateMachineGraph::StaticClass(), UAnimationStateMachineSchema::StaticClass()));
	check(EditorStateMachineGraph);
	EditorStateMachineGraph->OwnerAnimGraphNode = this;

	// Find an interesting name
	TSharedPtr<INameValidatorInterface> NameValidator = FNameValidatorFactory::MakeValidator(this);
	FBlueprintEditorUtils::RenameGraphWithSuggestion(EditorStateMachineGraph, NameValidator, TEXT("New State Machine"));

	// Initialize the anim graph
	const UEdGraphSchema* Schema = EditorStateMachineGraph->GetSchema();
	Schema->CreateDefaultNodesForGraph(*EditorStateMachineGraph);

	// Add the new graph as a child of our parent graph
	GetGraph()->SubGraphs.Add(EditorStateMachineGraph);
}

UObject* UAnimGraphNode_StateMachineBase::GetJumpTargetForDoubleClick() const
{
	// Open the state machine graph
	return EditorStateMachineGraph;
}

void UAnimGraphNode_StateMachineBase::DestroyNode()
{
	UEdGraph* GraphToRemove = EditorStateMachineGraph;

	EditorStateMachineGraph = NULL;
	Super::DestroyNode();

	if (GraphToRemove)
	{
		UBlueprint* Blueprint = GetBlueprint();
		GraphToRemove->Modify();
		FBlueprintEditorUtils::RemoveGraph(Blueprint, GraphToRemove, EGraphRemoveFlags::Recompile);
	}
}

void UAnimGraphNode_StateMachineBase::PostPasteNode()
{
	Super::PostPasteNode();

	// Add the new graph as a child of our parent graph
	UEdGraph* ParentGraph = CastChecked<UEdGraph>(GetGraph());
	ParentGraph->SubGraphs.Add(EditorStateMachineGraph);

	// Find an interesting name
	TSharedPtr<INameValidatorInterface> NameValidator = FNameValidatorFactory::MakeValidator(this);
	FBlueprintEditorUtils::RenameGraphWithSuggestion(EditorStateMachineGraph, NameValidator, EditorStateMachineGraph->GetName());

	//restore transactional flag that is lost during copy/paste process
	EditorStateMachineGraph->SetFlags(RF_Transactional);
}

FString UAnimGraphNode_StateMachineBase::GetStateMachineName()
{
	return (EditorStateMachineGraph != NULL) ? *(EditorStateMachineGraph->GetName()) : TEXT("(null)");
}

TSharedPtr<class INameValidatorInterface> UAnimGraphNode_StateMachineBase::MakeNameValidator() const
{
	return MakeShareable(new FAnimStateMachineNodeNameValidator(this));
}

FString UAnimGraphNode_StateMachineBase::GetDocumentationLink() const
{
	return TEXT("Shared/GraphNodes/AnimationStateMachine");
}

void UAnimGraphNode_StateMachineBase::OnRenameNode(const FString& NewName)
{
	FBlueprintEditorUtils::RenameGraph(EditorStateMachineGraph, NewName);
}


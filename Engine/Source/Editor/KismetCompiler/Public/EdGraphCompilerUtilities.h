// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#ifndef __EdGraphCompilerUtilities_h__
#define __EdGraphCompilerUtilities_h__

#pragma once

#include "Engine.h"
#include "BlueprintUtilities.h"
#include "EdGraphUtilities.h"

DECLARE_STATS_GROUP(TEXT("KismetCompiler"), STATGROUP_KismetCompiler);

DECLARE_CYCLE_STAT_EXTERN(TEXT("Compile Time"), EKismetCompilerStats_CompileTime, STATGROUP_KismetCompiler, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Create Schema"), EKismetCompilerStats_CreateSchema, STATGROUP_KismetCompiler, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Fixup GeneratedClass Refs"), EKismetCompilerStats_ReplaceGraphRefsToGeneratedClass, STATGROUP_KismetCompiler, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Create Function List"), EKismetCompilerStats_CreateFunctionList, STATGROUP_KismetCompiler, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Expansion"), EKismetCompilerStats_Expansion, STATGROUP_KismetCompiler, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Process uber"), EKismetCompilerStats_ProcessUbergraph, STATGROUP_KismetCompiler, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Process func"), EKismetCompilerStats_ProcessFunctionGraph, STATGROUP_KismetCompiler, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Precompile Function"), EKismetCompilerStats_PrecompileFunction, STATGROUP_KismetCompiler, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Compile Function"), EKismetCompilerStats_CompileFunction, STATGROUP_KismetCompiler, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Postcompile Function"), EKismetCompilerStats_PostcompileFunction, STATGROUP_KismetCompiler, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Finalization"), EKismetCompilerStats_FinalizationWork, STATGROUP_KismetCompiler, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Code Gen"), EKismetCompilerStats_CodeGenerationTime, STATGROUP_KismetCompiler, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Update Instances"), EKismetCompilerStats_UpdateBlueprintGeneratedClass, STATGROUP_KismetCompiler, );


//////////////////////////////////////////////////////////////////////////

class FGraphCompilerContext
{
public:
	// Compiler message log (errors, warnings, notes)
	FCompilerResultsLog& MessageLog;

protected:

	// Schema implementation

	/** Validates that the interconnection between two pins is schema compatible */
	virtual void ValidateLink(const UEdGraphPin* PinA, const UEdGraphPin* PinB) const;

	/** Validate that the wiring for a single pin is schema compatible */
	virtual void ValidatePin(const UEdGraphPin* Pin) const;

	/** Validates that the node is schema compatible */
	virtual void ValidateNode(const UEdGraphNode* Node) const;

	/** Can this node be ignored for further processing? */
	virtual bool CanIgnoreNode(const UEdGraphNode* Node) const
	{
		return false;
	}

	/** Should this node be kept even if it's not reached? */
	virtual bool ShouldForceKeepNode(const UEdGraphNode* Node) const
	{
		return false;
	}

	/** Does this pin potentially participate in data dependencies? */
	virtual bool PinIsImportantForDependancies(const UEdGraphPin* Pin) const
	{
		return false;
	}

	FGraphCompilerContext(FCompilerResultsLog& InMessageLog)
		: MessageLog(InMessageLog)
	{
	}

	/** Performs standard validation on the graph (outputs point to inputs, no more than one connection to each input, types match on both ends, etc...) */
	bool ValidateGraphIsWellFormed(UEdGraph* Graph) const;

	/**
	 * Scans a graph for a node of the specified class.  Can optionally continue scanning and print errors if additional nodes of the same category are found.
	 */
	UEdGraphNode* FindNodeByClass(const UEdGraph* Graph, TSubclassOf<UEdGraphNode>  NodeClass, bool bExpectedUnique) const;

	/**
	 * Scans a graph for all nodes of the specified class.
	 */
	void FindNodesByClass(const UEdGraph* Graph, TSubclassOf<UEdGraphNode>  NodeClass, TArray<UEdGraphNode*>& FoundNodes) const;

	/** Prunes any nodes that weren't visited from the graph, printing out a warning */
	virtual void PruneIsolatedNodes(const TArray<UEdGraphNode*>& RootSet, TArray<UEdGraphNode*>& GraphNodes);

	/**
	 * Performs a topological sort on the graph of nodes passed in (which is expected to form a DAG), scheduling them.
	 * If there are cycles or unconnected nodes present in the graph, an error will be output for each node that failed to be scheduled.
	 */
	void CreateExecutionSchedule(const TArray<UEdGraphNode*>& GraphNodes, /*out*/ TArray<UEdGraphNode*>& LinearExecutionSchedule) const;

	/** Counts the number of incoming edges this node has (along all input pins) */
	int32 CountIncomingEdges(const UEdGraphNode* Node) const
	{
		int32 NumEdges = 0;
		for (int32 PinIndex = 0; PinIndex < Node->Pins.Num(); ++PinIndex)
		{
			const UEdGraphPin* Pin = Node->Pins[PinIndex];
			if ((Pin->Direction == EGPD_Input) && PinIsImportantForDependancies(Pin))
			{
				NumEdges += Pin->LinkedTo.Num();
			}
		}

		return NumEdges;
	}
};

//////////////////////////////////////////////////////////////////////////

#endif	// __EdGraphCompilerUtilities_h__

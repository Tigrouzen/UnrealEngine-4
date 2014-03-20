// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#include "BlueprintGraphPrivatePCH.h"
#include "KismetCompiler.h"

#define LOCTEXT_NAMESPACE "K2Node"

UK2Node_CallFunctionOnMember::UK2Node_CallFunctionOnMember(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

}

UEdGraphPin* UK2Node_CallFunctionOnMember::CreateSelfPin(const UFunction* Function) 
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	UEdGraphPin* SelfPin = NULL;
	if (MemberVariableToCallOn.IsSelfContext())
	{
		// This means the function is defined within the blueprint, so the pin should be a true "self" pin
		SelfPin = CreatePin(EGPD_Input, K2Schema->PC_Object, K2Schema->PSC_Self, NULL, false, false, K2Schema->PN_Self);
	}
	else
	{
		// This means that the function is declared in an external class, and should reference that class
		SelfPin = CreatePin(EGPD_Input, K2Schema->PC_Object, TEXT(""), MemberVariableToCallOn.GetMemberParentClass(this), false, false, K2Schema->PN_Self);
	}
	check(SelfPin != NULL);

	return SelfPin;
}

FString UK2Node_CallFunctionOnMember::GetFunctionContextString() const
{
	UClass* MemberVarClass = MemberVariableToCallOn.GetMemberParentClass(this);
	FString CallFunctionClassName = (MemberVarClass != NULL) ? MemberVarClass->GetName() : TEXT("NULL");
	return FString(TEXT("\n")) + FString::Printf(TEXT("%s %s (%s)"), *LOCTEXT("CallFunctionOnMemberDifferentContext", "Target is").ToString(), *CallFunctionClassName, *MemberVariableToCallOn.GetMemberName().ToString());
}

FNodeHandlingFunctor* UK2Node_CallFunctionOnMember::CreateNodeHandler(FKismetCompilerContext& CompilerContext) const
{
	return new FNodeHandlingFunctor(CompilerContext);
}

void UK2Node_CallFunctionOnMember::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	// This skips UK2Node_CallFunction::ExpandNode.  Instead it spawns a new CallFunction node and does hookup that this is interested in,
	// and then that CallFunction node will get its own Expansion to handle the parent portions
	UK2Node::ExpandNode(CompilerContext, SourceGraph);

	if (CompilerContext.bIsFullCompile)
	{
		const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();
		UFunction* Function = GetTargetFunction();

		// Create real 'call function' node.
		UK2Node_CallFunction* CallFuncNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
		CallFuncNode->SetFromFunction(Function);
		CallFuncNode->AllocateDefaultPins();
		UEdGraphPin* CallFuncSelfPin = Schema->FindSelfPin(*CallFuncNode, EGPD_Input);

		// Now because you can wire multiple variables to a self pin, need to iterate over each one and create a 'get var' node for each
		UEdGraphPin* SelfPin = Schema->FindSelfPin(*this, EGPD_Input);
		if(SelfPin != NULL)
		{
			for(int32 TargetIdx = 0; TargetIdx < SelfPin->LinkedTo.Num(); TargetIdx++)
			{
				UEdGraphPin* SourcePin = SelfPin->LinkedTo[TargetIdx];
				if(SourcePin != NULL)
				{
					// Create 'get var' node to get the member
					UK2Node_VariableGet* GetVarNode = CompilerContext.SpawnIntermediateNode<UK2Node_VariableGet>(this, SourceGraph);
					GetVarNode->VariableReference = MemberVariableToCallOn;
					GetVarNode->AllocateDefaultPins();

					UEdGraphPin* VarNodeSelfPin = Schema->FindSelfPin(*GetVarNode, EGPD_Input);
					if( VarNodeSelfPin != NULL )
					{
						VarNodeSelfPin->MakeLinkTo(SourcePin);

						UEdGraphPin* ValuePin = GetVarNode->GetValuePin();
						ValuePin->MakeLinkTo(CallFuncSelfPin);
					}
					else
					{
						// Failed to find the member to call on for this expansion, so warn about it
						CompilerContext.MessageLog.Warning(*LOCTEXT("CallFunctionOnInvalidMember_Warning", "Function node @@ called on invalid target member.").ToString(), this);
					}
				}
			}
		}

		// Now move the rest of the connections (including exec connections...)
		for(int32 SrcPinIdx=0; SrcPinIdx<Pins.Num(); SrcPinIdx++)
		{
			UEdGraphPin* SrcPin = Pins[SrcPinIdx];
			if(SrcPin != NULL && SrcPin != SelfPin) // check its not the self pin
			{
				UEdGraphPin* DestPin = CallFuncNode->FindPin(SrcPin->PinName);
				if(DestPin != NULL)
				{
					CompilerContext.CheckConnectionResponse(Schema->MovePinLinks(*SrcPin, *DestPin), this); // Source node is assumed to be owner...
				}
			}
		}

		// Finally, break any remaining links on the 'call func on member' node
		BreakAllNodeLinks();
	}
}

#undef LOCTEXT_NAMESPACE

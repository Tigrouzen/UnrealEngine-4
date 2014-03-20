// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#include "BlueprintGraphPrivatePCH.h"

#include "EngineLevelScriptClasses.h"
#include "Kismet2NameValidators.h"
#include "K2Node_BaseMCDelegate.h"

#define LOCTEXT_NAMESPACE "K2Node_CustomEvent"

#define SNAP_GRID (16) // @todo ensure this is the same as SNodePanel::GetSnapGridSize()

UK2Node_CustomEvent::UK2Node_CustomEvent(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	bOverrideFunction = false;
	bIsEditable = true;
	bCanRenameNode = true;
}

FString UK2Node_CustomEvent::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (TitleType == ENodeTitleType::EditableTitle)
	{
		return CustomFunctionName.ToString();
	}
	else if(TitleType == ENodeTitleType::ListView)
	{
		return NSLOCTEXT("K2Node", "CustomEvent_Title", "Custom Event").ToString();
	}
	else
	{
		FString RPCString = UK2Node_Event::GetLocalizedNetString(FunctionFlags, false);
		return FString::Printf(*NSLOCTEXT("K2Node", "CustomEvent_Name", "%s\nCustom Event").ToString(), *CustomFunctionName.ToString()) + RPCString;
	}
}

UEdGraphPin* UK2Node_CustomEvent::CreatePinFromUserDefinition(const TSharedPtr<FUserPinInfo> NewPinInfo)
{
	UEdGraphPin* NewPin = CreatePin(
		EGPD_Output, 
		NewPinInfo->PinType.PinCategory, 
		NewPinInfo->PinType.PinSubCategory, 
		NewPinInfo->PinType.PinSubCategoryObject.Get(), 
		NewPinInfo->PinType.bIsArray, 
		NewPinInfo->PinType.bIsReference, 
		NewPinInfo->PinName);
	NewPin->DefaultValue = NewPin->AutogeneratedDefaultValue = NewPinInfo->PinDefaultValue;
	return NewPin;
}

void UK2Node_CustomEvent::RenameCustomEventCloseToName(int32 StartIndex)
{
	bool bFoundName = false;
	const FString& BaseName = CustomFunctionName.ToString();

	for (int32 NameIndex = StartIndex; !bFoundName; ++NameIndex)
	{
		const FString NewName = FString::Printf(TEXT("%s_%d"), *BaseName, NameIndex);
		if (Rename(*NewName, GetOuter(), REN_Test))
		{
			CustomFunctionName = FName(NewName.GetCharArray().GetData());
			Rename(*NewName);
			bFoundName = true;
		}
	}
}

void UK2Node_CustomEvent::OnRenameNode(const FString& NewName)
{
	CustomFunctionName = *NewName;
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprint());
}

TSharedPtr<class INameValidatorInterface> UK2Node_CustomEvent::MakeNameValidator() const
{
	return MakeShareable(new FKismetNameValidator(GetBlueprint(), CustomFunctionName));
}

void UK2Node_CustomEvent::ReconstructNode()
{
	const UEdGraphPin* DelegateOutPin = FindPin(DelegateOutputName);

	const UK2Node_BaseMCDelegate* OtherNode = (DelegateOutPin && DelegateOutPin->LinkedTo.Num() && DelegateOutPin->LinkedTo[0]) ?
		Cast<const UK2Node_BaseMCDelegate>(DelegateOutPin->LinkedTo[0]->GetOwningNode()) : NULL;
	const UFunction* DelegateSignature = OtherNode ? OtherNode->GetDelegateSignature() : NULL;
	const bool bUseDelegateSignature = (NULL == FindEventSignatureFunction()) && DelegateSignature;

	if (bUseDelegateSignature)
	{
		UserDefinedPins.Empty();
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
		for (TFieldIterator<UProperty> PropIt(DelegateSignature); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
		{
			const UProperty* Param = *PropIt;
			if (!Param->HasAnyPropertyFlags(CPF_OutParm) || Param->HasAnyPropertyFlags(CPF_ReferenceParm))
			{
				FEdGraphPinType PinType;
				K2Schema->ConvertPropertyToPinType(Param, /*out*/ PinType);

				FString NewPinName = Param->GetName();
				int32 Index = 1;
				while ((DelegateOutputName == NewPinName) || (K2Schema->PN_Then == NewPinName))
				{
					++Index;
					NewPinName += FString::FromInt(Index);
				}
				TSharedPtr<FUserPinInfo> NewPinInfo = MakeShareable( new FUserPinInfo() );
				NewPinInfo->PinName = NewPinName;
				NewPinInfo->PinType = PinType;
				UserDefinedPins.Add(NewPinInfo);
			}
		}
	}

	Super::ReconstructNode();
}

UK2Node_CustomEvent* UK2Node_CustomEvent::CreateFromFunction(FVector2D GraphPosition, UEdGraph* ParentGraph, const FString& Name, const UFunction* Function, bool bSelectNewNode/* = true*/)
{
	UK2Node_CustomEvent* CustomEventNode = NULL;
	if(ParentGraph && Function)
	{
		CustomEventNode = NewObject<UK2Node_CustomEvent>(ParentGraph);
		CustomEventNode->CustomFunctionName = FName(*Name);
		CustomEventNode->SetFlags(RF_Transactional);
		ParentGraph->AddNode(CustomEventNode, true, bSelectNewNode);
		CustomEventNode->CreateNewGuid();
		CustomEventNode->PostPlacedNewNode();
		CustomEventNode->AllocateDefaultPins();

		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
		for (TFieldIterator<UProperty> PropIt(Function); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
		{
			const UProperty* Param = *PropIt;
			if (!Param->HasAnyPropertyFlags(CPF_OutParm) || Param->HasAnyPropertyFlags(CPF_ReferenceParm))
			{
				FEdGraphPinType PinType;
				K2Schema->ConvertPropertyToPinType(Param, /*out*/ PinType);
				CustomEventNode->CreateUserDefinedPin(Param->GetName(), PinType);
			}
		}

		CustomEventNode->NodePosX = GraphPosition.X;
		CustomEventNode->NodePosY = GraphPosition.Y;
		CustomEventNode->SnapToGrid(SNAP_GRID);
	}

	return CustomEventNode;
}

bool UK2Node_CustomEvent::IsEditable() const
{
	const UEdGraphPin* DelegateOutPin = FindPin(DelegateOutputName);
	if(DelegateOutPin && DelegateOutPin->LinkedTo.Num())
	{
		return false;
	}
	return Super::IsEditable();
}

bool UK2Node_CustomEvent::IsUsedByAuthorityOnlyDelegate() const
{
	if(const UEdGraphPin* DelegateOutPin = FindPin(DelegateOutputName))
	{
		for(auto PinIter = DelegateOutPin->LinkedTo.CreateConstIterator(); PinIter; ++PinIter)
		{
			const UEdGraphPin* LinkedPin = *PinIter;
			const UK2Node_BaseMCDelegate* Node = LinkedPin ? Cast<const UK2Node_BaseMCDelegate>(LinkedPin->GetOwningNode()) : NULL;
			if(Node && Node->IsAuthorityOnly())
			{
				return true;
			}
		}
	}

	return false;
}

FString UK2Node_CustomEvent::GetTooltip() const
{
	return LOCTEXT("AddCustomEvent_Tooltip", "An event with customizable name and parameters.").ToString();
}

FString UK2Node_CustomEvent::GetDocumentationLink() const
{
	// Use the main k2 node doc
	return UK2Node::GetDocumentationLink();
}

FString UK2Node_CustomEvent::GetDocumentationExcerptName() const
{
	return TEXT("UK2Node_CustomEvent");
}

#undef LOCTEXT_NAMESPACE

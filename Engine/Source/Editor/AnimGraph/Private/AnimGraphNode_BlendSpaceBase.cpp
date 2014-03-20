// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphPrivatePCH.h"

#include "GraphEditorActions.h"

/////////////////////////////////////////////////////

// Action to add a sequence player node to the graph
struct FNewBlendSpacePlayerAction : public FEdGraphSchemaAction_K2NewNode
{
	FNewBlendSpacePlayerAction(class UBlendSpaceBase* BlendSpace)
	{
		check(BlendSpace);

		const bool bIsAimOffset = BlendSpace->IsA(UAimOffsetBlendSpace::StaticClass()) || BlendSpace->IsA(UAimOffsetBlendSpace1D::StaticClass());
		if (bIsAimOffset)
		{
			UAnimGraphNode_RotationOffsetBlendSpace* Template = NewObject<UAnimGraphNode_RotationOffsetBlendSpace>();
			Template->Node.BlendSpace = BlendSpace;
			NodeTemplate = Template;
			TooltipDescription = TEXT("Evaluates an aim offset at a particular coordinate to produce a pose");
		}
		else
		{
			UAnimGraphNode_BlendSpacePlayer* Template = NewObject<UAnimGraphNode_BlendSpacePlayer>();
			Template->Node.BlendSpace = BlendSpace;
			NodeTemplate = Template;
			TooltipDescription = TEXT("Evaluates a blend space at a particular coordinate to produce a pose");
		}

		MenuDescription = NodeTemplate->GetNodeTitle(ENodeTitleType::ListView);

		Category = TEXT("Animations");

		// Grab extra keywords
		Keywords = BlendSpace->GetPathName();
	}
};

/////////////////////////////////////////////////////
// UAnimGraphNode_BlendSpaceBase

UAnimGraphNode_BlendSpaceBase::UAnimGraphNode_BlendSpaceBase(const FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
}

FLinearColor UAnimGraphNode_BlendSpaceBase::GetNodeTitleColor() const
{
	return FLinearColor(0.2f, 0.8f, 0.2f);
}

void UAnimGraphNode_BlendSpaceBase::GetBlendSpaceEntries(bool bWantsAimOffset, FGraphContextMenuBuilder& ContextMenuBuilder)
{
	if ((ContextMenuBuilder.FromPin == NULL) || (UAnimationGraphSchema::IsPosePin(ContextMenuBuilder.FromPin->PinType) && (ContextMenuBuilder.FromPin->Direction == EGPD_Input)))
	{
		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraphChecked(ContextMenuBuilder.CurrentGraph);

		// Add an entry for each loaded animation sequence
		if (UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(Blueprint))
		{
			for (TObjectIterator<UBlendSpaceBase> BlendSpaceIt; BlendSpaceIt; ++BlendSpaceIt)
			{
				UBlendSpaceBase* BlendSpace = *BlendSpaceIt;
				
				const bool bIsAimOffset = BlendSpace->IsA(UAimOffsetBlendSpace::StaticClass()) || BlendSpace->IsA(UAimOffsetBlendSpace1D::StaticClass());
				const bool bPassesAimOffsetFilter = !(bIsAimOffset ^ bWantsAimOffset);
				const bool bPassesSkeletonFilter = (BlendSpace->GetSkeleton() == AnimBlueprint->TargetSkeleton);

				if (bPassesAimOffsetFilter && bPassesSkeletonFilter)
				{
					TSharedPtr<FNewBlendSpacePlayerAction> NewAction(new FNewBlendSpacePlayerAction(BlendSpace));
					ContextMenuBuilder.AddAction( NewAction );
				}
			}
		}
	}
}

void UAnimGraphNode_BlendSpaceBase::CustomizePinData(UEdGraphPin* Pin, FName SourcePropertyName, int32 ArrayIndex) const
{
	UBlendSpaceBase * BlendSpace = GetBlendSpace();

	if (BlendSpace != NULL)
	{
		if (SourcePropertyName == TEXT("X"))
		{
			Pin->PinFriendlyName = BlendSpace->GetBlendParameter(0).DisplayName;
		}
		else if (SourcePropertyName == TEXT("Y"))
		{
			Pin->PinFriendlyName = BlendSpace->GetBlendParameter(1).DisplayName;
			Pin->bHidden = (BlendSpace->NumOfDimension == 1) ? 1 : 0;
		}
		else if (SourcePropertyName == TEXT("Z"))
		{
			Pin->PinFriendlyName = BlendSpace->GetBlendParameter(2).DisplayName;
		}
	}
}


void UAnimGraphNode_BlendSpaceBase::PreloadRequiredAssets()
{
	PreloadObject(GetBlendSpace());

	Super::PreloadRequiredAssets();
}
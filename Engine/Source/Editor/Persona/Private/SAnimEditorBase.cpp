// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#include "PersonaPrivatePCH.h"

#include "SAnimEditorBase.h"
#include "GraphEditor.h"
#include "GraphEditorModule.h"
#include "Editor/Kismet/Public/SKismetInspector.h"
#include "SAnimationScrubPanel.h"
#include "SAnimNotifyPanel.h"
#include "SAnimCurvePanel.h"
#include "AnimPreviewInstance.h"

#define LOCTEXT_NAMESPACE "AnimEditorBase"

//////////////////////////////////////////////////////////////////////////
// SAnimEditorBase

void SAnimEditorBase::Construct(const FArguments& InArgs)
{
	PersonaPtr = InArgs._Persona;

	SetInputViewRange(0, GetSequenceLength());

	this->ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			// Header, shows name of timeline we are editing
			SNew(SBorder)
			. BorderImage( FEditorStyle::GetBrush( TEXT("Graph.TitleBackground") ) )
			. HAlign(HAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				. VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font( FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 14 ) )
					.ColorAndOpacity( FLinearColor(1,1,1,0.5) )
					.Text( this, &SAnimEditorBase::GetEditorObjectName )
				]
			]
		]

		+SVerticalBox::Slot()
		.FillHeight(1)
		[
			SNew(SBorder)
			.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
			[
				SNew( SScrollBox )
				+SScrollBox::Slot() 
				[
					SAssignNew (EditorPanels, SVerticalBox)
				]
			]
		]

		+SVerticalBox::Slot()
		.AutoHeight()
		.VAlign(VAlign_Center)
		[
			// this is *temporary* information to display stuff
			SNew( SBorder )
			.Padding(FMargin(4))
			[
				SNew( SHorizontalBox )
				+SHorizontalBox::Slot()
				.FillWidth(1)
				[
					SNew( SHorizontalBox )
					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(FMargin(4,4,0,0))
					[
						SNew( STextBlock )
						.Text(LOCTEXT("Animation", "Animation : ").ToString())
					]

					+SHorizontalBox::Slot()
					.FillWidth(1)
					.Padding(FMargin(4,4,0,0))
					[
						SNew( STextBlock )
						.Text(this, &SAnimEditorBase::GetEditorObjectName)
					]
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew( SHorizontalBox )
					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(FMargin(4,4,0,0))
					[
						SNew( STextBlock )
						.Text(LOCTEXT("Percentage", "Percentage: ").ToString())
					]
					+SHorizontalBox::Slot()
					.FillWidth(1)
					.Padding(FMargin(4,4,0,0))
					[
						SNew( STextBlock )
						.Text(this, &SAnimEditorBase::GetCurrentPercentage)
					]
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew( SHorizontalBox )
					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(FMargin(4,4,0,0))
					[
						SNew( STextBlock )
						.Text(LOCTEXT("CurrentTime", "CurrentTime: ").ToString())
					]
					+SHorizontalBox::Slot()
					.FillWidth(1)
					.Padding(FMargin(4,4,0,0))
					[
						SNew( STextBlock )
						.Text(this, &SAnimEditorBase::GetCurrentSequenceTime)
					]
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew( SHorizontalBox )
					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(FMargin(4,4,0,0))
					[
						SNew( STextBlock )
						.Text(LOCTEXT("CurrentFrame", "Current Frame: ").ToString())
					]
					+SHorizontalBox::Slot()
					.FillWidth(1)
					.Padding(FMargin(4,4,0,0))
					[
						SNew( STextBlock )
						.Text(this, &SAnimEditorBase::GetCurrentFrame)
					]
				]
			]
		]
	
		+SVerticalBox::Slot()
		.AutoHeight() 
		.VAlign(VAlign_Bottom)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot() 
			.FillWidth(1)
			[
				ConstructAnimScrubPanel()
			]
		]
	];
}

TSharedRef<class SAnimationScrubPanel> SAnimEditorBase::ConstructAnimScrubPanel()
{
	return SAssignNew( AnimScrubPanel, SAnimationScrubPanel )
		.Persona(PersonaPtr)
		.LockedSequence(GetEditorObject())
		.ViewInputMin(this, &SAnimEditorBase::GetViewMinInput)
		.ViewInputMax(this, &SAnimEditorBase::GetViewMaxInput)
		.OnSetInputViewRange(this, &SAnimEditorBase::SetInputViewRange)
		.bAllowZoom(true);
}

void SAnimEditorBase::AddReferencedObjects( FReferenceCollector& Collector )
{
	EditorObjectTracker.AddReferencedObjects(Collector);
}

UObject* SAnimEditorBase::ShowInDetailsView( UClass* EdClass )
{
	check(GetEditorObject()!=NULL);

	UObject *Obj = EditorObjectTracker.GetEditorObjectForClass(EdClass);

	if(Obj != NULL)
	{
		if(Obj->IsA(UEditorAnimBaseObj::StaticClass()))
		{
			UEditorAnimBaseObj *EdObj = Cast<UEditorAnimBaseObj>(Obj);
			InitDetailsViewEditorObject(EdObj);
			PersonaPtr.Pin()->SetDetailObject(EdObj);
		}
	}
	return Obj;
}

void SAnimEditorBase::ClearDetailsView()
{
	PersonaPtr.Pin()->SetDetailObject(NULL);
}

FString SAnimEditorBase::GetEditorObjectName() const
{
	if (GetEditorObject() != NULL)
	{
		return GetEditorObject()->GetName();
	}
	else
	{
		return LOCTEXT("NoEditorObject", "No Editor Object").ToString();
	}
}

void SAnimEditorBase::RecalculateSequenceLength()
{
	// Remove Gaps and update Montage Sequence Length
	if(UAnimCompositeBase* Composite = Cast<UAnimCompositeBase>(GetEditorObject()))
	{
		float NewSequenceLength = CalculateSequenceLengthOfEditorObject();
		if (NewSequenceLength != GetSequenceLength())
		{
			ClampToEndTime(NewSequenceLength);

			Composite->SetSequenceLength(NewSequenceLength);

			// Reset view if we changed length (note: has to be done after ->SetSequenceLength)!
			SetInputViewRange(0.f, NewSequenceLength);

			UAnimSingleNodeInstance * PreviewInstance = GetPreviewInstance();
			if (PreviewInstance)
			{
				// Re-set the position, so instance is clamped properly
				PreviewInstance->SetPosition(PreviewInstance->CurrentTime, false); 
			}
		}
	}

	if(UAnimSequenceBase* Sequence = Cast<UAnimSequenceBase>(GetEditorObject()))
	{
		Sequence->ClampNotifiesAtEndOfSequence();
	}
}

bool SAnimEditorBase::ClampToEndTime(float NewEndTime)
{
	float SequenceLength = GetSequenceLength();

	//if we had a valid sequence length before and our new end time is shorter
	//then we need to clamp.
	return (SequenceLength > 0.f && NewEndTime < SequenceLength);
}

void SAnimEditorBase::OnSelectionChanged(const FGraphPanelSelectionSet& SelectedItems)
{
	if (SelectedItems.Num() == 0)
	{
		// Edit the sequence
		PersonaPtr.Pin()->UpdateSelectionDetails(GetEditorObject(), LOCTEXT("Edit Sequence", "Edit Sequence").ToString());
	}
	else
	{
		// Edit selected notifications
		PersonaPtr.Pin()->FocusInspectorOnGraphSelection(SelectedItems);
	}
}

class UAnimSingleNodeInstance* SAnimEditorBase::GetPreviewInstance() const
{
	return (PersonaPtr.Pin()->GetPreviewMeshComponent())? PersonaPtr.Pin()->GetPreviewMeshComponent()->PreviewInstance:NULL;
}

float SAnimEditorBase::GetScrubValue() const
{
	UAnimSingleNodeInstance * PreviewInstance = GetPreviewInstance();
	if (PreviewInstance)
	{
		float CurTime = PreviewInstance->CurrentTime;
		return (CurTime); 
	}
	else
	{
		return 0.f;
	}
}

void SAnimEditorBase::SetInputViewRange(float InViewMinInput, float InViewMaxInput)
{
	ViewMaxInput = FMath::Min<float>(InViewMaxInput, GetEditorObject()->SequenceLength);
	ViewMinInput = FMath::Max<float>(InViewMinInput, 0.f);
}

FString SAnimEditorBase::GetCurrentSequenceTime() const
{
	UAnimSingleNodeInstance * PreviewInstance = GetPreviewInstance();
	float CurTime = 0.f;
	float TotalTime = GetEditorObject()->SequenceLength;

	if (PreviewInstance)
	{
		CurTime = PreviewInstance->CurrentTime;
	}

	const FString Fraction = FString::Printf(TEXT("%0.3f / %0.3f"), CurTime, TotalTime);
	return FText::Format( LOCTEXT("FractionSeconds", "{0} (second(s))"), FText::FromString(Fraction) ).ToString();
}

FString SAnimEditorBase::GetCurrentPercentage() const
{
	UAnimSingleNodeInstance * PreviewInstance = GetPreviewInstance();
	float Percentage = 0.f;
	if (PreviewInstance)
	{
		Percentage = PreviewInstance->CurrentTime / GetEditorObject()->SequenceLength;
	}

	return FString::Printf(TEXT("%0.2f %%"), Percentage*100.f);
}

FString SAnimEditorBase::GetCurrentFrame() const
{
	UAnimSingleNodeInstance * PreviewInstance = GetPreviewInstance();
	float Percentage = 0.f;
	float NumFrames = GetEditorObject()->GetNumberOfFrames();

	if (PreviewInstance)
	{
		Percentage = PreviewInstance->CurrentTime/GetEditorObject()->SequenceLength;
	}

	const FString Fraction = FString::Printf(TEXT("%0.2f / %d"), NumFrames*Percentage, (int32)NumFrames);
	return FText::Format( LOCTEXT("FractionKeys", "{0} (key(s))"), FText::FromString(Fraction) ).ToString();
}

#undef LOCTEXT_NAMESPACE

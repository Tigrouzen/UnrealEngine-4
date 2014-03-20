// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#include "PersonaPrivatePCH.h"

#include "SAnimCompositeEditor.h"
#include "GraphEditor.h"
#include "GraphEditorModule.h"
#include "Editor/Kismet/Public/SKismetInspector.h"
#include "SAnimCompositePanel.h"
#include "ScopedTransaction.h"
#include "SAnimNotifyPanel.h"
#include "SAnimCurvePanel.h"

//////////////////////////////////////////////////////////////////////////
// SAnimCompositeEditor

void SAnimCompositeEditor::Construct(const FArguments& InArgs)
{
	bRebuildPanel = false;
	PersonaPtr = InArgs._Persona;
	CompositeObj = InArgs._Composite;
	check(CompositeObj);

	SAnimEditorBase::Construct( SAnimEditorBase::FArguments()
		.Persona(InArgs._Persona)
		);

	PersonaPtr.Pin()->RegisterOnPostUndo(FPersona::FOnPostUndo::CreateSP( this, &SAnimCompositeEditor::PostUndo ) );	

	EditorPanels->AddSlot()
		.AutoHeight()
		.Padding(0, 10)
		[
			SAssignNew( AnimCompositePanel, SAnimCompositePanel )
			.Persona(PersonaPtr)
			.Composite(CompositeObj)
			.CompositeEditor(SharedThis(this))
			.WidgetWidth(S2ColumnWidget::DEFAULT_RIGHT_COLUMN_WIDTH)
			.ViewInputMin(this, &SAnimEditorBase::GetViewMinInput)
			.ViewInputMax(this, &SAnimEditorBase::GetViewMaxInput)
			.OnSetInputViewRange(this, &SAnimEditorBase::SetInputViewRange)
		];

	EditorPanels->AddSlot()
		.AutoHeight()
		.Padding(0, 10)
		[
			SAssignNew( AnimNotifyPanel, SAnimNotifyPanel )
			.Persona(InArgs._Persona)
			.Sequence(CompositeObj)
			.WidgetWidth(S2ColumnWidget::DEFAULT_RIGHT_COLUMN_WIDTH)
			.InputMin(this, &SAnimEditorBase::GetMinInput)
			.InputMax(this, &SAnimEditorBase::GetMaxInput)
			.ViewInputMin(this, &SAnimEditorBase::GetViewMinInput)
			.ViewInputMax(this, &SAnimEditorBase::GetViewMaxInput)
			.OnSetInputViewRange(this, &SAnimEditorBase::SetInputViewRange)
			.OnGetScrubValue(this, &SAnimEditorBase::GetScrubValue)
			.OnSelectionChanged(this, &SAnimEditorBase::OnSelectionChanged)
		];

	EditorPanels->AddSlot()
		.AutoHeight()
		.Padding(0, 10)
		[
			SAssignNew( AnimCurvePanel, SAnimCurvePanel )
			.Sequence(CompositeObj)
			.WidgetWidth(S2ColumnWidget::DEFAULT_RIGHT_COLUMN_WIDTH)
			.ViewInputMin(this, &SAnimEditorBase::GetViewMinInput)
			.ViewInputMax(this, &SAnimEditorBase::GetViewMaxInput)
			.InputMin(this, &SAnimEditorBase::GetMinInput)
			.InputMax(this, &SAnimEditorBase::GetMaxInput)
			.OnSetInputViewRange(this, &SAnimEditorBase::SetInputViewRange)
			.OnGetScrubValue(this, &SAnimEditorBase::GetScrubValue)
		];

	CollapseComposite();
}

SAnimCompositeEditor::~SAnimCompositeEditor()
{
	if (PersonaPtr.IsValid())
	{
		PersonaPtr.Pin()->UnregisterOnPostUndo(this);
	}
}

void SAnimCompositeEditor::PreAnimUpdate()
{
	CompositeObj->Modify();
}

void SAnimCompositeEditor::PostAnimUpdate()
{
	CompositeObj->MarkPackageDirty();
	SortAndUpdateComposite();
}

void SAnimCompositeEditor::RebuildPanel()
{
	SortAndUpdateComposite();
	AnimCompositePanel->Update();
	bRebuildPanel = false;
}

void SAnimCompositeEditor::OnCompositeChange(class UObject *EditorAnimBaseObj, bool Rebuild)
{
	if(CompositeObj!=NULL)
	{
		if(Rebuild)
		{
			bRebuildPanel = true;
		} 
		else
		{
			CollapseComposite();
		}

		CompositeObj->MarkPackageDirty();
	}
}

void SAnimCompositeEditor::CollapseComposite()
{
	if (CompositeObj==NULL)
	{
		return;
	}

	CompositeObj->AnimationTrack.CollapseAnimSegments();

	RecalculateSequenceLength();
}

void SAnimCompositeEditor::PostUndo()
{
	bRebuildPanel=true;

	// when undo or redo happens, we still have to recalculate length, so we can't rely on sequence length changes or not
	if (CompositeObj->SequenceLength)
	{
		CompositeObj->SequenceLength = 0.f;
	}
}

void SAnimCompositeEditor::InitDetailsViewEditorObject(UEditorAnimBaseObj* EdObj)
{
	EdObj->InitFromAnim(CompositeObj, FOnAnimObjectChange::CreateSP( SharedThis(this), &SAnimCompositeEditor::OnCompositeChange ));
}

void SAnimCompositeEditor::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	// we should not update any property related within PostEditChange, 
	// so this is deferred to Tick, when it needs to rebuild, just mark it and this will update in the next tick
	if (bRebuildPanel)
	{
		RebuildPanel();
	}

	SWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
}

float SAnimCompositeEditor::CalculateSequenceLengthOfEditorObject() const
{
	return CompositeObj->AnimationTrack.GetLength();
}

void SAnimCompositeEditor::SortAndUpdateComposite()
{
	if (CompositeObj == NULL)
	{
		return;
	}

	CompositeObj->AnimationTrack.SortAnimSegments();

	RecalculateSequenceLength();

	// Update view (this will recreate everything)
	AnimCompositePanel->Update();
}


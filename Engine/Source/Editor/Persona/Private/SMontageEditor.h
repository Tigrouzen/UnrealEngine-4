// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "Persona.h"
#include "GraphEditor.h"
#include "SNodePanel.h"
#include "SAnimEditorBase.h"

//////////////////////////////////////////////////////////////////////////
// SMontageEditor

/** Overall animation montage editing widget. This mostly contains functions for editing the UAnimMontage.

	SMontageEditor will create the SAnimMontagePanel which is mostly responsible for setting up the UI 
	portion of the Montage tool and registering callbacks to the SMontageEditor to do the actual editing.
	
*/
class SMontageEditor : public SAnimEditorBase 
{
public:
	SLATE_BEGIN_ARGS( SMontageEditor )
		: _Persona()
		, _Montage(NULL)
		{}

		SLATE_ARGUMENT( TSharedPtr<FPersona>, Persona )
		SLATE_ARGUMENT( UAnimMontage*, Montage )
	SLATE_END_ARGS()

	~SMontageEditor();

private:
	/** Persona reference **/
	TSharedPtr<class SAnimMontagePanel> AnimMontagePanel;
	TSharedPtr<class SAnimNotifyPanel> AnimNotifyPanel;
	TSharedPtr<class SAnimCurvePanel>	AnimCurvePanel;
	TSharedPtr<class SAnimMontageSectionsPanel> AnimMontageSectionsPanel;
	TSharedPtr<class SAnimMontageScrubPanel> AnimMontageScrubPanel;

	/** do I have to update montage **/
	bool bRebuildMontagePanel;
	void RebuildMontagePanel();

protected:
	// Begin SAnimEditorBase interface
	virtual TSharedRef<class SAnimationScrubPanel> ConstructAnimScrubPanel() OVERRIDE;
	// End SAnimEditorBase interface

public:
	void Construct(const FArguments& InArgs);
	void SetMontageObj(UAnimMontage * NewMontage);
	UAnimMontage * GetMontageObj() const { return MontageObj; }

	virtual UAnimSequenceBase* GetEditorObject() const OVERRIDE { return GetMontageObj(); }

	void RestartPreview();
	void RestartPreviewFromSection(int32 FromSectionIdx = INDEX_NONE);
	void RestartPreviewPlayAllSections();

private:
	/** Pointer to the animation sequence being edited */
	UAnimMontage* MontageObj;

	/** If currently previewing all or selected section */
	bool bPreviewingAllSections;

	/** If currently previewing tracks instead of sections */
	bool bPreviewingTracks;

	/** If previewing section, it is section used to restart previewing when play button is pushed */
	int32 PreviewingStartSectionIdx;

	/** If user is currently dragging an item */
	bool bDragging;
	
	virtual float CalculateSequenceLengthOfEditorObject() const OVERRIDE;
	void SortAndUpdateMontage();
	void CollapseMontage();
	void SortBranchPoints();
	void SortAnimSegments();
	void SortSections();
	void EnsureStartingSection();
	void EnsureSlotNode();
	virtual bool ClampToEndTime(float NewEndTime) OVERRIDE;
	void PostUndo();
	
	bool GetSectionTime( int32 SectionIndex, float &OutTime ) const;

	bool ValidIndexes(int32 AnimSlotIndex, int32 AnimSegmentIndex) const;
	bool ValidSection(int32 SectionIndex) const;
	bool ValidBranch(int32 BranchIndex) const;	

	/** Updates Notify trigger offsets to take into account current montage state */
	void RefreshNotifyTriggerOffsets();

protected:
	virtual void InitDetailsViewEditorObject(class UEditorAnimBaseObj* EdObj) OVERRIDE;

public:

	/** These are meant to be callbacks used by the montage editing UI widgets */

	void					OnMontageChange(class UObject *EditorAnimBaseObj, bool Rebuild);
	void					ShowSectionInDetailsView(int32 SectionIdx);

	TArray<float>			GetSectionStartTimes() const;
	TArray<FTrackMarkerBar>	GetMarkerBarInformation() const;
	TArray<FString>			GetSectionNames() const;
	TArray<float>			GetAnimSegmentStartTimes() const;
	void					OnEditSectionTime( int32 SectionIndex, float NewTime);
	void					OnEditSectionTimeFinish( int32 SectionIndex);

	void					AddNewSection(float StartTime, FString SectionName);
	void					RemoveSection(int32 SectionIndex);

	FString					GetSectionName(int32 SectionIndex) const;

	float					GetBranchPointStartPos(int32 BranchPointIndex) const;
	FString					GetBranchPointName(int32 BranchPointIndex) const;
	void					SetBranchPointStartPos(float NewStartPos, int32 BranchPointIndex);
	void					RemoveBranchPoint(int32 BranchPointIndex);
	void					AddBranchPoint(float StartTime, FString EventName);
	void					RenameBranchPoint(int32 BranchIndex, FString NewEventName);
	void					RenameSlotNode(int32 SlotIndex, FString NewSlotName);

	void					AddNewMontageSlot(FString NewSlotName);
	void					RemoveMontageSlot(int32 AnimSlotIndex);
	FText					GetMontageSlotName(int32 SlotIndex) const;

	void					MakeDefaultSequentialSections();
	void					ClearSquenceOrdering();

	/** Delegete handlers for when the editor UI is changing the montage */
	void			PreAnimUpdate();
	void			PostAnimUpdate();

	// SWidget interface start
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) OVERRIDE;
	// SWidget interface end
};

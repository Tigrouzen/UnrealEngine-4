// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

class SGraphNodeSequencePlayer : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SGraphNodeSequencePlayer){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UK2Node* InNode);

	// SNodePanel::SNode interface
	virtual void GetNodeInfoPopups(FNodeInfoContext* Context, TArray<FGraphInformationPopupInfo>& Popups) const OVERRIDE;
	// End of SNodePanel::SNode interface

	// SGraphNode interface
	virtual void UpdateGraphNode() OVERRIDE;
	virtual void CreateBelowWidgetControls(TSharedPtr<SVerticalBox> MainBox) OVERRIDE;
	// End of SGraphNode interface

protected:
	/** Get the sequence player associated with this graph node */
	struct FAnimNode_SequencePlayer* GetSequencePlayer() const;
	EVisibility GetSliderVisibility() const;
	float GetSequencePositionRatio() const;
	void SetSequencePositionRatio(float NewRatio);

	FString GetPositionTooltip() const;

	bool GetSequencePositionInfo(float& Out_Position, float& Out_Length, int32& FrameCount) const;
};

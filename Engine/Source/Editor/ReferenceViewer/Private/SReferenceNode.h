// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * 
 */
class SReferenceNode : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS( SReferenceNode ){}

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct( const FArguments& InArgs, UEdGraphNode_Reference* InNode );

	// SGraphNode implementation
	virtual void UpdateGraphNode() OVERRIDE;
	// End SGraphNode implementation

private:
	TSharedPtr<class FAssetThumbnail> AssetThumbnail;
	bool bUsesThumbnail;
};

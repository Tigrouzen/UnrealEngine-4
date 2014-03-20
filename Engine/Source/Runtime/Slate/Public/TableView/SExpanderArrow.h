// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

/** Expander arrow and indentation component that can be placed in a TableRow of a TreeView. Intended for use by TMultiColumnRow in TreeViews. */
class SLATE_API SExpanderArrow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SExpanderArrow ){}
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, const TSharedPtr<class ITableRow>& TableRow );

protected:

	/** Invoked when the expanded button is clicked (toggle item expansion) */
	FReply OnArrowClicked();

	/** @return Visible when has children; invisible otherwise */
	EVisibility GetExpanderVisibility() const;

	/** @return the margin corresponding to how far this item is indented */
	FMargin GetExpanderPadding() const;

	/** @return the name of an image that should be shown as the expander arrow */
	const FSlateBrush* GetExpanderImage() const;

	TWeakPtr<class ITableRow> OwnerRowPtr;

	/** A reference to the expander button */
	TSharedPtr<SButton> ExpanderArrow;

};

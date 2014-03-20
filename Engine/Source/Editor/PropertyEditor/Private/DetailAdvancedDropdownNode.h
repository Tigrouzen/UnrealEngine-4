// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once


class FAdvancedDropdownNode : public IDetailTreeNode, public TSharedFromThis<FAdvancedDropdownNode>
{
public:
	FAdvancedDropdownNode( FDetailCategoryImpl& InParentCategory, const TAttribute<bool>& InExpanded, const TAttribute<bool>& InEnabled, bool bInShouldShowAdvancedButton, bool bInDisplayShowAdvancedMessage, bool bInShowSplitter )
		: ParentCategory( InParentCategory )
		, IsEnabled( InEnabled )
		, IsExpanded( InExpanded )
		, bShouldShowAdvancedButton( bInShouldShowAdvancedButton )
		, bIsTopNode( false )
		, bDisplayShowAdvancedMessage( bInDisplayShowAdvancedMessage )
		, bShowSplitter( bInShowSplitter )
	{}

	FAdvancedDropdownNode( FDetailCategoryImpl& InParentCategory, bool bInIsTopNode )
		: ParentCategory( InParentCategory )
		, bShouldShowAdvancedButton( false )
		, bIsTopNode( bInIsTopNode )
		, bDisplayShowAdvancedMessage( false  )
		, bShowSplitter( false )
	{}
private:
	/** IDetailTreeNode Interface */
	virtual SDetailsView& GetDetailsView() const OVERRIDE { return ParentCategory.GetDetailsView(); }
	virtual TSharedRef< ITableRow > GenerateNodeWidget( const TSharedRef<STableViewBase>& OwnerTable, const FDetailColumnSizeData& ColumnSizeData, const TSharedRef<IPropertyUtilities>& PropertyUtilities ) OVERRIDE;
	virtual void GetChildren( TArray< TSharedRef<IDetailTreeNode> >& OutChildren )  OVERRIDE {}
	virtual void OnItemExpansionChanged( bool bIsExpanded ) OVERRIDE {}
	virtual bool ShouldBeExpanded() const OVERRIDE { return false; }
	virtual ENodeVisibility::Type GetVisibility() const OVERRIDE { return ENodeVisibility::Visible; }
	virtual void FilterNode( const FDetailFilter& InFilter ) OVERRIDE {}
	virtual void Tick( float DeltaTime ) OVERRIDE {}
	virtual bool ShouldShowOnlyChildren() const OVERRIDE { return false; }
	virtual FName GetNodeName() const OVERRIDE { return NAME_None; }

	/** Called when the advanced drop down arrow is clicked */
	FReply OnAdvancedDropDownClicked();
private:
	FDetailCategoryImpl& ParentCategory;
	TAttribute<bool> IsEnabled;
	TAttribute<bool> IsExpanded;
	bool bShouldShowAdvancedButton;
	bool bIsTopNode;
	bool bDisplayShowAdvancedMessage;
	bool bShowSplitter;
};

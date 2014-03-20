// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SInputBindingEditorPanel.h: Declares the SInputBindingEditorPanel class.
=============================================================================*/

#pragma once


/**
 * A gesture sort functor.  Sorts by name or gesture and ascending or descending
 */
struct FGestureSort
{
	FGestureSort( bool bInSortName, bool bInSortUp )
		: bSortName( bInSortName )
		, bSortUp( bInSortUp )
	{ }

	bool operator()( const TSharedPtr<FGestureTreeItem>& A, const TSharedPtr<FGestureTreeItem>& B ) const
	{
		if( bSortName )
		{
			bool bResult = A->CommandInfo->GetLabel().CompareTo( B->CommandInfo->GetLabel() ) == -1;
			return bSortUp ? !bResult : bResult;
		}
		else
		{
			// Sort by binding
			bool bResult = A->CommandInfo->GetInputText().CompareTo( B->CommandInfo->GetInputText() ) == -1;
			return bSortUp ? !bResult : bResult;
		}
	}


private:

	// Whether or not to sort by name.  If false we sort by binding.
	bool bSortName;

	// Whether or not to sort up.  If false we sort down.
	bool bSortUp;
};


/**
 * The main input binding editor widget                   
 */
class SInputBindingEditorPanel : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SInputBindingEditorPanel){}
	SLATE_END_ARGS()
	

public:

	/**
	 * Default constructor.
	 */
	SInputBindingEditorPanel()
		: GestureSortMode( true, false )
	{ }

	/**
	 * Destructor	Saves the user defined bindings to disk when closed
	 */
	virtual ~SInputBindingEditorPanel()
	{
		FInputBindingManager::Get().SaveInputBindings();
		FBindingContext::CommandsChanged.RemoveAll( this );
	}


public:

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs - The Slate argument list.
	 */
	void Construct(const FArguments& InArgs);

	// SWidget interface
	virtual bool SupportsKeyboardFocus() const OVERRIDE;
	virtual FReply OnKeyboardFocusReceived( const FGeometry& MyGeometry, const FKeyboardFocusEvent& InKeyboardFocusEvent ) OVERRIDE;
	// End of SWidget interface

private:

	/**
	 * Called when the text changes in the search box
	 * 
	 * @param NewSearch	The new search term to filter by
	 */
	void OnSearchChanged( const FText& NewSearch );

	/**
	 * Generates widget for an item in the gesture tree
	 */
	TSharedRef< ITableRow > OnGenerateWidgetForTreeItem( TSharedPtr<FGestureTreeItem> InTreeItem, const TSharedRef<STableViewBase>& OwnerTable );

	/**
	 * Gets children FGestureTreeItems from the passed in tree item.  Note: Only contexts have children and those children are the actual gestures
	 */
	void OnGetChildrenForTreeItem( TSharedPtr<FGestureTreeItem> InTreeItem, TArray< TSharedPtr< FGestureTreeItem > >& OutChildren );

	/**
	 * Called when the binding column is clicked.  We sort by binding in this case 
	 */	 
	FReply OnBindingColumnClicked();

	/**
	 * Called when the name column is clicked. We sort by name in this case
	 */	
	FReply OnNameColumnClicked();

	/**
	 * Updates the master context list with new commands
	 */
	void UpdateContextMasterList();

	/**
	 * Filters the currently visible context list
	 */
	void FilterVisibleContextList();

	/**
	 * Called when new commands are registered with the input binding manager
	 */
	void OnCommandsChanged();
private:

	// List of all known contexts.
	TArray< TSharedPtr<FGestureTreeItem> > ContextMasterList;
	
	// List of contexts visible in the tree.
	TArray< TSharedPtr<FGestureTreeItem> > ContextVisibleList;
	
	// Search box
	TSharedPtr<SWidget> SearchBox;

	// Gesture tree widget.
	TSharedPtr< SGestureTree > GestureTree;
	
	// The current gesture sort to use.
	FGestureSort GestureSortMode;
	
	// The current list of filter strings to filter gestures by.
	TArray<FString> FilterStrings;
};

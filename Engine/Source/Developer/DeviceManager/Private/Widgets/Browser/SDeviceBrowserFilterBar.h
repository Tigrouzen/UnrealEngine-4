// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SDeviceBrowserFilterBar.h: Declares the SDeviceBrowserFilterBar class.
=============================================================================*/

#pragma once


/**
 * Implements the device browser filter bar widget.
 */
class SDeviceBrowserFilterBar
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SDeviceBrowserFilterBar) { }
	SLATE_END_ARGS()

public:

	/**
	 * Default constructor.
	 */
	SDeviceBrowserFilterBar( )
		: Filter(NULL)
	{ }

	/**
	 * Destructor.
	 */
	~SDeviceBrowserFilterBar( );

public:

	/**
	 * Construct this widget
	 *
	 * @param InArgs - The declaration data for this widget.
	 * @param InFilter - The filter model to use.
	 */
	void Construct( const FArguments& InArgs, FDeviceBrowserFilterRef InFilter );

private:

	// Callback for filter model resets.
	void HandleFilterReset( );

	// Callback for changing the filter string text box text.
	void HandleFilterStringTextChanged( const FText& NewText );

	// Callback for changing the checked state of the given platform filter row.
	void HandlePlatformListRowCheckStateChanged( ESlateCheckBoxState::Type CheckState, TSharedPtr<FString> PlatformName );

	// Callback for getting the checked state of the given platform filter row.
	ESlateCheckBoxState::Type HandlePlatformListRowIsChecked( TSharedPtr<FString> PlatformName ) const;

	// Callback for getting the text for a row in the platform filter drop-down.
	FString HandlePlatformListRowText( TSharedPtr<FString> PlatformName ) const;

	// Generates a row widget for the platform filter list.
	TSharedRef<ITableRow> HandlePlatformListViewGenerateRow( TSharedPtr<FString> PlatformName, const TSharedRef<STableViewBase>& OwnerTable );

private:

	// Holds a pointer to the filter model.
	FDeviceBrowserFilterPtr Filter;

	// Holds the filter string text box.
	TSharedPtr<SSearchBox> FilterStringTextBox;

	// Holds the platform filters list view.
	TSharedPtr<SListView<TSharedPtr<FString> > > PlatformListView;
};

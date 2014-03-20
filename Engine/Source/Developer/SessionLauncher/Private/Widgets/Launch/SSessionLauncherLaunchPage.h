// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SSessionLauncherLaunchPage.h: Declares the SSessionLauncherLaunchPage class.
=============================================================================*/

#pragma once


/**
 * Implements the profile page for the session launcher wizard.
 */
class SSessionLauncherLaunchPage
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SSessionLauncherLaunchPage) { }
	SLATE_END_ARGS()


public:

	/**
	 * Destructor.
	 */
	~SSessionLauncherLaunchPage( );


public:

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs - The Slate argument list.
	 * @param InModel - The data model.
	 */
	void Construct(	const FArguments& InArgs, const FSessionLauncherModelRef& InModel );

	/**
	 * Refreshes the widget.
	 */
	void Refresh( );


private:

	// Callback for getting the visibility of the 'Cannot launch' text block.
	EVisibility HandleCannotLaunchTextBlockVisibility( ) const;

	// Callback for determining the visibility of the 'Launch Mode' box.
	EVisibility HandleLaunchModeBoxVisibility( ) const;

	// Callback for getting the content text of the 'Launch Mode' combo button.
	FString HandleLaunchModeComboButtonContentText( ) const;

	// Callback for clicking an item in the 'Launch Mode' menu.
	void HandleLaunchModeMenuEntryClicked( ELauncherProfileLaunchModes::Type LaunchMode );

	// Callback for getting the visibility state of the launch settings area.
	EVisibility HandleLaunchSettingsVisibility( ) const;

	// Callback for changing the selected profile in the profile manager.
	void HandleProfileManagerProfileSelected( const ILauncherProfilePtr& SelectedProfile, const ILauncherProfilePtr& PreviousProfile );

	// Callback for determining the visibility of a validation error icon.
	EVisibility HandleValidationErrorIconVisibility( ELauncherProfileValidationErrors::Type Error ) const;

	// Callback for updating any settings after the selected project has changed in the profile
	void HandleProfileProjectChanged();

private:

	// Holds the default role editor.
	TSharedPtr<SSessionLauncherLaunchRoleEditor> DefaultRoleEditor;

	// Holds the list of cultures that are available for the selected game.
	TArray<FString> AvailableCultures;

	// Holds the list of maps that are available for the selected game.
	TArray<FString> AvailableMaps;

	// Holds a pointer to the data model.
	FSessionLauncherModelPtr Model;
};

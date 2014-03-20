// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MainMenu.h: Declares the FMainMenu class.
=============================================================================*/

#pragma once


/**
 * Unreal editor main frame Slate widget
 */
class FMainMenu
{
public:

	/**
	 * Static: Creates a widget for the main menu bar.
	 *
	 * @param TabManager - Create the workspace menu based on this tab manager.
	 * @param Extender - Extensibility support for the menu.
	 *
	 * @return New widget
	 */
	static TSharedRef<SWidget> MakeMainMenu( const TSharedPtr<FTabManager>& TabManager, const TSharedRef<FExtender> Extender );

	/**
	 * Static: Creates a widget for the main tab's menu bar.  This is just like the main menu bar, but also includes.
	 * some "project level" menu items that we don't want propagated to most normal menus.
	 *
	 * @param TabManager - Create the workspace menu based on this tab manager.
	 * @param UserExtender - Extensibility support for the menu.
	 *
	 * @return New widget
	 */
	static TSharedRef<SWidget> MakeMainTabMenu( const TSharedPtr<FTabManager>& TabManager, const TSharedRef<FExtender> UserExtender );


protected:

	/**
	 * Called to fill the file menu's content.
	 *
	 * @param MenuBuilder (In, Out) - The multi-box builder that should be filled with content for this pull-down menu.
	 * @param Extender - Extensibility support for this menu.
	 */
	static void FillFileMenu( FMenuBuilder& MenuBuilder, const TSharedRef<FExtender> Extender );

	/**
	 * Called to fill the edit menu's content.
	 *
	 * @param MenuBuilder (In, Out)  - The multi-box builder that should be filled with content for this pull-down menu.
	 * @param Extender - Extensibility support for this menu.
	 * @param TabManager - A Tab Manager from which to populate tab spawner menu items.
	 */
	static void FillEditMenu( FMenuBuilder& MenuBuilder, const TSharedRef<FExtender> Extender, const TSharedPtr<FTabManager> TabManager );

	/**
	 * Called to fill the app menu's content.
	 *
	 * @param MenuBuilder (In, Out) - The multi-box builder that should be filled with content for this pull-down menu.
	 * @param Extender - Extensibility support for this menu.
	 * @param TabManager - A Tab Manager from which to populate tab spawner menu items.
	 */
	static void FillWindowMenu( FMenuBuilder& MenuBuilder, const TSharedRef<FExtender> Extender, const TSharedPtr<FTabManager> TabManager );

	/**
	 * Called to fill the help menu's content.
	 *
	 * @param MenuBuilder (In, Out) - The multi-box builder that should be filled with content for this pull-down menu.
	 * @param Extender - Extensibility support for this menu.
	 */
	static void FillHelpMenu( FMenuBuilder& MenuBuilder, const TSharedRef<FExtender> Extender );
};

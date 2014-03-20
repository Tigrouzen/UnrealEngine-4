// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SessionConsoleCategoryFilter.h: Declares the FSessionConsoleCategoryFilter class.
=============================================================================*/

#pragma once


/**
 * Type definition for shared pointers to instances of FSessionConsoleCategoryFilter.
 */
typedef TSharedPtr<class FSessionConsoleCategoryFilter> FSessionConsoleCategoryFilterPtr;

/**
 * Type definition for shared references to instances of FSessionConsoleCategoryFilter.
 */
typedef TSharedRef<class FSessionConsoleCategoryFilter> FSessionConsoleCategoryFilterRef;


/**
 * Delegate type for category filter state changes.
 *
 * The first parameter is the name of the category that changed its enabled state.
 * The second parameter is the new enabled state.
 */
DECLARE_DELEGATE_TwoParams(FOnSessionConsoleCategoryFilterStateChanged, const FName&, bool);


/**
 * Implements a view model for a console log category filter.
 */
class FSessionConsoleCategoryFilter
	: public TSharedFromThis<FSessionConsoleCategoryFilter>
{
public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InCategory - The filter category.
	 * @param InEnabled - Whether this filter is enabled.
	 * @param InOnStateChanged - A delegate that is executed when the filter's enabled state changed.
	 */
	FSessionConsoleCategoryFilter( const FName& InCategory, bool InEnabled, FOnSessionConsoleCategoryFilterStateChanged InOnStateChanged )
		: Category(InCategory)
		, Enabled(InEnabled)
		, OnStateChanged(InOnStateChanged)
	{ }


public:

	/**
	 * Enables or disables the filter based on the specified check box state.
	 *
	 * @param CheckState - The check box state.
	 */
	void EnableFromCheckState( ESlateCheckBoxState::Type CheckState )
	{
		Enabled = (CheckState == ESlateCheckBoxState::Checked);

		OnStateChanged.ExecuteIfBound(Category, Enabled);
	}

	/**
	 * Gets the filter's category.
	 *
	 * @return The category name.
	 */
	FName GetCategory( )
	{
		return Category;
	}

	/**
	 * Gets the check box state from the filter's enabled state.
	 *
	 * @return Checked if the filter is enabled, Unchecked otherwise.
	 */
	ESlateCheckBoxState::Type GetCheckStateFromIsEnabled( ) const
	{
		return (Enabled ? ESlateCheckBoxState::Checked : ESlateCheckBoxState::Unchecked);
	}

	/**
	 * Checks whether this filter is enabled.
	 *
	 * @param true if the filter is enabled, false otherwise.
	 */
	bool IsEnabled( ) const
	{
		return Enabled;
	}


private:

	// Holds the filter's category.
	FName Category;

	// Holds a flag indicating whether this filter is enabled.
	bool Enabled;


private:

	// Holds a delegate that is executed when the filter's enabled state changed.
	FOnSessionConsoleCategoryFilterStateChanged OnStateChanged;
};

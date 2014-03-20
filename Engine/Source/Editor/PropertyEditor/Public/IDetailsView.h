// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertyEditorDelegates.h"

/**
 * Init params for a details view widget
 */
struct FDetailsViewArgs
{
	/** Identifier for this details view; NAME_None if this view is anonymous */
	FName ViewIdentifier;
	/** Notify hook to call when properties are changed */
	FNotifyHook* NotifyHook;
	/** True if the viewed objects updates from editor selection */
	uint32 bUpdatesFromSelection : 1;
	/** True if this property view can be locked */
	uint32 bLockable : 1;
	/** True if we allow searching */
	uint32 bAllowSearch : 1;
	/** True if object selection wants to use the name area */
	uint32 bObjectsUseNameArea : 1;
	/** True if you want to not show the tip when no objects are selected (should only be used if viewing actors properties or bObjectsUseNameArea is true ) */
	uint32 bHideSelectionTip : 1;
	/** True if you want to hide the object/objects selected info area */
	uint32 bHideActorNameArea : 1;
	/** True if you want the search box to have initial keyboard focus */
	uint32 bSearchInitialKeyFocus : 1;
	/** Allow options to be changed */
	uint32 bShowOptions : 1;
	/** True if you want to show the 'Show Only Modified Properties'. Only valid in conjunction with bShowOptions */
	uint32 bShowModifiedPropertiesOption : 1;
	/** True if you want to show the actor label */
	uint32 bShowActorLabel : 1;

	/** Default constructor */
	FDetailsViewArgs( const bool InUpdateFromSelection = false, const bool InLockable = false, const bool InAllowSearch = true, const bool InObjectsUseNameArea = false, const bool InHideSelectionTip = false, FNotifyHook* InNotifyHook = NULL, const bool InSearchInitialKeyFocus = false, FName InViewIdentifier = NAME_None )
		: ViewIdentifier( InViewIdentifier )
		, NotifyHook( InNotifyHook ) 
		, bUpdatesFromSelection( InUpdateFromSelection )
		, bLockable(InLockable)
		, bAllowSearch( InAllowSearch )
		, bObjectsUseNameArea( InObjectsUseNameArea )
		, bHideSelectionTip( InHideSelectionTip )
		, bHideActorNameArea( false )
		, bSearchInitialKeyFocus( InSearchInitialKeyFocus )
		, bShowOptions( true )
		, bShowModifiedPropertiesOption(true)
		, bShowActorLabel(true)
	{
	}
};


/**
 * Interface class for all detail views
 */
class IDetailsView : public SCompoundWidget
{
public:
	/** Sets the callback for when the property view changes */
	virtual void SetOnObjectArrayChanged( FOnObjectArrayChanged OnObjectArrayChangedDelegate) = 0;

	/** List of all selected objects we are inspecting */
	virtual const TArray< TWeakObjectPtr<UObject> >& GetSelectedObjects() const = 0;

	/** @return	Returns list of selected actors we are inspecting */
	virtual const TArray< TWeakObjectPtr<AActor> >& GetSelectedActors() const = 0;

	/** @return Returns information about the selected set of actors */
	virtual const struct FSelectedActorInfo& GetSelectedActorInfo() const = 0;

	/** @return Whether or not the details view is viewing a CDO */
	virtual bool HasClassDefaultObject() const = 0;

	/** Gets the base class being viewed */
	virtual const UClass* GetBaseClass() const = 0;
	
	/**
	 * Registers a custom detail layout delegate for a specific class in this instance of the details view only
	 *
	 * @param Class	The class the custom detail layout is for
	 * @param DetailLayoutDelegate	The delegate to call when querying for custom detail layouts for the classes properties
	 */
	virtual void RegisterInstancedCustomPropertyLayout( UClass* Class, FOnGetDetailCustomizationInstance DetailLayoutDelegate ) = 0;

	/**
	 * Unregisters a custom detail layout delegate for a specific class in this instance of the details view only
	 *
	 * @param Class	The class with the custom detail layout delegate to remove
	 */
	virtual void UnregisterInstancedCustomPropertyLayout( UClass* Class ) = 0;

	/**
	 * Sets the objects this details view is viewing
	 *
	 * @param InObjects		The list of objects to observe
	 * @param bForceRefresh	If true, doesn't check if new objects are being set
	 */
	virtual void SetObjects( const TArray<UObject*>& InObjects, bool bForceRefresh = false ) = 0;
	virtual void SetObjects( const TArray< TWeakObjectPtr< UObject > >& InObjects, bool bForceRefresh = false ) = 0;

	/**
	 * Sets a single objects that details view is viewing
	 *
	 * @param InObject		The objects to view
	 * @param bForceRefresh	If true, doesn't check if new objects are being set
	 */
	virtual void SetObject( UObject* InObject, bool bForceRefresh = false ) = 0;

	/**
	 * Returns true if the details view is locked and cant have its observed objects changed 
	 */
	virtual bool IsLocked() const = 0;

	/**
	 * @return true of the details view can be updated from editor selection
	 */
	virtual bool IsUpdatable() const = 0;

	/** @return The identifier for this details view, or NAME_None is this view is anonymous */
	virtual FName GetIdentifier() const = 0;

	/**
	 * Sets a delegate to call to determine if a specific property should be visible in this instance of the details view
	 */ 
	virtual void SetIsPropertyVisibleDelegate( FIsPropertyVisible InIsPropertyVisible ) = 0;
		
	/**
	 * Sets a delegate to call to layout generic details not specific to an object being viewed
	 */ 
	virtual void SetGenericLayoutDetailsDelegate( FOnGetDetailCustomizationInstance OnGetGenericDetails ) = 0;

	/**
	 * Sets a delegate to call to determine if the properties  editing is enabled
	 */ 
	virtual void SetIsPropertyEditingEnabledDelegate( FIsPropertyEditingEnabled IsPropertyEditingEnabled ) = 0;

	/**
	 * A delegate which is called after properties have been edited and PostEditChange has been called on all objects.
	 * This can be used to safely make changes to data that the details panel is observing instead of during PostEditChange (which is
	 * unsafe)
	 */
	virtual FOnFinishedChangingProperties& OnFinishedChangingProperties() = 0;

	/** 
	 * Sets the visible state of the filter box/property grid area
	 */
	virtual void HideFilterArea(bool bIsVisible) = 0;
};

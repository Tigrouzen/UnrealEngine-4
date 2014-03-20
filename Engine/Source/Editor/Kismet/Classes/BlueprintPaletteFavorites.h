// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BlueprintPaletteFavorites.generated.h"

/*******************************************************************************
* FFavoritedPaletteItem
*******************************************************************************/

USTRUCT()
struct FFavoritedBlueprintPaletteItem
{
	GENERATED_USTRUCT_BODY()

public:
	/**
	 * Sometime favorites can be coming from user edited .ini files, so this 
	 * converts that readable text into a favorite (since the strings are user 
	 * generated, there could be some error, so sure to check its validity).
	 * 
	 * @param  SerializedAction		A string representing a serialized favorite.
	 */
	FFavoritedBlueprintPaletteItem(FString const& SerializedAction);

	/**
	 * Constructs a favorite from the specified palette action (some palette actions
	 * cannot be favorited, so make sure to check its validity).
	 * 
	 * @param  PaletteAction	The action you wish to favorite.
	 */
	FFavoritedBlueprintPaletteItem(TSharedPtr<FEdGraphSchemaAction> PaletteAction = NULL);

	/**
	 * Sometimes we're not able to construct favorites from specified actions, 
	 * so this provides us a way to check this item's validity.
	 * 
	 * @return True if this favorite is valid (refers to a specific node), false if not.
	 */
	bool IsValid() const;

	/**
	 * Checks to see if this favorite matches the specified one.
	 * 
	 * @param  Rhs	The other favorite you want to compare with.
	 * @return True if this favorite matches the other one, false if not.
	 */
	bool operator==(FFavoritedBlueprintPaletteItem const& Rhs) const;

	/**
	 * Checks to see if this favorite represents the supplied ed-graph action 
	 * (so we can match them together, and construct a favorites list).
	 * 
	 * @param  PaletteAction	The action you want to compare with.
	 * @return True if this favorite represents the specified action, false if not.
	 */
	bool operator==(TSharedPtr<FEdGraphSchemaAction> PaletteAction) const;

	/**
	 * We want to be able to specify some of these in .ini files, so let we have
	 * to have a readable string representation for them.
	 * 
	 * @return A string representation of this item.
	 */
	FString ToString() const;

private:

	/**
	 * Blueprint palette actions are used to place new nodes. So to uniquely 
	 * identify the action, we specify the node that they generate (this is the 
	 * node's type name).
	 */
	UPROPERTY()
	FString  NodeClassName;

	/**
	 * Multiple node types could share the same name, so to ensure we get the 
	 * right one we track its outer as well.
	 */
	UPROPERTY()
	UObject* NodeClassOuter;
	
	/** 
	 * Sometimes a node has multiple permutations (like UK2Node_CallFunction), so
	 * you need a more granular way of identifying it (like the function itself). 
	 * This could be empty, or filled in if we need that additional granularity.
	 */
	UPROPERTY()
	FString  FieldName;

	/** 
	 * Like FieldName, this is used to more granularly identify UK2Nodes (could
	 * be null if it's not needed).
	 */
	UPROPERTY()
	UObject* FieldOuter;
};

/*******************************************************************************
* FBlueprintPaletteFavorites
*******************************************************************************/

UCLASS(config=EditorUserSettings)
class KISMET_API UBlueprintPaletteFavorites : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	// Begin UObject interface
	virtual void PostInitProperties() OVERRIDE;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) OVERRIDE;
	// End UObject interface

	/**
	 * Not all palette actions can be turned into a favorite. This method is
	 * meant to catch those cases.
	 * 
	 * @param  PaletteAction	The action you wish to check.
	 * @return True if the action can be turned into a favorite, false if not.
	 */
	bool CanBeFavorited(TSharedPtr<FEdGraphSchemaAction> PaletteAction) const;

	/**
	 * This method can be used to see if a specified action is already favorited 
	 * by the user.
	 * 
	 * @param  PaletteAction	The action you wish to check.
	 * @return True if this action is already favorited, false if it not.
	 */
	bool IsFavorited(TSharedPtr<FEdGraphSchemaAction> PaletteAction) const;

	/**
	 * Adds the specified action to the current favorites list (fails if the action
	 * action can't be favorited, or if the favorite already exists). Will also 
	 * convert the user's profile to a custom one (if it isn't already).
	 * 
	 * @param  PaletteAction	The action you wish to add.
	 */
	void AddFavorite(TSharedPtr<FEdGraphSchemaAction> PaletteAction);

	/**
	 * Adds the specified actions to the current favorites list. Will also 
	 * convert the user's profile to a custom one (if it isn't already).
	 * 
	 * @param  PaletteActions	An array of action you wish to add.
	 */
	void AddFavorites(TArray< TSharedPtr<FEdGraphSchemaAction> > PaletteActions);

	/**
	 * Removes the specified action to the current favorites list (if it's 
	 * there). Will also convert the user's profile to a custom one (if it isn't 
	 * already).
	 * 
	 * @param  PaletteAction	The action you wish to remove.
	 */
	void RemoveFavorite(TSharedPtr<FEdGraphSchemaAction> PaletteAction);

	/**
	 * Remove the specified actions from the current favorites list. Will also 
	 * convert the user's profile to a custom one (if it isn't already).
	 * 
	 * @param  PaletteActions	An array of action you wish to remove.
	 */
	void RemoveFavorites(TArray< TSharedPtr<FEdGraphSchemaAction> > PaletteActions);

	/**
	 * Throws out all current favorites and loads in ones for the specified 
	 * profile (explicitly laid out in the editor .ini file).
	 * 
	 * @param  ProfileName	The name of the profile you wish to load.
	 */
	void LoadProfile(FString const& ProfileName);

	/**
	 * Provides any easy way to see if the user is currently using his/her own
	 * manual profile (one he/she set up through the tool).
	 * 
	 * @return True if he/she is using their own profile, false if it is a predefined one.
	 */
	bool IsUsingCustomProfile() const;

	/**
	 * Gets the user's currently set profile. If the user hasn't manually set 
	 * one themselves, then it'll return the default profile identifier.
	 * 
	 * @return A string name, representing the currently set profile (defined in the editor .ini file)
	 */
	FString const& GetCurrentProfile() const;

	/**
	 * Removes every single favorite and sets the user's profile to a custom one
	 * (if it isn't already).
	 */
	void ClearAllFavorites();

	/** 
	 * A event for users to hook into (specifically the UI), so they can be 
	 * notified when a change to the favorites has been made.
	 */
	DECLARE_EVENT(UBlueprintPaletteFavorites, FBlueprintFavoritesUpdatedEvent);
	FBlueprintFavoritesUpdatedEvent OnFavoritesUpdated;

private:
	/**
	 * Gets the default profile id specified by the user's .ini file (so we can 
	 * easily change it without modifying code).
	 * 
	 * @return A string name, representing the default profile (defined in the editor .ini file)
	 */
	FString const& GetDefaultProfileId() const;

	/**
	 * Throws out all current favorites and loads in ones for specified by 
	 * CurrentProfile.
	 */
	void LoadSetProfile();

	/**
	 * Fills the CurrentFavorites array with items that have been loaded into 
	 * the CustomFavorites array.
	 */
	void LoadCustomFavorites();

	/**
	 * Modifies the CurrentProfile member, but flags the property as edited so 
	 * that it invokes a save of the config.
	 * 
	 * @param  NewProfileName	The new profile that you want favorites saved with.
	 */
	void SetProfile(FString const& NewProfileName);

public:
	/** 
	 * A list of strings that are used to identify specific palette actions. 
	 * This is what gets saved out when the user has customized their own set, 
	 * and is not updated untile PreSave().
	 */
	UPROPERTY(config)
	TArray<FString> CustomFavorites;

	/** 
	 * A list of favorites that is constructed in PostLoad() (either from a 
	 * profile or the user's set of CustomFavorites). This list is up to date 
	 * and maintained at runtime.
	 */
	UPROPERTY(Transient)
	TArray<FFavoritedBlueprintPaletteItem> CurrentFavorites;

	/** 
	 * Users could load pre-existing profiles (intended to share favorites, and 
	 * hook into tutorials). If empty, the default profile will be loaded; if 
	 * the user has customized a pre-existing profile, then this will be "CustomProfile".
	 */
	UPROPERTY(config)
	FString CurrentProfile;
};


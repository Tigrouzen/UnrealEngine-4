// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

namespace ContentBrowserUtils
{
	/** Loads the specified object if needed and opens the asset editor for it */
	bool OpenEditorForAsset(const FString& ObjectPath);

	/** Opens the asset editor for the specified asset */
	bool OpenEditorForAsset(UObject* Asset);

	/** Opens the asset editor for the specified assets */
	bool OpenEditorForAsset(const TArray<UObject*>& Assets);

	/**
	  * Makes sure the specified assets are loaded into memory.
	  * 
	  * @param ObjectPaths The paths to the objects to load.
	  * @param LoadedObjects The returned list of objects that were already loaded or loaded by this method.
	  * @return false if user canceled after being warned about loading very many packages.
	  */
	bool LoadAssetsIfNeeded(const TArray<FString>& ObjectPaths, TArray<UObject*>& LoadedObjects, bool bAllowedToPromptToLoadAssets = true);

	/**
	 * Determines if enough assets are unloaded that we should prompt the user to load them instead of loading them automatically
	 *
	 * @param ObjectPaths		Paths to assets that may need to be loaded
	 * @param OutUnloadedObjects	List of the unloaded object paths
	 * @return true if the user should be prompted to load assets
	 */
	bool ShouldPromptToLoadAssets(const TArray<FString>& ObjectPaths, TArray<FString>& OutUnloadedObjects);

	/**
	 * Prompts the user to load the list of unloaded objects
 	 *
	 * @param UnloadedObjects	The list of unloaded objects that we should prompt for loading
	 * @param true if the user allows the objects to be loaded
	 */
	bool PromptToLoadAssets(const TArray<FString>& UnloadedObjects);

	/** Renames an asset */
	void RenameAsset(UObject* Asset, const FString& NewName, FText& ErrorMessage);

	/** Renames a folder */
	void RenameFolder(const FString& FolderPath, const FString& NewName, FText& ErrorMessage);

	/**
	  * Moves assets to a new path
	  * 
	  * @param Assets The assets to move
	  * @param DestPath The destination folder in which to move the assets
	  * @param SourcePath If non-empty, this will specify the base folder which will cause the move to maintain folder structure
	  * @return false if the move was not successful
	  */
	void MoveAssets(const TArray<UObject*>& Assets, const FString& DestPath, const FString& SourcePath = FString());

	/** Attempts to deletes the specified assets. Returns the number of assets deleted */
	int32 DeleteAssets(const TArray<UObject*>& AssetsToDelete);

	/** Attempts to delete the specified folders and all assets inside them. Returns true if the operation succeeded. */
	bool DeleteFolders(const TArray<FString>& PathsToDelete);

	/** Gets an array of assets inside the specified folders */
	void GetAssetsInPaths(const TArray<FString>& InPaths, TArray<FAssetData>& OutAssetDataList);

	/** Saves all the specified packages */
	bool SavePackages(const TArray<UPackage*>& Packages);

	/** Prompts to save all modified packages */
	bool SaveDirtyPackages();

	/** Loads all the specified packages */
	TArray<UPackage*> LoadPackages(const TArray<FString>& PackageNames);

	/** Displays a modeless message at the specified anchor. It is fine to specify a zero-size anchor, just use the top and left fields */
	void DisplayMessage(const FText& Message, const FSlateRect& ScreenAnchor, const TSharedRef<SWidget>& ParentContent);

	/** Displays a modeless message asking yes or no type question */
	void DisplayConfirmationPopup(const FText& Message, const FText& YesString, const FText& NoString, const TSharedRef<SWidget>& ParentContent, const FOnClicked& OnYesClicked, const FOnClicked& OnNoClicked = FOnClicked());

	/** Copies all assets in all source paths to the destination path, preserving path structure */
	void CopyFolders(const TArray<FString>& InSourcePathNames, const FString& DestPath);

	/** Moves all assets in all source paths to the destination path, preserving path structure */
	void MoveFolders(const TArray<FString>& InSourcePathNames, const FString& DestPath);

	/**
	  * A helper function for folder drag/drop which loads all assets in a path (including sub-paths) and returns the assets found
	  * 
	  * @param SourcePathNames				The paths to the folders to drag/drop
	  * @param OutSourcePathToLoadedAssets	The map of source folder paths to assets found
	  */
	void PrepareFoldersForDragDrop(const TArray<FString>& SourcePathNames, TMap< FString, TArray<UObject*> >& OutSourcePathToLoadedAssets);

	/** Copies references to the specified assets to the clipboard */
	void CopyAssetReferencesToClipboard(const TArray<FAssetData>& AssetsToCopy);

	/**
	 * Capture active viewport to thumbnail and assigns that thumbnail to incoming assets
	 *
	 * @param InViewport - viewport to sample from
	 * @param InAssetsToAssign - assets that should receive the new thumbnail ONLY if they are assets that use GenericThumbnails
	 */
	void CaptureThumbnailFromViewport(FViewport* InViewport, const TArray<FAssetData>& InAssetsToAssign);

	/**
	 * Clears custom thumbnails for the selected assets
	 *
	 * @param InAssetsToAssign - assets that should have their thumbnail cleared
	 */
	void ClearCustomThumbnails(const TArray<FAssetData>& InAssetsToAssign);

	/** Returns true if the specified asset that uses shared thumbnails has a thumbnail assigned to it */
	bool AssetHasCustomThumbnail( const FAssetData& AssetData );

	/** Returns true if the passed-in path is a engine folder */
	bool IsEngineFolder( const FString& InPath );

	/** Returns true if the passed-in path is a developers folder */
	bool IsDevelopersFolder( const FString& InPath );

	/** Get all the objects in a list of asset data */
	void GetObjectsInAssetData(const TArray<FAssetData>& AssetList, TArray<UObject*>& OutDroppedObjects);

	/** Returns true if the supplied folder name can be used as part of a package name */
	bool IsValidFolderName(const FString& FolderName, FText& Reason);

	/** Returns true if the path specified exists as a folder in the asset registry */
	bool DoesFolderExist(const FString& FolderPath);

	/** Returns true if the pass-ed-in path is one of the asset root directories */
	bool IsAssetRootDir(const FString& FolderPath);

	/**
	 * Loads the color of this path from the config
	 *
	 * @param FolderPath - The path to the folder
	 * @return The color the folder should appear as, will be NULL if not customized
	 */
	const TSharedPtr<FLinearColor> LoadColor(const FString& FolderPath);

	/**
	 * Saves the color of the path to the config
	 *
	 * @param FolderPath - The path to the folder
	 * @param FolderColor - The color the folder should appear as
	 * @param bForceAdd - If true, force the color to be added for the path
	 */
	void SaveColor(const FString& FolderPath, const TSharedPtr<FLinearColor> FolderColor, bool bForceAdd = false);

	/**
	 * Checks to see if any folder has a custom color, optionally outputs them to a list
	 *
	 * @param OutColors - If specified, returns all the custom colors being used
	 * @return true, if custom colors are present
	 */
	bool HasCustomColors( TArray< FLinearColor >* OutColors = NULL );

	/** Gets the default color the folder should appear as */
	FLinearColor GetDefaultColor();

	/** Gets the platform specific text for the "explore" command (FPlatformProcess::ExploreFolder) */
	FText GetExploreFolderText();
}
// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

class IAssetTypeActions;
class FAssetData;
class UFactory;

struct FAssetRenameData
{
	TWeakObjectPtr<UObject> Asset;
	FString PackagePath;
	FString NewName;

	FAssetRenameData(const TWeakObjectPtr<UObject>& InAsset, const FString& InPackagePath, const FString& InNewName)
		: Asset(InAsset)
		, PackagePath(InPackagePath)
		, NewName(InNewName)
	{}
};

class IAssetTools
{
public:
	/** Virtual destructor */
	virtual ~IAssetTools() {}

	/** Registers an asset type actions object so it can provide information about and actions for asset types. */
	virtual void RegisterAssetTypeActions(const TSharedRef<IAssetTypeActions>& NewActions) = 0;

	/** Unregisters an asset type actions object. It will no longer provide information about or actions for asset types. */
	virtual void UnregisterAssetTypeActions(const TSharedRef<IAssetTypeActions>& ActionsToRemove) = 0;

	/** Generates a list of currently registered AssetTypeActions */
	virtual void GetAssetTypeActionsList( TArray<TWeakPtr<IAssetTypeActions>>& OutAssetTypeActionsList ) const = 0;

	/** Gets the appropriate AssetTypeActions for the supplied class */
	virtual TWeakPtr<IAssetTypeActions> GetAssetTypeActionsForClass( UClass* Class ) const = 0;

	/**
	 * Fills out a menubuilder with a list of commands that can be applied to the specified objects.
	 *
	 * @param InObjects the objects for which to generate type-specific menu options
	 * @param MenuBuilder the menu in which to build options
	 * @param bIncludeHeader if true, will include a heading in the menu if any options were found
	 * @return true if any options were added to the MenuBuilder
	 */
	virtual bool GetAssetActions( const TArray<UObject*>& InObjects, class FMenuBuilder& MenuBuilder, bool bIncludeHeading = true ) = 0;

	/**
	 * Creates an asset with the specified name, path, and factory
	 *
	 * @param AssetName the name of the new asset
	 * @param PackagePath the package that will contain the new asset
	 * @param AssetClass the class of the new asset
	 * @param Factory the factory that will build the new asset
	 * @param CallingContext optional name of the module or method calling CreateAsset() - this is passed to the factory
	 * @return the new asset or NULL if it fails
	 */
	virtual UObject* CreateAsset(const FString& AssetName, const FString& PackagePath, UClass* AssetClass, UFactory* Factory, FName CallingContext = NAME_None) = 0;

	/** Creates an asset with the specified name and path. Uses OriginalObject as the duplication source. */
	virtual UObject* DuplicateAsset(const FString& AssetName, const FString& PackagePath, UObject* OriginalObject) = 0;

	/** Renames assets using the specified names. */
	virtual void RenameAssets(const TArray<FAssetRenameData>& AssetsAndNames) const = 0;

	/** Opens a file open dialog to choose files to import to the destination path. */
	virtual TArray<UObject*> ImportAssets(const FString& DestinationPath) = 0;

	/** Imports the specified files to the destination path. */
	virtual TArray<UObject*> ImportAssets(const TArray<FString>& Files, const FString& DestinationPath) const = 0;

	/** Creates a unique package and asset name taking the form InBasePackageName+InSuffix */
	virtual void CreateUniqueAssetName(const FString& InBasePackageName, const FString& InSuffix, FString& OutPackageName, FString& OutAssetName) const = 0;

	/** Returns true if the specified asset uses a stock thumbnail resource */
	virtual bool AssetUsesGenericThumbnail( const FAssetData& AssetData ) const = 0;

	/**
	 * Try to diff the local version of an asset against the latest one from the depot 
	 * 
	 * @param InObject - The object we want to compare against the depot
	 * @param InPackagePath - The fullpath to the package
	 * @param InPackageName - The name of the package
	*/
	virtual void DiffAgainstDepot(UObject* InObject, const FString& InPackagePath, const FString& InPackageName) const = 0;

	/** Try and diff two assets using class-specific tool. Will do nothing if either asset is NULL, or they are not the same class. */
	virtual void DiffAssets(UObject* OldAsset, UObject* NewAsset, const struct FRevisionInfo& OldRevision, const struct FRevisionInfo& NewRevision) const = 0;

	/** Util for dumping an asset to a temporary text file. Returns absolute filename to temp file */
	virtual FString DumpAssetToTempFile(UObject* Asset) const = 0;

	/* Attempt to spawn diff tool as external process */
	virtual bool CreateDiffProcess(const FString& DiffCommand, const FString& DiffArgs) const = 0;

	/* Migrate packages to another game content folder */
	virtual void MigratePackages(const TArray<FName>& PackageNamesToMigrate) const = 0;
};

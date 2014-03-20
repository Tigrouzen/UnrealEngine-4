// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

class FAssetContextMenu : public TSharedFromThis<FAssetContextMenu>
{
public:
	/** Constructor */
	FAssetContextMenu(const TWeakPtr<SAssetView>& InAssetView);

	/** Bind menu selection commands to the command list */
	void BindCommands(TSharedPtr< FUICommandList > InCommandList);

	/** Makes the context menu widget */
	TSharedRef<SWidget> MakeContextMenu(const TArray<FAssetData>& SelectedAssets, const FSourcesData& InSourcesData, TSharedPtr< FUICommandList > InCommandList);

	/** Delegate for when the context menu requests a rename */
	void SetOnFindInAssetTreeRequested(const FOnFindInAssetTreeRequested& InOnFindInAssetTreeRequested);

	/** Delegate for when the context menu requests a rename */
	DECLARE_DELEGATE_OneParam(FOnRenameRequested, const FAssetData& /*AssetToRename*/);
	void SetOnRenameRequested(const FOnRenameRequested& InOnRenameRequested);

	/** Delegate for when the context menu requests a rename of a folder */
	DECLARE_DELEGATE_OneParam(FOnRenameFolderRequested, const FString& /*FolderToRename*/);
	void SetOnRenameFolderRequested(const FOnRenameFolderRequested& InOnRenameFolderRequested);

	/** Delegate for when the context menu requests a rename */
	DECLARE_DELEGATE_OneParam(FOnDuplicateRequested, const TWeakObjectPtr<UObject>& /*OriginalObject*/);
	void SetOnDuplicateRequested(const FOnDuplicateRequested& InOnDuplicateRequested);

	/** Delegate for when the context menu requests an asset view refresh */
	DECLARE_DELEGATE(FOnAssetViewRefreshRequested);
	void SetOnAssetViewRefreshRequested(const FOnAssetViewRefreshRequested& InOnAssetViewRefreshRequested);

private:
	/** Adds common menu options to a menu builder. Returns true if any options were added. */
	bool AddCommonMenuOptions(FMenuBuilder& MenuBuilder);

	/** Adds asset reference menu options to a menu builder. Returns true if any options were added. */
	bool AddReferenceMenuOptions(FMenuBuilder& MenuBuilder);

	/** Adds asset type-specific menu options to a menu builder. Returns true if any options were added. */
	bool AddAssetTypeMenuOptions(FMenuBuilder& MenuBuilder);

	/** Adds source control menu options to a menu builder. Returns true if any options were added. */
	bool AddSourceControlMenuOptions(FMenuBuilder& MenuBuilder);

	/** Adds menu options related to working with collections */
	bool AddCollectionMenuOptions(FMenuBuilder& MenuBuilder);

	/** Handler for when sync to asset tree is selected */
	void ExecuteSyncToAssetTree();

	/** Handler for when find in explorer is selected */
	void ExecuteFindInExplorer();

	/** Handler for when create using asset is selected */
	void ExecuteCreateBlueprintUsing();

	/** Handler for when "Find in World" is selected */
	void ExecuteFindAssetInWorld();

	/** Handler for when "Properties" is selected */
	void ExecuteProperties();

	/** Handler for when "Property Matrix..." is selected */
	void ExecutePropertyMatrix();

	/** Handler for when "Save Asset" is selected */
	void ExecuteSaveAsset();

	/** Handler for when "Diff Selected" is selected */
	void ExecuteDiffSelected() const;

	/** Handler for Duplicate */
	void ExecuteDuplicate();

	/** Handler for Rename */
	void ExecuteRename();

	/** Handler for Delete */
	void ExecuteDelete();

	/** Handler for confirmation of folder deletion */
	FReply ExecuteDeleteFolderConfirmed();

	/** Handler for Consolidate */
	void ExecuteConsolidate();

	/** Handler for Capture Thumbnail */
	void ExecuteCaptureThumbnail();

	/** Handler for Clear Thumbnail */
	void ExecuteClearThumbnail();

	/** Handler for when "Migrate Asset" is selected */
	void ExecuteMigrateAsset();

	/** Handler for ShowReferenceViewer */
	void ExecuteShowReferenceViewer();

	/** Handler for CopyReference */
	void ExecuteCopyReference();

	/** Handler for Export */
	void ExecuteExport();

	/** Handler for Bulk Export */
	void ExecuteBulkExport();

	/** Handler for when "Remove from collection" is selected */
	void ExecuteRemoveFromCollection();

	/** Handler for when "Refresh source control" is selected */
	void ExecuteSCCRefresh();

	/** Handler for when "Checkout from source control" is selected */
	void ExecuteSCCCheckOut();

	/** Handler for when "Open for add to source control" is selected */
	void ExecuteSCCOpenForAdd();

	/** Handler for when "Checkin to source control" is selected */
	void ExecuteSCCCheckIn();

	/** Handler for when "Source Control History" is selected */
	void ExecuteSCCHistory();

	/** Handler for when "Diff Against Depot" is selected */
	void ExecuteSCCDiffAgainstDepot() const;

	/** Handler for when "Source Control Revert" is selected */
	void ExecuteSCCRevert();

	/** Handler for when "Source Control Sync" is selected */
	void ExecuteSCCSync();

	/** Handler for when source control is disabled */
	void ExecuteEnableSourceControl();

	/** Handler to check to see if a sync to asset tree command is allowed */
	bool CanExecuteSyncToAssetTree() const;

	/** Handler to check to see if a find in explorer command is allowed */
	bool CanExecuteFindInExplorer() const;	

	/** Handler to check if we can create blueprint using selected asset */
	bool CanExecuteCreateBlueprintUsing() const;

	/** Handler to check to see if a find in world command is allowed */
	bool CanExecuteFindAssetInWorld() const;

	/** Handler to check to see if a properties command is allowed */
	bool CanExecuteProperties() const;

	/** Handler to check to see if a property matrix command is allowed */
	bool CanExecutePropertyMatrix() const;

	/** Handler to check to see if a duplicate command is allowed */
	bool CanExecuteDuplicate() const;

	/** Handler to check to see if a rename command is allowed */
	bool CanExecuteRename() const;

	/** Handler to check to see if a delete command is allowed */
	bool CanExecuteDelete() const;

	/** Handler to check to see if a "Remove from collection" command is allowed */
	bool CanExecuteRemoveFromCollection() const;

	/** Handler to check to see if "Refresh source control" can be executed */
	bool CanExecuteSCCRefresh() const;

	/** Handler to check to see if "Checkout from source control" can be executed */
	bool CanExecuteSCCCheckOut() const;

	/** Handler to check to see if "Open for add to source control" can be executed */
	bool CanExecuteSCCOpenForAdd() const;

	/** Handler to check to see if "Checkin to source control" can be executed */
	bool CanExecuteSCCCheckIn() const;

	/** Handler to check to see if "Source Control History" can be executed */
	bool CanExecuteSCCHistory() const;

	/** Handler to check to see if "Source Control Revert" can be executed */
	bool CanExecuteSCCRevert() const;

	/** Handler to check to see if "Source Control Sync" can be executed */
	bool CanExecuteSCCSync() const;

	/** Handler to check to see if "Diff Against Depot" can be executed */
	bool CanExecuteSCCDiffAgainstDepot() const;

	/** Handler to check to see if "Enable source control" can be executed */
	bool CanExecuteSCCEnable() const;

	/** Handler to check to see if "Consolidate" can be executed */
	bool CanExecuteConsolidate() const;

	/** Handler to check to see if "Save Asset" can be executed */
	bool CanExecuteSaveAsset() const;

	/** Handler to check to see if "Diff Selected" can be executed */
	bool CanExecuteDiffSelected() const;

	/** Handler to check to see if "Capture Thumbnail" can be executed */
	bool CanExecuteCaptureThumbnail() const;

	/** Handler to check to see if "Clear Thumbnail" can be executed */
	bool CanExecuteClearThumbnail() const;

	/** Handler to check to see if "Clear Thumbnail" should be visible */
	bool CanClearCustomThumbnails() const;

	/** Initializes some variable used to in "CanExecute" checks that won't change at runtime or are too expensive to check every frame. */
	void CacheCanExecuteVars();

	/** Helper function to gather the package names of all selected assets */
	void GetSelectedPackageNames(TArray<FString>& OutPackageNames) const;

	/** Helper function to gather the packages containing all selected assets */
	void GetSelectedPackages(TArray<UPackage*>& OutPackages) const;

private:
	/** Generates a list of selected assets in the content browser */
	void GetSelectedAssets(TArray<UObject*>& Assets, bool SkipRedirectors);

	TArray<FAssetData> SelectedAssets;
	FSourcesData SourcesData;

	/** The asset view this context menu is a part of */
	TWeakPtr<SAssetView> AssetView;

	FOnFindInAssetTreeRequested OnFindInAssetTreeRequested;
	FOnRenameRequested OnRenameRequested;
	FOnRenameFolderRequested OnRenameFolderRequested;
	FOnDuplicateRequested OnDuplicateRequested;
	FOnAssetViewRefreshRequested OnAssetViewRefreshRequested;

	/** Cached CanExecute vars */
	bool bAtLeastOneNonRedirectorSelected;
	bool bCanExecuteSCCCheckOut;
	bool bCanExecuteSCCOpenForAdd;
	bool bCanExecuteSCCCheckIn;
	bool bCanExecuteSCCHistory;
	bool bCanExecuteSCCRevert;
	bool bCanExecuteSCCSync;
};

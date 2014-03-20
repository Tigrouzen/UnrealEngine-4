// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#ifndef __PACKAGEAUTOSAVER_H__
#define __PACKAGEAUTOSAVER_H__

#include "IPackageAutoSaver.h"

class SNotificationItem;

/** A class to handle the creation, destruction, and restoration of auto-saved packages */
class FPackageAutoSaver : public IPackageAutoSaver
{
public:
	FPackageAutoSaver();
	virtual ~FPackageAutoSaver();

	/** IPackageAutoSaver */
	virtual void UpdateAutoSaveCount(const float DeltaSeconds) OVERRIDE;
	virtual void ResetAutoSaveTimer() OVERRIDE;
	virtual void ForceAutoSaveTimer() OVERRIDE;
	virtual void ForceMinimumTimeTillAutoSave(const float TimeTillAutoSave = 10.0f) OVERRIDE;
	virtual void AttemptAutoSave() OVERRIDE;
	virtual void LoadRestoreFile() OVERRIDE;
	virtual void UpdateRestoreFile(const bool bRestoreEnabled) const OVERRIDE;
	virtual bool HasPackagesToRestore() const OVERRIDE;
	virtual void OfferToRestorePackages() OVERRIDE;
	virtual bool IsAutoSaving() const OVERRIDE
	{
		return bIsAutoSaving;
	}

private:
	/**
	 * Called when a package dirty state has been updated
	 *
	 * @param Pkg The package that was changed
	 */
	void OnPackageDirtyStateUpdated(UPackage* Pkg);

	/**
	 * Called when a package has been saved
	 *
	 * @param Filename The filename the package was saved to
	 * @param Obj The package that was saved
	 */
	void OnPackageSaved(const FString& Filename, UObject* Obj);

	/**
	 * Update the dirty lists for the current package
	 *
	 * @param Pkg The package to update the lists for
	 */
	void UpdateDirtyListsForPackage(UPackage* Pkg);

	/** @return Returns whether or not the user is able to auto-save. */
	bool CanAutoSave() const;

	/** @return Returns whether or not we would need to perform an auto-save (note: does not check if we can perform an auto-save, only that we should if we could). */
	bool DoPackagesNeedAutoSave() const;

	/** @return The notification text to be displayed during auto-saving */
	static FText GetAutoSaveNotificationText(const int32 TimeInSecondsUntilAutosave);

	/** 
	 * @param bIgnoreCanAutoSave if True this function returns the time regardless of whether auto-save is enabled.
	 * 
	 * @return Returns the amount of time until the next auto-save in seconds. Or -1 if auto-save is disabled.
	 */
	int32 GetTimeTillAutoSave(const bool bIgnoreCanAutoSave = false) const;

	/**
	 * Attempts to launch a auto-save warning notification if auto-save is imminent, if this is already the case
	 * it will update the time remaining. 
	*/
	void UpdateAutoSaveNotification();

	/** Closes the auto-save warning notification if open, with an appropriate message based on whether it was successful */
	void CloseAutoSaveNotification(const bool Success);

	/** Used as a callback for auto-save warning buttons, called when auto-save is forced early */
	void OnAutoSaveSave();

	/** Used as a callback for auto-save warning buttons, called when auto-save is postponed */
	void OnAutoSaveCancel();

	/** Clear out any stale pointers in the dirty packages containers */
	void ClearStalePointers();

	/** The current auto-save number, appended to the auto-save map name, wraps after 10 */
	int32 AutoSaveIndex;

	/** The number of 10-sec intervals that have passed since last auto-save. */
	float AutoSaveCount;

	/** If we are currently auto-saving */
	bool bIsAutoSaving;

	/** Flag for whether auto-save warning notification has been launched */
	bool bAutoSaveNotificationLaunched;

	/** If we are delaying the time a little bit because we failed to save */
	bool bDelayingDueToFailedSave;

	/** Used to reference to the active auto-save warning notification */
	TWeakPtr<SNotificationItem> AutoSaveNotificationPtr;

	/** Packages that have been dirtied and not saved by the user, mapped to their latest auto-save file */
	TMap< TWeakObjectPtr<UPackage>, FString > DirtyPackagesForUserSave;

	/** Restore information that was loaded following a crash */
	TMap<FString, FString> PackagesThatCanBeRestored;
};

#endif // __PACKAGEAUTOSAVER_H__

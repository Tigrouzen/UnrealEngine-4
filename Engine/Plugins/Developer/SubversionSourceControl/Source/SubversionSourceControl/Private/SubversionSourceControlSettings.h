// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

class FSubversionSourceControlSettings
{
public:
	/** Get the Subversion Repository */
	const FString& GetRepository() const;

	/** Set the Subversion host */
	void SetRepository(const FString& InString);

	/** Get the Subversion username */
	const FString& GetUserName() const;

	/** Set the Subversion username */
	void SetUserName(const FString& InString);

	/** Get the Subversion labels root */
	const FString& GetLabelsRoot() const;

	/** Set the Subversion labels root */
	void SetLabelsRoot(const FString& InString);

	/** Load settings from ini file */
	void LoadSettings();

	/** Save settings to ini file */
	void SaveSettings() const;

private:
	/** A critical section for settings access */
	mutable FCriticalSection CriticalSection;

	/** Address of SVN repository */
	FString Repository;

	/** SVN username */
	FString UserName;

	/** Relative path to repository root where labels/tags are stored. For example, if the labels/tags were to be stored in 'http://repo-name/tags/', then the path here would be 'tags/' */
	FString LabelsRoot;
};
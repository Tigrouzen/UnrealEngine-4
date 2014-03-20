// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISourceControlRevision.h"

class FSubversionSourceControlRevision : public ISourceControlRevision, public TSharedFromThis<FSubversionSourceControlRevision, ESPMode::ThreadSafe>
{
public:
	FSubversionSourceControlRevision()
		: RevisionNumber(0)
	{
	}

	/** ISourceControlRevision interface */
	virtual bool Get( FString& InOutFilename ) const OVERRIDE;
	virtual bool GetAnnotated( TArray<FAnnotationLine>& OutLines ) const OVERRIDE;
	virtual bool GetAnnotated( FString& InOutFilename ) const OVERRIDE;
	virtual const FString& GetFilename() const OVERRIDE;
	virtual int32 GetRevisionNumber() const OVERRIDE;
	virtual const FString& GetDescription() const OVERRIDE;
	virtual const FString& GetUserName() const OVERRIDE;
	virtual const FString& GetClientSpec() const OVERRIDE;
	virtual const FString& GetAction() const OVERRIDE;
	virtual const FDateTime& GetDate() const OVERRIDE;
	virtual int32 GetCheckInIdentifier() const OVERRIDE;
	virtual int32 GetFileSize() const OVERRIDE;

public:

	/** The filename this revision refers to */
	FString Filename;

	/** The revision number */
	int32 RevisionNumber;

	/** The description of this revision */
	FString Description;

	/** The user that made the change */
	FString UserName;

	/** The action (add, edit etc.) performed at this revision */
	FString Action;

	/** The date this revision was made */
	FDateTime Date;
};
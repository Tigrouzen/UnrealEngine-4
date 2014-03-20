// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
 
#include "OnlineUserCloudInterface.h"
#include "OnlineSubsystemSteamTypes.h"
#include "OnlineAsyncTaskManagerSteam.h"
#include "OnlineSubsystemSteamPackage.h"

/** 
 *  Async task for enumerating all cloud files for a given user
 */
class FOnlineAsyncTaskSteamEnumerateUserFiles : public FOnlineAsyncTaskSteam
{
	/** UserId for file enumeration */
	FUniqueNetIdSteam UserId;

	/** Hidden on purpose */
	FOnlineAsyncTaskSteamEnumerateUserFiles() :
		FOnlineAsyncTaskSteam(NULL, k_uAPICallInvalid),
		UserId(0)
	{
	}

public:
	FOnlineAsyncTaskSteamEnumerateUserFiles(class FOnlineSubsystemSteam* InSubsystem, const FUniqueNetIdSteam& InUserId) :
		FOnlineAsyncTaskSteam(InSubsystem, k_uAPICallInvalid),
		UserId(InUserId)
	{
	}

	/**
	 *	Get a human readable description of task
	 */
	virtual FString ToString() const OVERRIDE;

	/**
	 * Give the async task time to do its work
	 * Can only be called on the async task manager thread
	 */
	virtual void Tick() OVERRIDE;
	
	/**
	 *	Async task is given a chance to trigger it's delegates
	 */
	virtual void TriggerDelegates() OVERRIDE;
};

/** 
 *  Async task for reading into memory a single cloud files for a given user
 */
class FOnlineAsyncTaskSteamReadUserFile : public FOnlineAsyncTaskSteam
{
PACKAGE_SCOPE:

	/** UserId making the request */
	FUniqueNetIdSteam UserId;
	/** Filename shared */
	FString FileName;

	/** Hidden on purpose */
	FOnlineAsyncTaskSteamReadUserFile() :
		UserId(0)
	{
	}

public:

	FOnlineAsyncTaskSteamReadUserFile(class FOnlineSubsystemSteam* InSubsystem, const FUniqueNetIdSteam& InUserId, const FString& InFileName) :
		FOnlineAsyncTaskSteam(InSubsystem, k_uAPICallInvalid),
		UserId(InUserId), 
		FileName(InFileName)
	{
	}

	/**
	 *	Get a human readable description of task
	 */
	virtual FString ToString() const OVERRIDE;

	/**
	 * Give the async task time to do its work
	 * Can only be called on the async task manager thread
	 */
	virtual void Tick() OVERRIDE;
	
	/**
	 *	Async task is given a chance to trigger it's delegates
	 */
	virtual void TriggerDelegates() OVERRIDE;
};

/** 
 *  Async task for writing a single cloud file to disk for a given user
 */
class FOnlineAsyncTaskSteamWriteUserFile : public FOnlineAsyncTaskSteam
{
PACKAGE_SCOPE:

	/** Copy of the data to write */
	TArray<uint8> Contents;
	/** UserId making the request */
	FUniqueNetIdSteam UserId;
	/** File being written */
	FString FileName;

	/** Hidden on purpose */
	FOnlineAsyncTaskSteamWriteUserFile() :
		UserId(0)
	{
	}

	/**
	 * Write the specified user file to the network platform's file store
	 *
	 * @param UserId User owning the storage
	 * @param FileToWrite the name of the file to write
	 * @param FileContents the out buffer to copy the data into
	 *
	 * @return true if the calls starts successfully, false otherwise
	 */
	bool WriteUserFile(const FUniqueNetId& UserId, const FString& FileToWrite, const TArray<uint8>& Contents);

public:

	FOnlineAsyncTaskSteamWriteUserFile(class FOnlineSubsystemSteam* InSubsystem, const FUniqueNetIdSteam& InUserId, const FString& InFileName, const TArray<uint8>& InContents) :
		FOnlineAsyncTaskSteam(InSubsystem, k_uAPICallInvalid),
		Contents(InContents),
		UserId(InUserId), 
		FileName(InFileName)
	{
	}

	/**
	 *	Get a human readable description of task
	 */
	virtual FString ToString() const OVERRIDE;

	/**
	 * Give the async task time to do its work
	 * Can only be called on the async task manager thread
	 */
	virtual void Tick() OVERRIDE;
	
	/**
	 *	Async task is given a chance to trigger it's delegates
	 */
	virtual void TriggerDelegates() OVERRIDE;
};

/** 
 *  Async task for deleting a single cloud file for a given user
 */
class FOnlineAsyncTaskSteamDeleteUserFile : public FOnlineAsyncTaskSteam
{
	/** Should the file be deleted from the cloud record */
	bool bShouldCloudDelete;
	/** Should the local copy of the file be deleted */
	bool bShouldLocallyDelete;
	/** UserId making the request */
	FUniqueNetIdSteam UserId;
	/** File being deleted */
	FString FileName;

	/** Hidden on purpose */
	FOnlineAsyncTaskSteamDeleteUserFile() :
		FOnlineAsyncTaskSteam(NULL, k_uAPICallInvalid),
		bShouldCloudDelete(false),
		bShouldLocallyDelete(false),
		UserId(0)
	{
	}

public:
	FOnlineAsyncTaskSteamDeleteUserFile(class FOnlineSubsystemSteam* InSubsystem, const FUniqueNetIdSteam& InUserId, const FString& InFileName, bool bInShouldCloudDelete, bool bInShouldLocallyDelete) :
		FOnlineAsyncTaskSteam(InSubsystem, k_uAPICallInvalid),
		bShouldCloudDelete(bInShouldCloudDelete),
		bShouldLocallyDelete(bInShouldLocallyDelete),
		UserId(InUserId), 
		FileName(InFileName)
	{
	}

	/**
	 *	Get a human readable description of task
	 */
	virtual FString ToString() const OVERRIDE;

	/**
	 * Give the async task time to do its work
	 * Can only be called on the async task manager thread
	 */
	virtual void Tick() OVERRIDE;
	
	/**
	 *	Async task is given a chance to trigger it's delegates
	 */
	virtual void TriggerDelegates() OVERRIDE;
};

/**
 * Provides access to per user cloud file storage
 */
class FOnlineUserCloudSteam : public IOnlineUserCloud
{
private:
	/** Reference to the main Steam subsystem */
	class FOnlineSubsystemSteam* SteamSubsystem;

	FOnlineUserCloudSteam() :
		SteamSubsystem(NULL)
	{
	}

PACKAGE_SCOPE:

	FOnlineUserCloudSteam(class FOnlineSubsystemSteam* InSubsystem) :
		SteamSubsystem(InSubsystem)
	{
	}

public:
	
	virtual ~FOnlineUserCloudSteam();

	// IOnlineUserCloud
	virtual bool GetFileContents(const FUniqueNetId& UserId, const FString& FileName, TArray<uint8>& FileContents) OVERRIDE;
	virtual bool ClearFiles(const FUniqueNetId& UserId) OVERRIDE;
	virtual bool ClearFile(const FUniqueNetId& UserId, const FString& FileName) OVERRIDE;
	virtual void EnumerateUserFiles(const FUniqueNetId& UserId) OVERRIDE;
	virtual void GetUserFileList(const FUniqueNetId& UserId, TArray<FCloudFileHeader>& UserFiles) OVERRIDE;
	virtual bool ReadUserFile(const FUniqueNetId& UserId, const FString& FileName) OVERRIDE;
	virtual bool WriteUserFile(const FUniqueNetId& UserId, const FString& FileName, TArray<uint8>& FileContents) OVERRIDE;
	virtual bool DeleteUserFile(const FUniqueNetId& UserId, const FString& FileName, bool bShouldCloudDelete, bool bShouldLocallyDelete) OVERRIDE;
	virtual void DumpCloudState(const FUniqueNetId& UserId) OVERRIDE;
	virtual void DumpCloudFileState(const FUniqueNetId& UserId, const FString& FileName) OVERRIDE;

};

typedef TSharedPtr<FOnlineUserCloudSteam, ESPMode::ThreadSafe> FOnlineUserCloudSteamPtr;
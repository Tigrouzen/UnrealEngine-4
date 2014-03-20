// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemUtilsPrivatePCH.h"
#include "TestTitleFileInterface.h"
#include "Online.h"

void FTestTitleFileInterface::Test()
{
	OnlineTitleFile = Online::GetTitleFileInterface(SubsystemName.Len() ? FName(*SubsystemName, FNAME_Find) : NAME_None);
	if (OnlineTitleFile.IsValid())
	{
		OnlineTitleFile->AddOnEnumerateFilesCompleteDelegate(OnEnumerateFilesCompleteDelegate);
		OnlineTitleFile->AddOnReadFileCompleteDelegate(OnReadFileCompleteDelegate);
		OnlineTitleFile->EnumerateFiles();
	}
	else
	{
		UE_LOG(LogOnline, Warning, TEXT("Failed to get title file interface for %s"), *SubsystemName);
	}
}

FTestTitleFileInterface::FTestTitleFileInterface(const FString& InSubsystemName)
	: SubsystemName(InSubsystemName)
	, OnEnumerateFilesCompleteDelegate(FOnEnumerateFilesCompleteDelegate::CreateRaw(this, &FTestTitleFileInterface::OnEnumerateFilesComplete))
	, OnReadFileCompleteDelegate(FOnReadFileCompleteDelegate::CreateRaw(this, &FTestTitleFileInterface::OnReadFileComplete))
	, NumPendingFileReads(0)
{
}

void FTestTitleFileInterface::FinishTest()
{
	UE_LOG(LogOnline, Log, TEXT("Test finished"));

	if (OnlineTitleFile.IsValid())
	{
		OnlineTitleFile->ClearOnEnumerateFilesCompleteDelegate(OnEnumerateFilesCompleteDelegate);
		OnlineTitleFile->ClearOnReadFileCompleteDelegate(OnReadFileCompleteDelegate);
	}
	delete this;
}

void FTestTitleFileInterface::OnEnumerateFilesComplete(bool bSuccess)
{
	TArray<FCloudFileHeader> Files;
	OnlineTitleFile->GetFileList(Files);
	UE_LOG(LogOnline, Log, TEXT("Found %i files"), Files.Num());

	NumPendingFileReads = Files.Num();
	if (NumPendingFileReads > 0)
	{
		for (TArray<FCloudFileHeader>::TConstIterator It(Files); It; ++It)
		{
			// kick off reads
			const FCloudFileHeader& CloudFile = *It;
			OnlineTitleFile->ReadFile(CloudFile.DLName);
		}
	}
	else
	{
		// no files to read
		FinishTest();
	}
}

void FTestTitleFileInterface::OnReadFileComplete(bool bSuccess, const FString& Filename)
{
	if (bSuccess)
	{
		UE_LOG(LogOnline, Log, TEXT("File read. file=[%s]"), *Filename);
		TArray<uint8> FileContents;
		OnlineTitleFile->GetFileContents(Filename, FileContents);
		UE_LOG(LogOnline, Log, TEXT("File length=%d. file=[%s]"), FileContents.Num(), *Filename);
		OnlineTitleFile->ClearFile(Filename);
	}
	else
	{
		UE_LOG(LogOnline, Log, TEXT("File not read. file=[%s]"), *Filename);
	}
	
	NumPendingFileReads--;
	if (NumPendingFileReads <= 0)
	{
		FinishTest();
	}
}
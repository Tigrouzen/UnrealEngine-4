// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemUtilsPrivatePCH.h"
#include "ModuleManager.h"
#include "Online.h"
#include "TestTimeInterface.h"

void FTestTimeInterface::Test(void)
{
	OnlineTime = Online::GetTimeInterface(SubsystemName.Len() ? FName(*SubsystemName, FNAME_Find) : NAME_None);
	if (OnlineTime.IsValid())
	{
		// Create and add delegate for the async call
		OnQueryServerUtcTimeCompleteDelegate = FOnQueryServerUtcTimeCompleteDelegate::CreateRaw(this, &FTestTimeInterface::OnQueryServerUtcTimeComplete);
		OnlineTime->AddOnQueryServerUtcTimeCompleteDelegate(OnQueryServerUtcTimeCompleteDelegate);
		// Kick off the async query for server time
		OnlineTime->QueryServerUtcTime();
	}
	else
	{
		UE_LOG(LogOnline, Warning,
			TEXT("Failed to get server time interface for %s"), *SubsystemName);

		delete this;
	}
}

void FTestTimeInterface::OnQueryServerUtcTimeComplete(bool bWasSuccessful, const FString& DateTimeStr, const FString& Error)
{
	OnlineTime->ClearOnQueryServerUtcTimeCompleteDelegate(OnQueryServerUtcTimeCompleteDelegate);

	if (bWasSuccessful)
	{
		UE_LOG(LogOnline, Log, TEXT("Successful query for server time. Result=[%s]"), *DateTimeStr);
	}
	else
	{
		UE_LOG(LogOnline, Log, TEXT("Failed to query server time. Error=[%s]"), *Error);
	}

	// done with the test
	delete this;
}
